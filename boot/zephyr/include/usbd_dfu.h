/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BOOT_USBD_DFU_H
#define BOOT_USBD_DFU_H

#include <zephyr/kernel.h>

struct usbd_context;

/**
 * @brief Build and enable MCUboot's USB device stack for DFU.
 *
 * Initializes MCUboot's own USB device context with the DFU class on first
 * call, then enables the device. Safe to call more than once.
 *
 * @return 0 on success, negative errno code on failure.
 */
int boot_usb_dfu_enable(void);

/**
 * @brief Wait for a DFU download to complete.
 *
 * Blocks until the host has finished downloading an image or @p delay expires.
 * On completion the downloaded image is marked for upgrade, so that the swap
 * is performed on the next boot.
 *
 * @param delay How long to wait; K_FOREVER to wait indefinitely.
 */
void boot_usb_dfu_wait(k_timeout_t delay);

/**
 * @brief Get MCUboot's USB device context used for DFU.
 *
 * Valid to call before boot_usb_dfu_enable(); the context is statically
 * defined. Used to disable the USB device before chain-loading the
 * application.
 *
 * @return Pointer to the USB device context.
 */
struct usbd_context *boot_usb_dfu_get_context(void);

#endif /* BOOT_USBD_DFU_H */
