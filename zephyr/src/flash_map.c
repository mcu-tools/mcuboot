/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <zephyr.h>
#include <misc/printk.h>
#include <flash.h>

#include <flash_map/flash_map.h>
#include <hal/hal_flash.h>
#include <sysflash/sysflash.h>

extern struct device *boot_flash_device;

/*
 * The flash area describes essentially the partition table of the
 * flash.  In this case, it starts with FLASH_AREA_IMAGE_0.
 */
static const struct flash_area part_map[] = {
	{
		.fa_id = FLASH_AREA_IMAGE_0,
		.fa_off = 0x20000,
		.fa_size = 0x20000,
	},
	{
		.fa_id = FLASH_AREA_IMAGE_1,
		.fa_off = 0x40000,
		.fa_size = 0x20000,
	},
	{
		.fa_id = FLASH_AREA_IMAGE_SCRATCH,
		.fa_off = 0x60000,
		.fa_size = 0x20000,
	},
};

/*
 * The K64F has a simple 1MB of uniform 4KB sectors.  Initially, we'll
 * use the same partition layout as the Carbon board to make
 * development easier.
 */

/*
 * `open` a flash area.  The `area` in this case is not the individual
 * sectors, but describes the particular flash area in question.
 */
int flash_area_open(uint8_t id, const struct flash_area **area)
{
	int i;
	printk("%s: area %d\n", __func__, id);

	for (i = 0; i < ARRAY_SIZE(part_map); i++) {
		if (id == part_map[i].fa_id)
			break;
	}
	if (i == ARRAY_SIZE(part_map))
		return -1;

	*area = &part_map[i];
	return 0;
}

/*
 * Nothing to do on close.
 */
void flash_area_close(const struct flash_area *area)
{
}

int flash_area_read(const struct flash_area *area, uint32_t off, void *dst,
		    uint32_t len)
{
	// printk("%s: area=%d, off=%x, len=%x\n", __func__, area->fa_id, off, len);
	return flash_read(boot_flash_device, area->fa_off + off, dst, len);
}

int flash_area_write(const struct flash_area *area, uint32_t off, const void *src,
		     uint32_t len)
{
	printk("%s: area=%d, off=%x, len=%x\n", __func__, area->fa_id, off, len);
	return flash_write(boot_flash_device, area->fa_off + off, src, len);
}

int flash_area_erase(const struct flash_area *area, uint32_t off, uint32_t len)
{
	printk("%s: area=%d, off=%x, len=%x\n", __func__, area->fa_id, off, len);
	return flash_erase(boot_flash_device, area->fa_off + off, len);
}

uint8_t flash_area_align(const struct flash_area *area)
{
	return hal_flash_align(area->fa_id);
}

/*
 * This depends on the mappings defined in sysflash.h, and assumes
 * that slot 0, slot 1, and the scratch area area contiguous.
 */
int flash_area_id_from_image_slot(int slot)
{
	return slot + FLASH_AREA_IMAGE_0;
}

/*
 * Lookup the sector map for a given flash area.  This should fill in
 * `ret` with all of the sectors in the area.  `*cnt` will be set to
 * the storage at `ret` and should be set to the final number of
 * sectors in this area.
 */
int flash_area_to_sectors(int idx, int *cnt, struct flash_area *ret)
{
	uint32_t off;

	printk("%s: lookup area %d\n", __func__, idx);
	/*
	 * This simple layout has uniform slots, so just fill in the
	 * right one.
	 */
	if (idx < FLASH_AREA_IMAGE_0 || idx > FLASH_AREA_IMAGE_SCRATCH)
		return -1;

	if (*cnt < 1)
		return -1;

	off = (idx - FLASH_AREA_IMAGE_0 + 1) * 0x20000;

	ret->fa_id = idx;
	ret->fa_device_id = 0;
	ret->pad16 = 0;
	ret->fa_off = off;
	ret->fa_size = 0x20000;

	return 0;
}
