/* Run the boot image. */

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bootutil/bootutil.h>
#include <bootutil/image.h>
#include "flash_map/flash_map.h"

#include "../../../boot/bootutil/src/bootutil_priv.h"

#define BOOT_LOG_LEVEL BOOT_LOG_LEVEL_ERROR
#include <bootutil/bootutil_log.h>

extern int sim_flash_erase(uint32_t offset, uint32_t size);
extern int sim_flash_read(uint32_t offset, uint8_t *dest, uint32_t size);
extern int sim_flash_write(uint32_t offset, const uint8_t *src, uint32_t size);

static jmp_buf boot_jmpbuf;
static int current_img_idx = 0;
int flash_counter;
uint8_t br_flash_dev_id = 0;
uint32_t br_image_off = 0;
uint16_t ih_hdr_size = 0;

int jumped = 0;

uint8_t sim_flash_align = 1;
uint8_t flash_area_align(const struct flash_area *area)
{
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

// In this structure we have pointers to slots that we received from the
// simulator environment. The pointers are set up by the
// flash_partition_map_init(...) function, so that the pointers point at the
// slots currently in use.
// We have 3 pointers: for the current primary, upgrade and scratch area slots.
struct area_pointer_desc {
    struct area* slots[3];
    uint32_t num_slots;
};

static struct area_pointer_desc current_flash_areas = {0};

void *(*mbedtls_calloc)(size_t n, size_t size);
void (*mbedtls_free)(void *ptr);

void save_rsp_fields(struct boot_rsp* rsp)
{
    br_flash_dev_id = rsp->br_flash_dev_id;
    br_image_off = rsp->br_image_off;
    ih_hdr_size = rsp->br_hdr.ih_hdr_size;
}

int invoke_boot_go(int boot_image_count, struct area_desc *adesc)
{
    int res;
    struct boot_rsp rsp;

    memset(&rsp, 0, sizeof(rsp));
    save_rsp_fields(&rsp);

    mbedtls_calloc = calloc;
    mbedtls_free = free;

    flash_areas = adesc;
    if (setjmp(boot_jmpbuf) == 0) {
        res = boot_go(boot_image_count, &rsp);
        flash_areas = NULL;
        /* printf("boot_go off: %d (0x%08x)\n", res, rsp.br_image_off); */
        save_rsp_fields(&rsp);
        return res;
    } else {
        flash_areas = NULL;
        return -0x13579;
    }
}

int hal_flash_read(uint8_t flash_id, uint32_t address, void *dst,
                   uint32_t num_bytes)
{
    // printf("hal_flash_read: %d, 0x%08x (0x%x)\n",
    //        flash_id, address, num_bytes);
    return sim_flash_read(address, dst, num_bytes);
}

int hal_flash_write(uint8_t flash_id, uint32_t address,
                    const void *src, int32_t num_bytes)
{
    // printf("hal_flash_write: 0x%08x (0x%x)\n", address, num_bytes);
    // fflush(stdout);

    // if flash_counter is not positive, we only want to decrease
    // it in case of the first image pair. So when we do the fail test
    // from the simulator, only the 1st image will be tested.
    if (flash_counter>0 ||
        current_img_idx == 0)
    {
        --flash_counter;
    }
    if (flash_counter == 0) {
        jumped++;
        longjmp(boot_jmpbuf, 1);
    }
    return sim_flash_write(address, src, num_bytes);
}

int hal_flash_erase(uint8_t flash_id, uint32_t address,
                    uint32_t num_bytes)
{
    // printf("hal_flash_erase: 0x%08x, (0x%x)\n", address, num_bytes);
    // fflush(stdout);

    // if flash_counter is not positive, we only want to decrease
    // it in case of the first image pair. So when we do the fail test
    // from the simulator, only the 1st image will be tested.
    if (flash_counter>0 ||
        current_img_idx == 0)
    {
        --flash_counter;
    }
    if (flash_counter == 0) {
        jumped++;
        longjmp(boot_jmpbuf, 1);
    }
    return sim_flash_erase(address, num_bytes);
}

uint8_t hal_flash_align(uint8_t flash_id)
{
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
    int i;

    for (i = 0; i < current_flash_areas.num_slots; i++) {
        if (current_flash_areas.slots[i]->id == id)
            break;
    }
    if (i == current_flash_areas.num_slots) {
        printf("Unsupported area\n");
        abort();
    }

    /* Unsure if this is right, just returning the first area. */
    *area = &current_flash_areas.slots[i]->whole;
    return 0;
}

void flash_area_close(const struct flash_area *area)
{
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
    int i;
    struct area *slot;

    for (i = 0; i < current_flash_areas.num_slots; i++) {
        if (current_flash_areas.slots[i]->id == idx)
            break;
    }
    if (i == current_flash_areas.num_slots) {
        printf("Unsupported area\n");
        abort();
    }

    slot = current_flash_areas.slots[i];

    if (slot->num_areas > *cnt) {
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
    int i;
    struct area *slot;

    for (i = 0; i < current_flash_areas.num_slots; i++) {
        if (current_flash_areas.slots[i]->id == fa_id)
            break;
    }
    if (i == current_flash_areas.num_slots) {
        printf("Unsupported area\n");
        abort();
    }

    slot = current_flash_areas.slots[i];

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

// Set up the current_flash_areas structure.
// Iterate over the slots received from the simulation environment,
// and save the pointers of the slots that need to be worked on.
void flash_partition_map_init(uint8_t img_idx)
{
    int i;

    current_img_idx = img_idx;
    current_flash_areas.num_slots = 3;

    // For each pointer in current_flash_areas...
    for (i = 0; i < 3; ++i)
    {
        int slots_to_skip = img_idx;
        int slot_idx;

        // Go over the slots received from the simulator...
        for (slot_idx = 0; slot_idx < flash_areas->num_slots; ++slot_idx)
        {
            // Check whether we found a slot with the required area id.
            if (flash_areas->slots[slot_idx].id == i+1)
            {
                // We might not be looking for this slot. We might need to skip it
                if (slots_to_skip > 0)
                {
                    --slots_to_skip;
                } else
                {
                    // This is the slot we are looking for. Save it.
                    current_flash_areas.slots[i] = &flash_areas->slots[slot_idx];
                    break;
                }
            }
        }

        if (slot_idx == flash_areas->num_slots)
        {
            printf("Unsupported image index\n");
            abort();
        }
    }
}
