/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2026 Tolt Technologies
 */

#ifndef BOOT_USBD_CDC_SERIAL_H
#define BOOT_USBD_CDC_SERIAL_H

#include <zephyr/kernel.h>

int boot_usb_cdc_serial_init(void);
extern struct k_sem boot_cdc_acm_ready;

#endif /* BOOT_USBD_CDC_SERIAL_H */
