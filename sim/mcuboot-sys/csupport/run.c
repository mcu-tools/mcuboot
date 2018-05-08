/* Run the boot image. */

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bootutil/bootutil.h>
#include <bootutil/image.h>
#include "flash_map/flash_map.h"

#include "../../../boot/bootutil/src/bootutil_priv.h"
#include "bootsim.h"

#ifdef MCUBOOT_SIGN_EC256
#include "../../../ext/tinycrypt/lib/include/tinycrypt/ecc_dsa.h"
#endif

#define BOOT_LOG_LEVEL BOOT_LOG_LEVEL_ERROR
#include <bootutil/bootutil_log.h>

extern int sim_flash_erase(uint32_t offset, uint32_t size);
extern int sim_flash_read(uint32_t offset, uint8_t *dest, uint32_t size);
extern int sim_flash_write(uint32_t offset, const uint8_t *src, uint32_t size);

static jmp_buf boot_jmpbuf;
int flash_counter;

int jumped = 0;
uint8_t c_asserts = 0;
uint8_t c_catch_asserts = 0;

int ecdsa256_sign_(const uint8_t *privkey, const uint8_t *hash,
                   unsigned hash_len, uint8_t *signature)
{
#ifdef MCUBOOT_SIGN_EC256
    return uECC_sign(privkey, hash, hash_len, signature, uECC_secp256r1());
#else
    (void)privkey;
    (void)hash;
    (void)hash_len;
    (void)signature;
    return 0;
#endif
}

uint8_t sim_flash_align = 1;
uint8_t flash_area_align(const struct flash_area *area)
{
    (void)area;
    return sim_flash_align;
}

struct area {
    struct flash_area whole;
    struct flash_area *areas;
    uint32_t num_areas;
    uint8_t id;
};

struct area_desc {
    struct area slots[16];
    uint32_t num_slots;
};

static struct area_desc *flash_areas;

void *(*mbedtls_calloc)(size_t n, size_t size);
void (*mbedtls_free)(void *ptr);

int invoke_boot_go(struct area_desc *adesc)
{
    int res;
    struct boot_rsp rsp;

    mbedtls_calloc = calloc;
    mbedtls_free = free;

    flash_areas = adesc;
    if (setjmp(boot_jmpbuf) == 0) {
        res = boot_go(&rsp);
        flash_areas = NULL;
        /* printf("boot_go off: %d (0x%08x)\n", res, rsp.br_image_off); */
        return res;
    } else {
        flash_areas = NULL;
        return -0x13579;
    }
}

int hal_flash_read(uint8_t flash_id, uint32_t address, void *dst,
                   uint32_t num_bytes)
{
    (void)flash_id;
    // printf("hal_flash_read: %d, 0x%08x (0x%x)\n",
    //        flash_id, address, num_bytes);
    return sim_flash_read(address, dst, num_bytes);
}

int hal_flash_write(uint8_t flash_id, uint32_t address,
                    const void *src, int32_t num_bytes)
{
    (void)flash_id;
    // printf("hal_flash_write: 0x%08x (0x%x)\n", address, num_bytes);
    // fflush(stdout);
    if (--flash_counter == 0) {
        jumped++;
        longjmp(boot_jmpbuf, 1);
    }
    return sim_flash_write(address, src, num_bytes);
}

int hal_flash_erase(uint8_t flash_id, uint32_t address,
                    uint32_t num_bytes)
{
    (void)flash_id;
    // printf("hal_flash_erase: 0x%08x, (0x%x)\n", address, num_bytes);
    // fflush(stdout);
    if (--flash_counter == 0) {
        jumped++;
        longjmp(boot_jmpbuf, 1);
    }
    return sim_flash_erase(address, num_bytes);
}

uint8_t hal_flash_align(uint8_t flash_id)
{
    (void)flash_id;
    return sim_flash_align;
}

void *os_malloc(size_t size)
{
    // printf("os_malloc 0x%x bytes\n", size);
    return malloc(size);
}

int flash_area_id_from_image_slot(int slot)
{
    return slot + 1;
}

int flash_area_open(uint8_t id, const struct flash_area **area)
{
    uint32_t i;

    for (i = 0; i < flash_areas->num_slots; i++) {
        if (flash_areas->slots[i].id == id)
            break;
    }
    if (i == flash_areas->num_slots) {
        printf("Unsupported area\n");
        abort();
    }

    /* Unsure if this is right, just returning the first area. */
    *area = &flash_areas->slots[i].whole;
    return 0;
}

void flash_area_close(const struct flash_area *area)
{
    (void)area;
}

/*
 * Read/write/erase. Offset is relative from beginning of flash area.
 */
int flash_area_read(const struct flash_area *area, uint32_t off, void *dst,
                    uint32_t len)
{
    BOOT_LOG_DBG("%s: area=%d, off=%x, len=%x",
                 __func__, area->fa_id, off, len);
    return hal_flash_read(area->fa_id,
                          area->fa_off + off,
                          dst, len);
}

int flash_area_write(const struct flash_area *area, uint32_t off, const void *src,
                     uint32_t len)
{
    BOOT_LOG_DBG("%s: area=%d, off=%x, len=%x", __func__,
                 area->fa_id, off, len);
    return hal_flash_write(area->fa_id,
                           area->fa_off + off,
                           src, len);
}

int flash_area_erase(const struct flash_area *area, uint32_t off, uint32_t len)
{
    BOOT_LOG_DBG("%s: area=%d, off=%x, len=%x", __func__,
                 area->fa_id, off, len);
    return hal_flash_erase(area->fa_id,
                           area->fa_off + off,
                           len);
}

int flash_area_to_sectors(int idx, int *cnt, struct flash_area *ret)
{
    uint32_t i;
    struct area *slot;

    for (i = 0; i < flash_areas->num_slots; i++) {
        if (flash_areas->slots[i].id == idx)
            break;
    }
    if (i == flash_areas->num_slots) {
        printf("Unsupported area\n");
        abort();
    }

    slot = &flash_areas->slots[i];

    if (slot->num_areas > (uint32_t)*cnt) {
        printf("Too many areas in slot\n");
        abort();
    }

    *cnt = slot->num_areas;
    memcpy(ret, slot->areas, slot->num_areas * sizeof(struct flash_area));

    return 0;
}

int flash_area_get_sectors(int fa_id, uint32_t *count,
                           struct flash_sector *sectors)
{
    uint32_t i;
    struct area *slot;

    for (i = 0; i < flash_areas->num_slots; i++) {
        if (flash_areas->slots[i].id == fa_id)
            break;
    }
    if (i == flash_areas->num_slots) {
        printf("Unsupported area\n");
        abort();
    }

    slot = &flash_areas->slots[i];

    if (slot->num_areas > *count) {
        printf("Too many areas in slot\n");
        abort();
    }

    for (i = 0; i < slot->num_areas; i++) {
        sectors[i].fs_off = slot->areas[i].fa_off -
            slot->whole.fa_off;
        sectors[i].fs_size = slot->areas[i].fa_size;
    }
    *count = slot->num_areas;

    return 0;
}

void sim_assert(int x, const char *assertion, const char *file, unsigned int line, const char *function)
{
    if (!(x)) {
        if (c_catch_asserts) {
            c_asserts++;
        } else {
            BOOT_LOG_ERR("%s:%d: %s: Assertion `%s' failed.", file, line, function, assertion);

            /* NOTE: if the assert below is triggered, the place where it was originally
             * asserted is printed by the message above...
             */
            assert(x);
        }
    }
}
