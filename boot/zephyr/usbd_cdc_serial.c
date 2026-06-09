// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 Tolt Technologies
 *
 * Self-contained USB CDC ACM setup for MCUboot serial recovery.
 * Initialized lazily in boot_uart_fifo_init() only when recovery is needed.
 */

#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usbd_msg.h>

#include "bootutil/bootutil_log.h"
#include "usbd_cdc_serial.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

USBD_DEVICE_DEFINE(boot_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   CONFIG_BOOT_SERIAL_CDC_ACM_VID,
		   CONFIG_BOOT_SERIAL_CDC_ACM_PID);

USBD_DESC_LANG_DEFINE(boot_usbd_lang);
USBD_DESC_MANUFACTURER_DEFINE(boot_usbd_mfr,
			      CONFIG_BOOT_SERIAL_CDC_ACM_MANUFACTURER_STRING);
USBD_DESC_PRODUCT_DEFINE(boot_usbd_product,
			 CONFIG_BOOT_SERIAL_CDC_ACM_PRODUCT_STRING);
IF_ENABLED(CONFIG_HWINFO, (USBD_DESC_SERIAL_NUMBER_DEFINE(boot_usbd_sn)));

static const uint8_t boot_usbd_attributes =
	IS_ENABLED(CONFIG_BOOT_SERIAL_CDC_ACM_SELF_POWERED)
		? USB_SCD_SELF_POWERED : 0;

USBD_DESC_CONFIG_DEFINE(boot_usbd_fs_cfg_desc, "FS Configuration");
USBD_CONFIGURATION_DEFINE(boot_usbd_fs_config,
			  boot_usbd_attributes,
			  CONFIG_BOOT_SERIAL_CDC_ACM_MAX_POWER,
			  &boot_usbd_fs_cfg_desc);

#if USBD_SUPPORTS_HIGH_SPEED
USBD_DESC_CONFIG_DEFINE(boot_usbd_hs_cfg_desc, "HS Configuration");
USBD_CONFIGURATION_DEFINE(boot_usbd_hs_config,
			  boot_usbd_attributes,
			  CONFIG_BOOT_SERIAL_CDC_ACM_MAX_POWER,
			  &boot_usbd_hs_cfg_desc);
#endif

K_SEM_DEFINE(boot_cdc_acm_ready, 0, 1);

static void boot_usbd_msg_cb(struct usbd_context *const ctx,
			     const struct usbd_msg *const msg)
{
	ARG_UNUSED(ctx);

	if (msg->type == USBD_MSG_CDC_ACM_CONTROL_LINE_STATE) {
		k_sem_give(&boot_cdc_acm_ready);
	}
}

static int boot_usbd_register_fs(void)
{
	int err;

	err = usbd_add_configuration(&boot_usbd, USBD_SPEED_FS,
				     &boot_usbd_fs_config);
	if (err) {
		BOOT_LOG_ERR("Failed to add FS configuration: %d", err);
		return err;
	}

	err = usbd_register_class(&boot_usbd, "cdc_acm_0", USBD_SPEED_FS, 1);
	if (err) {
		BOOT_LOG_ERR("Failed to register CDC ACM class (FS): %d", err);
		return err;
	}

	err = usbd_device_set_code_triple(&boot_usbd, USBD_SPEED_FS,
					  USB_BCC_MISCELLANEOUS, 0x02, 0x01);
	if (err) {
		BOOT_LOG_ERR("Failed to set code triple (FS): %d", err);
	}

	return err;
}

#if USBD_SUPPORTS_HIGH_SPEED
static int boot_usbd_register_hs(void)
{
	int err;

	err = usbd_add_configuration(&boot_usbd, USBD_SPEED_HS,
				     &boot_usbd_hs_config);
	if (err) {
		BOOT_LOG_ERR("Failed to add HS configuration: %d", err);
		return err;
	}

	err = usbd_register_class(&boot_usbd, "cdc_acm_0", USBD_SPEED_HS, 1);
	if (err) {
		BOOT_LOG_ERR("Failed to register CDC ACM class (HS): %d", err);
		return err;
	}

	err = usbd_device_set_code_triple(&boot_usbd, USBD_SPEED_HS,
					  USB_BCC_MISCELLANEOUS, 0x02, 0x01);
	if (err) {
		BOOT_LOG_ERR("Failed to set code triple (HS): %d", err);
	}

	return err;
}
#endif

int boot_usb_cdc_serial_init(void)
{
	int err;

	err = usbd_add_descriptor(&boot_usbd, &boot_usbd_lang);
	if (err) {
		BOOT_LOG_ERR("Failed to add language descriptor: %d", err);
		return err;
	}

	err = usbd_add_descriptor(&boot_usbd, &boot_usbd_mfr);
	if (err) {
		BOOT_LOG_ERR("Failed to add manufacturer descriptor: %d", err);
		return err;
	}

	err = usbd_add_descriptor(&boot_usbd, &boot_usbd_product);
	if (err) {
		BOOT_LOG_ERR("Failed to add product descriptor: %d", err);
		return err;
	}

	IF_ENABLED(CONFIG_HWINFO, (
		err = usbd_add_descriptor(&boot_usbd, &boot_usbd_sn);
		if (err) {
			BOOT_LOG_ERR("Failed to add serial number descriptor: %d", err);
			return err;
		}
	))

#if USBD_SUPPORTS_HIGH_SPEED
	if (usbd_caps_speed(&boot_usbd) == USBD_SPEED_HS) {
		err = boot_usbd_register_hs();
		if (err) {
			BOOT_LOG_ERR("Failed to register HS configuration: %d", err);
			return err;
		}
	}
#endif

	err = boot_usbd_register_fs();
	if (err) {
		BOOT_LOG_ERR("Failed to register FS configuration: %d", err);
		return err;
	}

	usbd_self_powered(&boot_usbd,
			  boot_usbd_attributes & USB_SCD_SELF_POWERED);

	err = usbd_msg_register_cb(&boot_usbd, boot_usbd_msg_cb);
	if (err) {
		BOOT_LOG_ERR("Failed to register message callback: %d", err);
		return err;
	}

	err = usbd_init(&boot_usbd);
	if (err) {
		BOOT_LOG_ERR("Failed to initialize USB device: %d", err);
	}

	return err;
}

struct usbd_context *boot_usb_cdc_serial_get_context(void)
{
	return &boot_usbd;
}
