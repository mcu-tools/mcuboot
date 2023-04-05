/* Run the boot image. */

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bootutil/bootutil.h>
#include <bootutil/image.h>

#include <flash_map_backend/flash_map_backend.h>

#include "../../../boot/bootutil/src/bootutil_priv.h"
#include "bootsim.h"

#ifdef MCUBOOT_ENCRYPT_RSA
#include "mbedtls/rsa.h"
#include "mbedtls/asn1.h"
#endif

#ifdef MCUBOOT_ENCRYPT_KW
#include "mbedtls/nist_kw.h"
#endif

#define BOOT_LOG_LEVEL BOOT_LOG_LEVEL_ERROR
#include <bootutil/bootutil_log.h>
#include "bootutil/crypto/common.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct area_desc;
extern struct area_desc *sim_get_flash_areas(void);
extern void sim_set_flash_areas(struct area_desc *areas);
extern void sim_reset_flash_areas(void);

struct sim_context;
extern struct sim_context *sim_get_context(void);
extern void sim_set_context(struct sim_context *ctx);
extern void sim_reset_context(void);

extern int sim_flash_erase(uint8_t flash_id, uint32_t offset, uint32_t size);
extern int sim_flash_read(uint8_t flash_id, uint32_t offset, uint8_t *dest,
        uint32_t size);
extern int sim_flash_write(uint8_t flash_id, uint32_t offset, const uint8_t *src,
        uint32_t size);
extern uint32_t sim_flash_align(uint8_t flash_id);
extern uint8_t sim_flash_erased_val(uint8_t flash_id);

struct sim_context {
    int flash_counter;
    int jumped;
    uint8_t c_asserts;
    uint8_t c_catch_asserts;
    jmp_buf boot_jmpbuf;
};

#ifdef MCUBOOT_ENCRYPT_RSA
static int
parse_pubkey(mbedtls_rsa_context *ctx, uint8_t **p, uint8_t *end)
{
    int rc;
    size_t len;

    if ((rc = mbedtls_asn1_get_tag(p, end, &len,
                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return -1;
    }

    if (*p + len != end) {
        return -2;
    }

    if ((rc = mbedtls_asn1_get_tag(p, end, &len,
                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return -3;
    }

    *p += len;

    if ((rc = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_BIT_STRING)) != 0) {
        return -4;
    }

    if (**p != MBEDTLS_ASN1_PRIMITIVE) {
        return -5;
    }

    *p += 1;

    if ((rc = mbedtls_asn1_get_tag(p, end, &len,
                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return -6;
    }

    if (mbedtls_asn1_get_mpi(p, end, &ctx->MBEDTLS_CONTEXT_MEMBER(N)) != 0) {
        return -7;
    }

    if (mbedtls_asn1_get_mpi(p, end, &ctx->MBEDTLS_CONTEXT_MEMBER(E)) != 0) {
        return -8;
    }

    ctx->MBEDTLS_CONTEXT_MEMBER(len) = mbedtls_mpi_size(&ctx->MBEDTLS_CONTEXT_MEMBER(N));

    if (*p != end) {
        return -9;
    }

    if (mbedtls_rsa_check_pubkey(ctx) != 0) {
        return -10;
    }

    return 0;
}

static int
fake_rng(void *p_rng, unsigned char *output, size_t len)
{
    size_t i;

    (void)p_rng;
    for (i = 0; i < len; i++) {
        output[i] = (char)i;
    }

    return 0;
}
#endif

int mbedtls_platform_set_calloc_free(void * (*calloc_func)(size_t, size_t),
                                     void (*free_func)(void *));

int rsa_oaep_encrypt_(const uint8_t *pubkey, unsigned pubkey_len,
                      const uint8_t *seckey, unsigned seckey_len,
                      uint8_t *encbuf)
{
#ifdef MCUBOOT_ENCRYPT_RSA
    mbedtls_rsa_context ctx;
    uint8_t *cp;
    uint8_t *cpend;
    int rc;

    mbedtls_platform_set_calloc_free(calloc, free);

#if MBEDTLS_VERSION_NUMBER >= 0x03000000
    mbedtls_rsa_init(&ctx);
    mbedtls_rsa_set_padding(&ctx, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);
#else
    mbedtls_rsa_init(&ctx, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);
#endif

    cp = (uint8_t *)pubkey;
    cpend = cp + pubkey_len;

    rc = parse_pubkey(&ctx, &cp, cpend);
    if (rc) {
        goto done;
    }

#if MBEDTLS_VERSION_NUMBER >= 0x03000000
    rc = mbedtls_rsa_rsaes_oaep_encrypt(&ctx, fake_rng, NULL,
            NULL, 0, seckey_len, seckey, encbuf);
#else
    rc = mbedtls_rsa_rsaes_oaep_encrypt(&ctx, fake_rng, NULL, MBEDTLS_RSA_PUBLIC,
            NULL, 0, seckey_len, seckey, encbuf);
#endif
    if (rc) {
        goto done;
    }

done:
    mbedtls_rsa_free(&ctx);
    return rc;

#else
    (void)pubkey;
    (void)pubkey_len;
    (void)seckey;
    (void)seckey_len;
    (void)encbuf;
    return 0;
#endif
}

int kw_encrypt_(const uint8_t *kek, const uint8_t *seckey, uint8_t *encbuf)
{
#ifdef MCUBOOT_ENCRYPT_KW
#ifdef MCUBOOT_AES_256
    int key_len = 256;
    int out_size = 40;
    int in_len = 32;
#else
    int key_len = 128;
    int out_size = 24;
    int in_len = 16;
#endif
    mbedtls_nist_kw_context kw;
    size_t olen;
    int rc;

    mbedtls_platform_set_calloc_free(calloc, free);

    mbedtls_nist_kw_init(&kw);

    rc = mbedtls_nist_kw_setkey(&kw, MBEDTLS_CIPHER_ID_AES, kek, key_len, 1);
    if (rc) {
        goto done;
    }

    rc = mbedtls_nist_kw_wrap(&kw, MBEDTLS_KW_MODE_KW, seckey, in_len, encbuf,
            &olen, out_size);

done:
    mbedtls_nist_kw_free(&kw);
    return rc;

#else
    (void)kek;
    (void)seckey;
    (void)encbuf;
    return 0;
#endif
}

uint32_t flash_area_align(const struct flash_area *area)
{
    return sim_flash_align(area->fa_device_id);
}

uint8_t flash_area_erased_val(const struct flash_area *area)
{
    return sim_flash_erased_val(area->fa_device_id);
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

int invoke_boot_go(struct sim_context *ctx, struct area_desc *adesc,
                   struct boot_rsp *rsp, int image_id)
{
    int res;
    struct boot_loader_state *state;

#if defined(MCUBOOT_SIGN_RSA) || \
    (defined(MCUBOOT_SIGN_EC256) && defined(MCUBOOT_USE_MBED_TLS)) ||\
    (defined(MCUBOOT_ENCRYPT_EC256) && defined(MCUBOOT_USE_MBED_TLS)) ||\
    (defined(MCUBOOT_ENCRYPT_X25519) && defined(MCUBOOT_USE_MBED_TLS))
    mbedtls_platform_set_calloc_free(calloc, free);
#endif

    state = malloc(sizeof(struct boot_loader_state));

    sim_set_flash_areas(adesc);
    sim_set_context(ctx);

    if (setjmp(ctx->boot_jmpbuf) == 0) {
        boot_state_clear(state);

#if BOOT_IMAGE_NUMBER > 1
        if (image_id >= 0) {
            memset(state->img_mask, 1, sizeof(state->img_mask));
            state->img_mask[image_id] = 0;
        }
#else
        (void) image_id;
#endif /* BOOT_IMAGE_NUMBER > 1 */

#if defined(MCUBOOT_RAM_LOAD)
        res = context_boot_go_ram(state, rsp);
#else
        res = context_boot_go_flash(state, rsp);
#endif
        sim_reset_flash_areas();
        sim_reset_context();
        free(state);
        /* printf("boot_go off: %d (0x%08x)\n", res, rsp.br_image_off); */
        return res;
    } else {
        sim_reset_flash_areas();
        sim_reset_context();
        free(state);
        return -0x13579;
    }
}

void *os_malloc(size_t size)
{
    // printf("os_malloc 0x%x bytes\n", size);
    return malloc(size);
}

int flash_area_id_from_multi_image_slot(int image_index, int slot)
{
    switch (slot) {
    case 0: return FLASH_AREA_IMAGE_PRIMARY(image_index);
    case 1: return FLASH_AREA_IMAGE_SECONDARY(image_index);
    case 2: return FLASH_AREA_IMAGE_SCRATCH;
    }

    printf("Image flash area ID not found\n");
    return -1; /* flash_area_open will fail on that */
}

int flash_area_open(uint8_t id, const struct flash_area **area)
{
    uint32_t i;
    struct area_desc *flash_areas;

    flash_areas = sim_get_flash_areas();
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
    BOOT_LOG_SIM("%s: area=%d, off=%x, len=%x",
                 __func__, area->fa_id, off, len);
    return sim_flash_read(area->fa_device_id, area->fa_off + off, dst, len);
}

int flash_area_write(const struct flash_area *area, uint32_t off, const void *src,
                     uint32_t len)
{
    BOOT_LOG_SIM("%s: area=%d, off=%x, len=%x", __func__,
                 area->fa_id, off, len);
    struct sim_context *ctx = sim_get_context();
    if (--(ctx->flash_counter) == 0) {
        ctx->jumped++;
        longjmp(ctx->boot_jmpbuf, 1);
    }
    return sim_flash_write(area->fa_device_id, area->fa_off + off, src, len);
}

int flash_area_erase(const struct flash_area *area, uint32_t off, uint32_t len)
{
    BOOT_LOG_SIM("%s: area=%d, off=%x, len=%x", __func__,
                 area->fa_id, off, len);
    struct sim_context *ctx = sim_get_context();
    if (--(ctx->flash_counter) == 0) {
        ctx->jumped++;
        longjmp(ctx->boot_jmpbuf, 1);
    }
    return sim_flash_erase(area->fa_device_id, area->fa_off + off, len);
}

int flash_area_to_sectors(int idx, int *cnt, struct flash_area *ret)
{
    uint32_t i;
    struct area *slot;
    struct area_desc *flash_areas;

    flash_areas = sim_get_flash_areas();
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
    struct area_desc *flash_areas;

    flash_areas = sim_get_flash_areas();
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

int flash_area_id_to_multi_image_slot(int image_index, int area_id)
{
    if (area_id == FLASH_AREA_IMAGE_PRIMARY(image_index)) {
        return 0;
    }
    if (area_id == FLASH_AREA_IMAGE_SECONDARY(image_index)) {
        return 1;
    }

    printf("Unsupported image area ID\n");
    abort();
}

int flash_area_id_from_image_slot(int slot) {
    /* For single image cases, just use the first image. */
    return flash_area_id_from_multi_image_slot(0, slot);
}

int flash_area_sector_from_off(uint32_t off, struct flash_sector *sector)
{
    uint32_t i, sec_off, sec_size;
    struct area *slot;
    struct area_desc *flash_areas;

    flash_areas = sim_get_flash_areas();
    for (i = 0; i < flash_areas->num_slots; i++) {
        if (flash_areas->slots[i].id == FLASH_AREA_ID(image_0))
            break;
    }

    if (i == flash_areas->num_slots) {
        printf("Unsupported area\n");
        abort();
    }

    slot = &flash_areas->slots[i];

    for (i = 0; i < slot->num_areas; i++) {
        sec_off = slot->areas[i].fa_off - slot->whole.fa_off;
        sec_size = slot->areas[i].fa_size;

        if (off >= sec_off && off < (sec_off + sec_size)) {
            sector->fs_off = sec_off;
            sector->fs_size = sec_size;
            break;
        }
    }

    return (i < slot->num_areas) ? 0 : -1;
}

int flash_area_get_sector(const struct flash_area *fa, uint32_t off,
                          struct flash_sector *sector)
{
    uint32_t i, sec_off, sec_size;
    struct area *slot;
    struct area_desc *flash_areas;

    flash_areas = sim_get_flash_areas();
    for (i = 0; i < flash_areas->num_slots; i++) {
        if (&flash_areas->slots[i].whole == fa)
            break;
    }

    if (i == flash_areas->num_slots) {
        printf("Unsupported area\n");
        abort();
    }

    slot = &flash_areas->slots[i];

    for (i = 0; i < slot->num_areas; i++) {
        sec_off = slot->areas[i].fa_off - slot->whole.fa_off;
        sec_size = slot->areas[i].fa_size;

        if (off >= sec_off && off < (sec_off + sec_size)) {
            sector->fs_off = sec_off;
            sector->fs_size = sec_size;
            break;
        }
    }

    return (i < slot->num_areas) ? 0 : -1;
}

void sim_assert(int x, const char *assertion, const char *file, unsigned int line, const char *function)
{
    if (!(x)) {
        struct sim_context *ctx = sim_get_context();
        if (ctx->c_catch_asserts) {
            ctx->c_asserts++;
        } else {
            BOOT_LOG_ERR("%s:%d: %s: Assertion `%s' failed.", file, line, function, assertion);

            /* NOTE: if the assert below is triggered, the place where it was originally
             * asserted is printed by the message above...
             */
            assert(x);
        }
    }
}

uint32_t boot_max_align(void)
{
    return BOOT_MAX_ALIGN;
}

uint32_t boot_magic_sz(void)
{
    return BOOT_MAGIC_ALIGN_SIZE;
}
