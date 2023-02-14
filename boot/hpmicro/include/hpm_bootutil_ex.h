/*
 * Copyright (c) 2023 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef HPM_BOOTUTIL_EX_H
#define HPM_BOOTUTIL_EX_H

/* read image state by id */
int boot_read_image_state_by_id(int flash_area_id, uint8_t *image_ok);

#endif