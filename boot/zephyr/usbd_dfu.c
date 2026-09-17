/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usbd_msg.h>

#include "bootutil/bootutil_log.h"
#include "bootutil/bootutil_public.h"
#include "usbd_dfu.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

USBD_DEVICE_DEFINE(boot_dfu_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   CONFIG_BOOT_USB_DFU_VID,
		   CONFIG_BOOT_USB_DFU_PID);

USBD_DESC_LANG_DEFINE(boot_dfu_lang);
USBD_DESC_MANUFACTURER_DEFINE(boot_dfu_mfr,
			      CONFIG_BOOT_USB_DFU_MANUFACTURER_STRING);
USBD_DESC_PRODUCT_DEFINE(boot_dfu_product,
			 CONFIG_BOOT_USB_DFU_PRODUCT_STRING);
IF_ENABLED(CONFIG_HWINFO, (USBD_DESC_SERIAL_NUMBER_DEFINE(boot_dfu_sn)));

static const uint8_t boot_dfu_attributes =
	IS_ENABLED(CONFIG_BOOT_USB_DFU_SELF_POWERED) ? USB_SCD_SELF_POWERED : 0;

USBD_DESC_CONFIG_DEFINE(boot_dfu_fs_cfg_desc, "DFU FS Configuration");
USBD_CONFIGURATION_DEFINE(boot_dfu_fs_config,
			  boot_dfu_attributes,
			  CONFIG_BOOT_USB_DFU_MAX_POWER,
			  &boot_dfu_fs_cfg_desc);

#if USBD_SUPPORTS_HIGH_SPEED
USBD_DESC_CONFIG_DEFINE(boot_dfu_hs_cfg_desc, "DFU HS Configuration");
USBD_CONFIGURATION_DEFINE(boot_dfu_hs_config,
			  boot_dfu_attributes,
			  CONFIG_BOOT_USB_DFU_MAX_POWER,
			  &boot_dfu_hs_cfg_desc);
#endif

K_SEM_DEFINE(boot_dfu_complete, 0, 1);

static bool boot_dfu_initialized;

static void boot_dfu_msg_cb(struct usbd_context *const ctx,
			    const struct usbd_msg *const msg)
{
	ARG_UNUSED(ctx);

	if (msg->type == USBD_MSG_DFU_DOWNLOAD_COMPLETED) {
		k_sem_give(&boot_dfu_complete);
	}
}

static int boot_dfu_register_fs(void)
{
	int err;

	err = usbd_add_configuration(&boot_dfu_usbd, USBD_SPEED_FS,
				     &boot_dfu_fs_config);
	if (err) {
		BOOT_LOG_ERR("Failed to add DFU FS configuration: %d", err);
		return err;
	}

	err = usbd_register_class(&boot_dfu_usbd, "dfu_dfu", USBD_SPEED_FS, 1);
	if (err) {
		BOOT_LOG_ERR("Failed to register DFU class (FS): %d", err);
		return err;
	}

	err = usbd_device_set_code_triple(&boot_dfu_usbd, USBD_SPEED_FS, 0, 0, 0);
	if (err) {
		BOOT_LOG_ERR("Failed to set DFU code triple (FS): %d", err);
	}

	return err;
}

#if USBD_SUPPORTS_HIGH_SPEED
static int boot_dfu_register_hs(void)
{
	int err;

	err = usbd_add_configuration(&boot_dfu_usbd, USBD_SPEED_HS,
				     &boot_dfu_hs_config);
	if (err) {
		BOOT_LOG_ERR("Failed to add DFU HS configuration: %d", err);
		return err;
	}

	err = usbd_register_class(&boot_dfu_usbd, "dfu_dfu", USBD_SPEED_HS, 1);
	if (err) {
		BOOT_LOG_ERR("Failed to register DFU class (HS): %d", err);
		return err;
	}

	err = usbd_device_set_code_triple(&boot_dfu_usbd, USBD_SPEED_HS, 0, 0, 0);
	if (err) {
		BOOT_LOG_ERR("Failed to set DFU code triple (HS): %d", err);
	}

	return err;
}
#endif

static int boot_dfu_init(void)
{
	int err;

	err = usbd_add_descriptor(&boot_dfu_usbd, &boot_dfu_lang);
	if (err) {
		BOOT_LOG_ERR("Failed to add DFU language descriptor: %d", err);
		return err;
	}

	err = usbd_add_descriptor(&boot_dfu_usbd, &boot_dfu_mfr);
	if (err) {
		BOOT_LOG_ERR("Failed to add DFU manufacturer descriptor: %d", err);
		return err;
	}

	err = usbd_add_descriptor(&boot_dfu_usbd, &boot_dfu_product);
	if (err) {
		BOOT_LOG_ERR("Failed to add DFU product descriptor: %d", err);
		return err;
	}

	IF_ENABLED(CONFIG_HWINFO, (
		err = usbd_add_descriptor(&boot_dfu_usbd, &boot_dfu_sn);
		if (err) {
			BOOT_LOG_ERR("Failed to add DFU serial number descriptor: %d", err);
			return err;
		}
	))

#if USBD_SUPPORTS_HIGH_SPEED
	if (usbd_caps_speed(&boot_dfu_usbd) == USBD_SPEED_HS) {
		err = boot_dfu_register_hs();
		if (err) {
			return err;
		}
	}
#endif

	err = boot_dfu_register_fs();
	if (err) {
		return err;
	}

	usbd_self_powered(&boot_dfu_usbd,
			  boot_dfu_attributes & USB_SCD_SELF_POWERED);

	err = usbd_msg_register_cb(&boot_dfu_usbd, boot_dfu_msg_cb);
	if (err) {
		BOOT_LOG_ERR("Failed to register DFU message callback: %d", err);
		return err;
	}

	err = usbd_init(&boot_dfu_usbd);
	if (err) {
		BOOT_LOG_ERR("Failed to initialize USB DFU: %d", err);
	}

	return err;
}

int boot_usb_dfu_enable(void)
{
	int err;

	if (!boot_dfu_initialized) {
		err = boot_dfu_init();
		if (err) {
			return err;
		}

		boot_dfu_initialized = true;
	}

	err = usbd_enable(&boot_dfu_usbd);
	if (err && err != -EALREADY) {
		BOOT_LOG_ERR("Failed to enable USB DFU: %d", err);
	}

	return err;
}

struct usbd_context *boot_usb_dfu_get_context(void)
{
	return &boot_dfu_usbd;
}

void boot_usb_dfu_wait(k_timeout_t delay)
{
	if (k_sem_take(&boot_dfu_complete, delay) != 0) {
		return;
	}

	BOOT_LOG_INF("USB DFU completed");

#if !defined(CONFIG_SINGLE_APPLICATION_SLOT)
	/* The DFU backend only writes the image into the secondary slot. Unless
	 * the swap is requested here the download is silently ignored on the
	 * next boot.
	 */
	if (boot_set_pending(IS_ENABLED(CONFIG_BOOT_USB_DFU_PERMANENT_DOWNLOAD))) {
		BOOT_LOG_ERR("Failed to request upgrade to the downloaded image");
	}
#endif
}
