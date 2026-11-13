/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Simulator XIP encryption support.
 *
 * Provides:
 * - mbedTLS platform stubs (calloc/free)
 * - MCUBoot hook stubs (required by MCUBOOT_IMAGE_ACCESS_HOOKS)
 *
 * boot_decrypt_xip() is provided by the weak default in
 * xip_enc_decrypt.c (SW AES-CTR decryption).
 * The boot_image_check_hook is provided by the weak default in
 * xip_enc_validate.c (ECIES unwrap + SHA-256 hash verification).
 */

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_ENC_IMAGES_XIP) && defined(MCUBOOT_IMAGE_ACCESS_HOOKS)

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "bootutil/bootutil.h"
#include "bootutil/bootutil_public.h"
#include "bootutil/image.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/boot_hooks.h"
#include "flash_map_backend/flash_map_backend.h"
#include "xip_enc/xip_enc.h"

/*
 * mbedTLS platform stubs for asn1parse.c (calloc/free).
 * Only needed when enc-xip-ec256 is the sole encryption feature.
 */
void *mbedtls_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

void mbedtls_free(void *ptr)
{
    free(ptr);
}

/*
 * flash_device_base for the simulator.
 * Simulated flash starts at address 0 -- fap->fa_off is already the
 * absolute address within the simulated address space.
 */
int flash_device_base(uint8_t fd_id, uintptr_t *ret)
{
    (void)fd_id;
    if (ret != NULL) {
        *ret = 0;
    }
    return 0;
}

/*
 * Stub implementations for MCUBoot hooks.
 * MCUBOOT_IMAGE_ACCESS_HOOKS enables all hooks globally.
 * boot_image_check_hook is provided by xip_enc_validate.c (weak default).
 * All other hooks defer to upstream.
 */
int boot_read_image_header_hook(int img_index, int slot,
                                struct image_header *img_head)
{
    (void)img_index;
    (void)slot;
    (void)img_head;
    return BOOT_HOOK_REGULAR;
}

int boot_perform_update_hook(int img_index, struct image_header *img_head,
                             const struct flash_area *area)
{
    (void)img_index;
    (void)img_head;
    (void)area;
    return BOOT_HOOK_REGULAR;
}

int boot_copy_region_post_hook(int img_index, const struct flash_area *area,
                               size_t size)
{
    (void)img_index;
    (void)area;
    (void)size;
    return 0;
}

int boot_read_swap_state_primary_slot_hook(int image_index,
                                           struct boot_swap_state *state)
{
    (void)image_index;
    (void)state;
    return BOOT_HOOK_REGULAR;
}

#endif /* MCUBOOT_ENC_IMAGES_XIP && MCUBOOT_IMAGE_ACCESS_HOOKS */
