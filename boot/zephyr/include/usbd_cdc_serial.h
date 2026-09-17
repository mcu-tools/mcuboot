/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2026 Tolt Technologies
 */

#ifndef BOOT_USBD_CDC_SERIAL_H
#define BOOT_USBD_CDC_SERIAL_H

#include <zephyr/kernel.h>

struct usbd_context;

/**
 * @brief Build MCUboot's USB device stack for CDC ACM serial recovery.
 *
 * Adds the descriptors and the CDC ACM class to MCUboot's own USB device
 * context and initializes it. The caller is responsible for enabling the
 * device afterwards with usbd_enable(). Called only when serial recovery is
 * actually entered, so that a normal boot leaves USB untouched.
 *
 * @return 0 on success, negative errno code on failure.
 */
int boot_usb_cdc_serial_init(void);

/**
 * @brief Get MCUboot's USB device context used for CDC ACM serial recovery.
 *
 * Valid to call before boot_usb_cdc_serial_init(); the context is statically
 * defined. Used to enable and, before chain-loading the application, disable
 * the USB device.
 *
 * @return Pointer to the USB device context.
 */
struct usbd_context *boot_usb_cdc_serial_get_context(void);

/**
 * @brief Given once the USB host has opened the CDC ACM port.
 *
 * Signalled from the USB message callback on a control line state change, so
 * that serial recovery does not start reading before a host is listening.
 */
extern struct k_sem boot_cdc_acm_ready;

#endif /* BOOT_USBD_CDC_SERIAL_H */
