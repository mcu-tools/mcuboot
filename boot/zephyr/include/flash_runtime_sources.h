/*
 * Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __FLASH_RUNTIME_SOURCES_H__
#define __FLASH_RUNTIME_SOURCES_H__

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Get next flash map id.
 *
 * Implement this function to get the next flash map id. The function should
 * return true if the flash map id was successfully updated. If the reset
 * parameter is true, the function should reset the flash map id to the first
 * one.
 *
 * @param id Pointer to the flash map id.
 * @param reset If true, the function will reset the flash map id to the first
 *             one.
 * @retval true If the flash map id was successfully updated.
 */
bool flash_map_id_get_next(uint8_t *id, bool reset);

/*
 * Get current flash map id.
 *
 * Implement this function to get the current flash map id. The function should
 * return true if the flash map id was successfully read.
 *
 * @param id Pointer to the flash map id.
 * @retval true If the flash map id was successfully read.
 */
bool flash_map_id_get_current(uint8_t *id);

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_RUNTIME_SOURCES_H__ */
