/*
 * Copyright (c) 2021 Infineon Technologies AG
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cyw_20829_utils.h"

/* Cypress pdl headers */
#include "cy_pdl.h"

#include "bootutil/bootutil.h"
#include "bootutil/bootutil_log.h"
#include "flash_map_backend/flash_map_backend.h"

#include <string.h>

#include "flash_qspi.h"

#ifdef CYW20829

#define CY_GET_XIP_REMAP_ADDR(addr)     ((addr) - CY_XIP_BASE + CY_XIP_REMAP_OFFSET)
#define CY_GET_XIP_REMAP_ADDR_FIH(addr) fih_uint_encode(fih_uint_decode((addr)) - CY_XIP_BASE + CY_XIP_REMAP_OFFSET)

#define CY_GET_SRAM0_REMAP_ADDR(addr)   ((addr) - CY_SRAM0_BASE + CY_SRAM0_REMAP_OFFSET)

/* TOC2 */
#define TOC2_SIZE                 16u
#define TOC2_SIZE_IDX             0u
/* is followed by L1 Application Descriptor */
#define L1_APP_DESCR_SIZE         28u
#define L1_APP_DESCR_SIZE_IDX     0u
#define BOOTSTRAP_SRC_ADDR_IDX    1u     /* points to Non-Secure Vector Table */
#define BOOTSTRAP_DST_ADDR_IDX    2u
#define BOOTSTRAP_SIZE_IDX        3u

/* Non-Secure Vector Table */
#define NS_VECTOR_TABLE_SIZE      340u
#define NS_VECTOR_TABLE_ALIGNMENT 0x200u /* NS_VECTOR_TABLE_SIZE rounded up to the power of 2 */
#define L1_APP_STACK_POINTER_IDX  0u
#define L1_APP_RESET_HANDLER_IDX  1u

/* Valid memory address range 0x2000_4000 - 0x2002_0000 */
#define BOOTSTRAP_PROHIBITED      0x4000u
#define BOOTSTRAP_SRAM0_ADDR      (CY_SRAM0_BASE + BOOTSTRAP_PROHIBITED)
#define BOOTSTRAP_SRAM0_SIZE      (CY_SRAM0_SIZE - BOOTSTRAP_PROHIBITED)

#define Q(x) #x
#define QUOTE(x) Q(x)

/*
 * Check whether data fits into the specified memory area.
 */
static inline __attribute__((always_inline))
bool fits_into(uintptr_t data, size_t data_size,
               uintptr_t area, size_t area_size)
{
    uintptr_t end = data + data_size;
    return data >= area &&
           data_size <= area_size &&
           end <= area + area_size &&
           end >= data; /* overflow check */
}

/*
 * Check whether pointer is aligned to the specified power of 2 boundary.
 */
static inline __attribute__((always_inline))
bool is_aligned(uintptr_t ptr, uint32_t align)
{
    if ((align == 0u) || ((align & (align - 1u)) != 0u)) {
        return false;
    }
    else {
        return (ptr & (align - 1u)) == 0u;
    }
}

/*
 * Get an indexed word from the fih_uint-pointed array.
 */
static inline __attribute__((always_inline))
uint32_t fih_ptr_word(fih_uint fih_ptr, uint32_t index)
{
    return ((const uint32_t *)fih_uint_decode(fih_ptr))[index];
}

#ifdef MCUBOOT_ENC_IMAGES_XIP
/*
 * AES-CTR buffer encryption/decryption for SMIF XIP mode
 */
static int mbedtls_aes_crypt_ctr_xip(mbedtls_aes_context *ctx,
                                     size_t length,
                                     uint32_t *nc_off,
                                     uint8_t nonce_counter[BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE],
                                     uint8_t stream_block[BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE],
                                     const uint8_t *input,
                                     uint8_t *output)
{
    int rc = 0;
    uint32_t xip_addr = 0;
    uint32_t i;

    if (length % BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE != 0u) {
        return BOOT_EBADARGS;
    }

    (void)nc_off;

    while (length > 0u) {
        rc = mbedtls_aes_crypt_ecb(ctx, MBEDTLS_AES_ENCRYPT, nonce_counter, stream_block);

        if (rc != 0) {
            break;
        }

        (void)memcpy((uint8_t*)&xip_addr, nonce_counter, sizeof(xip_addr));
        xip_addr += BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE;
        (void)memcpy(nonce_counter, (uint8_t*)&xip_addr, sizeof(xip_addr));

        for (i = 0; i < BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE; i++) {
            uint8_t c = *input++;
            *output++ = c ^ stream_block[i];
        }

        length -= BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE;
    }

    return rc;
}

int
bootutil_img_encrypt(struct enc_key_data *enc_state, int image_index,
        struct image_header *hdr, const struct flash_area *fap, uint32_t off, uint32_t sz,
        uint32_t blk_off, uint8_t *buf)
{
    struct enc_key_data *enc;
    uint8_t nonce[BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE];
    uint8_t stream_block[BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE];
    int slot = 0;
    int rc = -1;
    uint32_t fa_addr = 0u;
    uintptr_t flash_base = 0u;

    rc = flash_device_base(fap->fa_device_id, &flash_base);

    if (0 == rc) {

        fa_addr = flash_base + fap->fa_off;

        (void)hdr;

        /* boot_copy_region will call boot_encrypt with sz = 0 when skipping over
        the TLVs. */
        if (sz > 0u) {

            slot = flash_area_id_to_multi_image_slot(image_index, (int)fap->fa_id);

            if (slot > 0) {
                uint8_t id_pri = FLASH_AREA_IMAGE_PRIMARY((uint32_t)image_index);
                const struct flash_area *fa_pri = NULL;

                if (flash_area_open(id_pri, &fa_pri) < 0) {
                    return BOOT_EFLASH;
                }
                else {
                    fa_addr = flash_base + fa_pri->fa_off;
                    flash_area_close(fa_pri);
                }
            }
            else if (slot < 0) {
                return -1;
            }
            else {
                /* No action required */
            }

            enc = &enc_state[slot];
            if (enc->valid == 1u) {

                off += CY_GET_XIP_REMAP_ADDR(fa_addr);

                (void)memcpy(nonce, (uint8_t*)&off, sizeof(off));
                (void)memcpy(nonce + 4u, enc->aes_iv, BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE - 4u);

                rc = mbedtls_aes_crypt_ctr_xip(&enc->aes_ctr, sz, &blk_off, nonce, stream_block, buf, buf);
            }
        }
    }

    return rc;
}
#endif

static __NO_RETURN void hang(void)
{
    FIH_PANIC;
}

/* End of cyw20829_RunAppFinish() */
extern uint8_t hsiniFppAnuR_92802wyc[];

CY_RAMFUNC_BEGIN
static __NO_RETURN __attribute__((naked))
void cyw20829_RunAppFinish(uintptr_t bootstrap_dst,
                           uintptr_t bootstrap_src,
                           uint32_t bootstrap_size)
{
    /* MCUBoot is over! The code below depends on the linker script. Assuming
     * stack is located at the very beginning, followed by code and data, and
     * heap is located at the end. cyw20829_RunAppFinish is located as low as
     * possible, ideally following the MCUBoot's Vector Table in RAM, to save
     * max space for the application (the goal is to fit below 0x20004000).
     * First we wipe SRAM from the end of cyw20829_RunAppFinish to the end of
     * heap. Then bootstrap is copied to its place. Then, after the necessary
     * setup, we wipe SRAM from the stack limit to the wiping loop itself. So
     * only 6 launcher's instructions are left. Note that the app's bootstrap
     * is copied to the area cleaned at the start, so self-destruction cannot
     * affect it. Also, we have to remap code addresses from C-bus to S-AHB.
     */
    asm volatile(
        "   cpsid i\n"                /* Disable interrupts */
        /* Wipe MCUBoot's RAM to prevent information leakage (Pt. 1) */
        "   mov   r0, #0\n"
        "   ldr   r1, =(hsiniFppAnuR_92802wyc-"  /* Should be remapped from */
                        QUOTE(CY_SRAM0_REMAP_OFFSET)"+"  /* C-bus to S-AHB! */
                        QUOTE(CY_SRAM0_BASE)")\n" /* Avoid self-destruction */
        "   ldr   r2, =__HeapLimit\n"
        "1: str   r0, [r1]\n"
        "   add   r1, #4\n"
        "   cmp   r1, r2\n"
        "   blo   1b\n"
        /* Copy most of bootstrap by doublewords */
        "   mov   r2, %0\n"
        "2: cmp   %2, #8\n"
        "   blo   3f\n"               /* Note: bootstrap_size cannot be 0 */
        "   ldmia %1!, {r0, r1}\n"
        "   stmia r2!, {r0, r1}\n"
        "   subs  %2, #8\n"
        "   beq   4f\n"               /* The whole bootstrap is copied */
        "   b     2b\n"
        /* Copy rest of bootstrap by bytes (if any) */
        "3: ldrb  r0, [%1]\n"
        "   add   %1, #1\n"
        "   strb  r0, [r2]\n"
        "   add   r2, #1\n"
        "   subs  %2, #1\n"
        "   bne   3b\n"
        "4: dmb   sy\n"
        /* Relocate Vector Table */
        "   str   %0, [%3]\n"         /* Bootstrap starts with Vector Table */
        "   str   %0, [%4]\n"         /* (%0 points to it) */
        /* Prepare stack */
        "   ldr   r0, ="QUOTE(CY_SRAM0_BASE)"\n"
        "   msr   msplim, r0\n"
        "   ldr   r0, [%0]\n"
        "   msr   msp, r0\n"
        /* Reset handler */
        "   ldr   lr, [%0, #4]\n"
        /* Wipe MCUBoot's RAM to prevent information leakage (Pt. 2) */
        "   mov   r0, #0\n"
        "   ldr   r1, =__StackLimit\n"
        "   ldr   r2, =(5f-"     /* Should be remapped from C-bus to S-AHB! */
                        QUOTE(CY_SRAM0_REMAP_OFFSET)"+"
                        QUOTE(CY_SRAM0_BASE)")\n" /* Final self-destruction */
        "   b     5f\n"               /* Skip the constant pool */
        /* Put the constant pool here (to avoid premature self-destruction) */
        "   .ltorg\n"
        "5: str   r0, [r1]\n"
        "   add   r1, #4\n"
        "   cmp   r1, r2\n"
        "   blo   5b\n"
        /* Wipe general-purpose registers just in case */
        "   Ldmdb r1, {r1-r12}\n"     /* Load GPRs from the zeroed memory */
        /* Launch bootstrap */
        "   bx    lr\n"
        "hsiniFppAnuR_92802wyc:\n"    /* End of function code */
        : /* no output */
        : /* input  %0 */ "r" (bootstrap_dst),
          /* input  %1 */ "r" (bootstrap_src),
          /* input  %2 */ "r" (bootstrap_size),
          /* input  %3 */ "r" (&MXCM33->CM33_NS_VECTOR_TABLE_BASE),
          /* input  %4 */ "r" (&SCB->VTOR)
        : /* clobbered */ "cc", "r0", "r1", "r2");
}
CY_RAMFUNC_END

CY_RAMFUNC_BEGIN
__NO_RETURN void cyw20829_RunApp(fih_uint toc2_addr, uint32_t *key, uint32_t *iv)
{
    fih_uint l1_app_descr_addr = (fih_uint)FIH_FAILURE;
    fih_uint ns_vect_tbl_addr = (fih_uint)FIH_FAILURE;

    uint32_t bootstrap_src_addr = 0u;
    uint32_t bootstrap_dst_addr = 0u;
    uint32_t bootstrap_size = 0u;

    uint32_t stack_pointer = 0u;
    uint32_t reset_handler = (uint32_t)&hang;

#ifdef MCUBOOT_ENC_IMAGES_XIP
    if (key != NULL && iv != NULL) {
        SMIF_Type *smif_device = qspi_get_device();

        Cy_SMIF_SetCryptoKey(smif_device, key);
        Cy_SMIF_SetCryptoIV(smif_device, iv);

        // TODO: Check for other Slave select ID
        if (Cy_SMIF_SetCryptoEnable(smif_device, CY_SMIF_SLAVE_SELECT_0) != CY_SMIF_SUCCESS) {
            FIH_PANIC;
        }

        Cy_SMIF_SetMode(smif_device, CY_SMIF_MEMORY);

        /* Clean up key and IV */
        (void)memset(key, 0, BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE);
        (void)memset(iv, 0, BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE);
    }
#else
    (void)key;
    (void)iv;
#endif /* MCUBOOT_ENC_IMAGES_XIP */

    /* Validate TOC2 in external memory (non-remapped) */
    if (!is_aligned((uintptr_t)fih_uint_decode(toc2_addr), 4u) ||
        !fits_into((uintptr_t)fih_uint_decode(toc2_addr), TOC2_SIZE,
                   (uintptr_t)CY_XIP_BASE, CY_XIP_SIZE) ||
        fih_ptr_word(CY_GET_XIP_REMAP_ADDR_FIH(fih_uint_decode(toc2_addr)), TOC2_SIZE_IDX) != TOC2_SIZE) {

        FIH_PANIC;
    }

    /* TOC2 is followed by L1 Application Descriptor */
    l1_app_descr_addr = CY_GET_XIP_REMAP_ADDR_FIH(fih_uint_decode(toc2_addr) + TOC2_SIZE);

    /* Validate L1 Application Descriptor in external memory */
    if (!is_aligned((uintptr_t)fih_uint_decode(l1_app_descr_addr), 4u) ||
        !fits_into((uintptr_t)fih_uint_decode(l1_app_descr_addr), L1_APP_DESCR_SIZE,
                   (uintptr_t)CY_XIP_REMAP_OFFSET, CY_XIP_SIZE) ||
        fih_ptr_word(l1_app_descr_addr, L1_APP_DESCR_SIZE_IDX) != L1_APP_DESCR_SIZE) {

        FIH_PANIC;
    }

    /* Extract L1 Application Descriptor fields */
    bootstrap_src_addr = fih_ptr_word(l1_app_descr_addr, BOOTSTRAP_SRC_ADDR_IDX);
    bootstrap_dst_addr = fih_ptr_word(l1_app_descr_addr, BOOTSTRAP_DST_ADDR_IDX);
    bootstrap_size = fih_ptr_word(l1_app_descr_addr, BOOTSTRAP_SIZE_IDX);

#ifndef NDEBUG
    /* Make sure bootstrap and launcher do not overlap. Checks the validity of
     * the linker script, the runtime check is below (BOOTSTRAP_SRAM0_ADDR).
     */
    if (fits_into((uintptr_t)bootstrap_dst_addr, 0,
                  (uintptr_t)cyw20829_RunAppFinish,
                  (uintptr_t)hsiniFppAnuR_92802wyc - (uintptr_t)cyw20829_RunAppFinish) ||
        fits_into((uintptr_t)bootstrap_dst_addr + bootstrap_size, 0,
                  (uintptr_t)cyw20829_RunAppFinish,
                  (uintptr_t)hsiniFppAnuR_92802wyc - (uintptr_t)cyw20829_RunAppFinish)) {

        FIH_PANIC;
    }
#endif /* !NDEBUG */

    /* Validate bootstrap destination in SRAM (starts with the NS Vector Table) */
    if (bootstrap_size < NS_VECTOR_TABLE_SIZE ||
        !is_aligned((uintptr_t)bootstrap_dst_addr, NS_VECTOR_TABLE_ALIGNMENT) ||
        !fits_into((uintptr_t)bootstrap_dst_addr, bootstrap_size,
                   (uintptr_t)BOOTSTRAP_SRAM0_ADDR, BOOTSTRAP_SRAM0_SIZE)) {

        FIH_PANIC;
    }

    /* Bootstrap source in external memory starts with the image of NS Vector Table) */
    ns_vect_tbl_addr = CY_GET_XIP_REMAP_ADDR_FIH(fih_uint_decode(toc2_addr) + bootstrap_src_addr);

    /* Validate bootstrap source image in external memory */
    if (!is_aligned((uintptr_t)fih_uint_decode(ns_vect_tbl_addr), 4u) ||
        !fits_into((uintptr_t)fih_uint_decode(ns_vect_tbl_addr), bootstrap_size,
                   (uintptr_t)CY_XIP_REMAP_OFFSET, CY_XIP_SIZE)) {

        FIH_PANIC;
    }

    /* Extract app's Stack Pointer from the image of NS Vector Table and validate it */
    stack_pointer = fih_ptr_word(ns_vect_tbl_addr, L1_APP_STACK_POINTER_IDX);

    if (!is_aligned((uintptr_t)stack_pointer, 8u) ||
        !fits_into((uintptr_t)stack_pointer, 0u,
                   (uintptr_t)CY_SRAM0_BASE, CY_SRAM0_SIZE)) {

        FIH_PANIC;
    }

    /* Extract app's Reset Handler from the image of NS Vector Table and validate it */
    reset_handler = fih_ptr_word(ns_vect_tbl_addr, L1_APP_RESET_HANDLER_IDX);

    if ((reset_handler & 1u) != 1u /* i.e., thumb function */ ||
        !fits_into((uintptr_t)(reset_handler & ~1u), 2u, /* should lay in the remapped SRAM */
                   (uintptr_t)CY_GET_SRAM0_REMAP_ADDR(bootstrap_dst_addr), bootstrap_size)) {

        FIH_PANIC;
    }

    /* MCUBoot is over */
    cyw20829_RunAppFinish((uintptr_t)bootstrap_dst_addr,
                          (uintptr_t)fih_uint_decode(ns_vect_tbl_addr),
                          bootstrap_size);
}
CY_RAMFUNC_END

#endif /* CYW20829 */
