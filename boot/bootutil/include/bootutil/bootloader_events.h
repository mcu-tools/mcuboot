/*
 *  Copyright (C) 2020, IP-Logix Inc.
 *
 *  SPDX-License-Identifier: Apache-2.0
 */

#ifndef H_BOOTLOADER_EVENTS_
#define H_BOOTLOADER_EVENTS_

enum bootloader_event {
	EVT_BL_START					= 0,
	EVT_BL_BOOTING_IMAGE,
	EVT_BL_ENTER_SERIAL_RECOVERY,
	EVT_BL_WAIT_FOR_DFU,
	EVT_BL_DFU_TIMEOUT,
	EVT_BL_SWAP_OP,
	EVT_BL_SWAP_SECTOR_PROGRESS,
	EVT_BL_MOVE_SECTOR_PROGRESS,
	EVT_BL_ERROR_NO_BOOTABLE_IMAGE,
	EVT_BL_ERROR_FLASH_NOT_FOUND,
	EVT_BL_ERROR_USB_ENABLE_FAILED,
	EVT_BL_ERROR_FLASH_PROTECT_FAILED,
	EVT_BL_ERROR_SWAP_PANIC,
};

struct bootloader_event_param {
	union {
		struct {
			int image_index;
			int op;
		} swap_op;
		struct {
			int image_index;
			int sector;
			int total_sectors;
		} sector_op;
		struct {
			int rc;
		} error;
	};
};

void bootloader_event(enum bootloader_event event, 
		      		  struct bootloader_event_param *param);

#endif /* H_BOOTLOADER_EVENTS_ */
