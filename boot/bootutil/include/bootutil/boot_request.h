/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2025 Nordic Semiconductor ASA
 */

#ifndef __BOOT_REQUEST_H__
#define __BOOT_REQUEST_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/** Special value, indicating that there is no preferred slot. */
#define BOOT_REQUEST_NO_PREFERRED_SLOT UINT32_MAX

/**
 * @brief Request a bootloader to confirm the specified slot of an image.
 *
 * @param[in] image  Image number.
 * @param[in] slot   Slot number.
 *
 * @return 0 if requested, negative error code otherwise.
 */
int boot_request_confirm_slot(uint8_t image, uint32_t slot);

/**
 * @brief Request a bootloader to boot the specified slot of an image.
 *
 * @param[in] image  Image number.
 * @param[in] slot  Slot number.
 *
 * @return 0 if requested, negative error code otherwise.
 */
int boot_request_set_preferred_slot(uint8_t image, uint32_t slot);

/**
 * @brief Request a bootloader to boot recovery image.
 *
 * @return 0 if requested, negative error code otherwise.
 */
int boot_request_enter_recovery(void);

/**
 * @brief Request a bootloader to boot firmware loader image.
 *
 * @return 0 if requested, negative error code otherwise.
 */
int boot_request_enter_firmware_loader(void);

/**
 * @brief Check if there is a request to confirm the specified slot of an image.
 *
 * @param[in] image  Image number.
 * @param[in] slot   Slot number.
 *
 * @return true if requested, false otherwise.
 */
bool boot_request_check_confirmed_slot(uint8_t image, uint32_t slot);

/**
 * @brief Find if there is a request to boot certain slot of the specified image.
 *
 * @param[in] image  Image number.
 *
 * @return slot number if requested, BOOT_REQUEST_NO_PREFERRED_SLOT otherwise.
 */
uint32_t boot_request_get_preferred_slot(uint8_t image);

/**
 * @brief Check if there is a request to boot recovery image.
 *
 * @return true if requested, false otherwise.
 */
bool boot_request_detect_recovery(void);

/**
 * @brief Check if there is a request to boot firmware loader image.
 *
 * @return true if requested, false otherwise.
 */
bool boot_request_detect_firmware_loader(void);

/**
 * @brief Initialize boot requests module.
 *
 * @return 0 if successful, negative error code otherwise.
 */
int boot_request_init(void);

/**
 * @brief Clear/drop all requests.
 *
 * @return 0 if successful, negative error code otherwise.
 */
int boot_request_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOOT_REQUEST_H__ */
