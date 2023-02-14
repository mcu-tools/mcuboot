/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "flash.h"
#include "sys/types.h"
#include "errno.h"

void flash_page_foreach(const struct device *dev, flash_page_cb cb,
			void *data)
{
	const struct flash_driver_api *api = dev->api;
	const struct flash_pages_layout *layout;
	struct flash_pages_info page_info;
	size_t block, num_blocks, page = 0, i;
	off_t off = 0;

	api->page_layout(dev, &layout, &num_blocks);

	for (block = 0; block < num_blocks; block++) {
		const struct flash_pages_layout *l = &layout[block];
		page_info.size = l->pages_size;

		for (i = 0; i < l->pages_count; i++) {
			page_info.start_offset = off;
			page_info.index = page;

			if (!cb(&page_info, data)) {
				return;
			}

			off += page_info.size;
			page++;
		}
	}
}
