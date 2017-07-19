#include <flash_map/flash_map.h>
#include <hal/hal_flash.h>
#include <os/os_malloc.h>
#include <mbedtls/rsa.h>
#include <mbedtls/asn1.h>
#include <mbedtls/sha256.h>

#include <bootutil/bootutil.h>
#include <bootutil/image.h>

/*
 * flash_map
 */

void
flash_area_close(const struct flash_area *a)
{
    (void)a;
}

int
flash_area_open(uint8_t a, const struct flash_area **b)
{
    (void)a;
    (void)b;
    return 0;
}

int
flash_area_read(const struct flash_area *a, uint32_t b, void *c, uint32_t d)
{
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    return 0;
}

int
flash_area_write(const struct flash_area *a, uint32_t b, const void *c, uint32_t d)
{
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    return 0;
}

int
flash_area_erase(const struct flash_area *a, uint32_t b, uint32_t c)
{
    (void)a;
    (void)b;
    (void)c;
    return 0;
}

int
flash_area_to_sectors(int a, int *b, struct flash_area *c)
{
    (void)a;
    (void)b;
    (void)c;
    return 0;
}

int
flash_area_id_from_image_slot(int a)
{
    (void)a;
    return 0;
}

uint8_t
flash_area_align(const struct flash_area *a)
{
    return 8;
}

/*
 * hal_flash
 */

int
hal_flash_read(uint8_t flash_id, uint32_t address, void *dst, uint32_t num_bytes)
{
    (void)flash_id;
    (void)address;
    (void)dst;
    (void)num_bytes;
    return 0;
}

int
hal_flash_write(uint8_t flash_id, uint32_t address, const void *src, uint32_t num_bytes)
{
    (void)flash_id;
    (void)address;
    (void)src;
    (void)num_bytes;
    return 0;
}

int
hal_flash_erase_sector(uint8_t flash_id, uint32_t sector_address)
{
    (void)flash_id;
    (void)sector_address;
    return 0;
}

int
hal_flash_erase(uint8_t flash_id, uint32_t address, uint32_t num_bytes)
{
    (void)flash_id;
    (void)address;
    (void)num_bytes;
    return 0;
}

uint8_t
hal_flash_align(uint8_t flash_id)
{
    return 8;
}

int
hal_flash_init(void)
{
    return 0;
}

/*
 * os_malloc
 */

//TODO

/*
 * mbedtls
 */

void
mbedtls_rsa_free(mbedtls_rsa_context *a)
{
    (void)a;
}

int
mbedtls_rsa_public(mbedtls_rsa_context *a, const unsigned char *b, unsigned char *c)
{
    (void)a;
    (void)b;
    (void)c;
    return 0;
}

void
mbedtls_rsa_init(mbedtls_rsa_context *a, int b, int c)
{
    (void)a;
    (void)b;
    (void)c;
}

int
mbedtls_rsa_check_pubkey(const mbedtls_rsa_context *a)
{
    (void)a;
    return 0;
}

int
mbedtls_asn1_get_tag(unsigned char **a, const unsigned char *b, size_t *c, int d)
{
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    return 0;
}

int
mbedtls_asn1_get_mpi(unsigned char **a, const unsigned char *b, mbedtls_mpi *c)
{
    (void)a;
    (void)b;
    (void)c;
    return 0;
}

int
mbedtls_sha256_init(mbedtls_sha256_context *a)
{
    (void)a;
    return 0;
}

int
mbedtls_sha256_starts(mbedtls_sha256_context *a, int b)
{
    (void)a;
    (void)b;
    return 0;
}

int
mbedtls_sha256_update(mbedtls_sha256_context *a, const uint8_t *b, size_t c)
{
    (void)a;
    (void)b;
    (void)c;
    return 0;
}

void
mbedtls_sha256_finish(mbedtls_sha256_context *a, unsigned char *b)
{
    (void)a;
    (void)b;
}

size_t
mbedtls_mpi_size(const mbedtls_mpi *a)
{
    (void)a;
    return 0;
}

/*
 *
 */

void
hal_system_start(void *a)
{
    (void)a;
}

int
flash_device_base(uint8_t a, uintptr_t *b)
{
    (void)a;
    *b = 0;
    return 0;
}

int main(void)
{
    struct boot_rsp rsp;
    uintptr_t flash_base;
    int rc = 0;

    rc = boot_go(&rsp);
    if (!rc) {
        goto out;
    }

    rc = flash_device_base(rsp.br_flash_dev_id, &flash_base);
    if (!rc) {
        goto out;
    }

    hal_system_start((void *)(flash_base + rsp.br_image_off + rsp.br_hdr->ih_hdr_size));

out:
    return rc;
}
