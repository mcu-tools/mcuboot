/* Run the boot image. */

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bootutil/bootutil.h>
#include <bootutil/image.h>
#include "flash_map/flash_map.h"

#include "../../boot/bootutil/src/bootutil_priv.h"

#define BOOT_LOG_LEVEL BOOT_LOG_LEVEL_ERROR
#include <bootutil/bootutil_log.h>

extern int sim_flash_erase(void *flash, uint32_t offset, uint32_t size);
extern int sim_flash_read(void *flash, uint32_t offset, uint8_t *dest, uint32_t size);
extern int sim_flash_write(void *flash, uint32_t offset, const uint8_t *src, uint32_t size);

static void *flash_device;
static jmp_buf boot_jmpbuf;
int flash_counter;

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

int invoke_boot_go(void *flash, struct area_desc *adesc)
{
	int res;
	struct boot_rsp rsp;

	flash_device = flash;
	flash_areas = adesc;
	if (setjmp(boot_jmpbuf) == 0) {
		res = boot_go(&rsp);
		/* printf("boot_go result: %d (0x%08x)\n", res, rsp.br_image_addr); */
		return res;
	} else {
		return -0x13579;
	}
}

int hal_flash_read(uint8_t flash_id, uint32_t address, void *dst,
		   uint32_t num_bytes)
{
	// printf("hal_flash_read: %d, 0x%08x (0x%x)\n",
	//        flash_id, address, num_bytes);
	return sim_flash_read(flash_device, address, dst, num_bytes);
}

int hal_flash_write(uint8_t flash_id, uint32_t address,
		    const void *src, int32_t num_bytes)
{
	// printf("hal_flash_write: 0x%08x (0x%x)\n", address, num_bytes);
	// fflush(stdout);
	if (--flash_counter == 0) {
		jumped++;
		longjmp(boot_jmpbuf, 1);
	}
	return sim_flash_write(flash_device, address, src, num_bytes);
}

int hal_flash_erase(uint8_t flash_id, uint32_t address,
		    uint32_t num_bytes)
{
	// printf("hal_flash_erase: 0x%08x, (0x%x)\n", address, num_bytes);
	// fflush(stdout);
	if (--flash_counter == 0) {
		jumped++;
		longjmp(boot_jmpbuf, 1);
	}
	return sim_flash_erase(flash_device, address, num_bytes);
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
	struct area *slot;

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

	for (i = 0; i < flash_areas->num_slots; i++) {
		if (flash_areas->slots[i].id == idx)
			break;
	}
	if (i == flash_areas->num_slots) {
		printf("Unsupported area\n");
		abort();
	}

	slot = &flash_areas->slots[i];

	if (slot->num_areas > *cnt) {
		printf("Too many areas in slot\n");
		abort();
	}

	*cnt = slot->num_areas;
	memcpy(ret, slot->areas, slot->num_areas * sizeof(struct flash_area));

	return 0;
}


int bootutil_img_validate(struct image_header *hdr,
                          const struct flash_area *fap,
                          uint8_t *tmp_buf, uint32_t tmp_buf_sz,
                          uint8_t *seed, int seed_len, uint8_t *out_hash)
{
	if (hal_flash_read(fap->fa_id, fap->fa_off, tmp_buf, 4)) {
		printf("Flash read error\n");
		abort();
	}

	return (*((uint32_t *) tmp_buf) != 0x96f3b83c);
}
