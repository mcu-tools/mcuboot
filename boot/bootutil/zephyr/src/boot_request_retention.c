/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2025 Nordic Semiconductor ASA
 */
#include <bootutil/boot_request.h>

#include <zephyr/retention/retention.h>
#include "bootutil/bootutil_log.h"

/** Special value of image number, indicating a request to the bootloader. */
#define BOOT_REQUEST_IMG_BOOTLOADER 0xFF

/** Helper value, indicating how many images are supported. */
#define BOOT_REQUEST_IMAGES 2

/** Number of requests per image. */
#define BOOT_REQUEST_PER_IMAGE 2

/** Maximum number of request slots. */
#define BOOT_REQUEST_SLOT_MAX_N (BOOT_REQUEST_PER_IMAGE * BOOT_REQUEST_IMAGES + 1)

MCUBOOT_LOG_MODULE_REGISTER(bootloader_request);

static const struct device *bootloader_request_dev =
    DEVICE_DT_GET(DT_CHOSEN(zephyr_bootloader_request));
BUILD_ASSERT((BOOT_REQUEST_SLOT_MAX_N * sizeof(uint8_t)) <
             DT_REG_SIZE_BY_IDX(DT_CHOSEN(zephyr_bootloader_request), 0),
             "zephyr,bootloader-request area is too small for bootloader request struct");

enum boot_request_type {
    /** Invalid request. */
    BOOT_REQUEST_INVALID = 0,

    /** Request a change in the bootloader boot mode.
     *
     * @details Use @p boot_request_mode as argument.
     *          @p BOOT_REQUEST_IMG_BOOTLOADER as image number.
     *
     * @note Used to trigger recovery through i.e. retention sybsystem.
     */
    BOOT_REQUEST_BOOT_MODE = 1,

    /** Select the preferred image to be selected during boot or update.
     *
     * @details Use @p boot_request_slot_t as argument.
     *
     * @note Used in the Direct XIP mode.
     */
    BOOT_REQUEST_IMG_PREFERENCE = 2,

    /** Request a confirmation of an image.
     *
     * @details Use @p boot_request_slot_t as argument.
     *
     * @note Used if the code cannot modify the image trailer directly.
     */
    BOOT_REQUEST_IMG_CONFIRM = 3,
};

enum boot_request_slot {
    /** Unsupported value. */
    BOOT_REQUEST_SLOT_INVALID = 0,
    /** Primary slot. */
    BOOT_REQUEST_SLOT_PRIMARY = 1,
    /** Secondary slot. */
    BOOT_REQUEST_SLOT_SECONDARY = 2,
};

/** Alias type for the image and number. */
typedef uint8_t boot_request_slot_t;

enum boot_request_mode {
    /** Execute a regular boot logic. */
    BOOT_REQUEST_MODE_REGULAR = 0,
    /** Execute the recovery boot logic. */
    BOOT_REQUEST_MODE_RECOVERY = 1,
    /** Execute the firmware loader logic. */
    BOOT_REQUEST_MODE_FIRMWARE_LOADER = 2,
    /** Unsupported value. */
    BOOT_REQUEST_MODE_INVALID = 0xFF,
};

/** Alias type for the image number. */
typedef uint8_t boot_request_img_t;

/**
 * @brief Find a variable slot for a given request.
 *
 * @param[in]  type   Type of request.
 * @param[in]  image  Image number. Use @p BOOT_REQUEST_IMG_BOOTLOADER for generic requests.
 * @param[out] slot   Variable slot to use.
 *
 * @return 0 on success; nonzero on failure.
 */
static int
boot_request_slot_find(enum boot_request_type type, boot_request_img_t image, size_t *slot)
{
    if (slot == NULL) {
        return -EINVAL;
    }

    switch (type) {
    case BOOT_REQUEST_BOOT_MODE:
        *slot = 0;
        break;
    case BOOT_REQUEST_IMG_PREFERENCE:
        *slot = 1 + image * BOOT_REQUEST_PER_IMAGE;
        break;
    case BOOT_REQUEST_IMG_CONFIRM:
        *slot = 2 + image * BOOT_REQUEST_PER_IMAGE;
        break;
    default:
        return -EINVAL;
    }

    if (*slot > BOOT_REQUEST_SLOT_MAX_N) {
        return -EINVAL;
    }

    return 0;
}

int
boot_request_init(void)
{
    if (!device_is_ready(bootloader_request_dev)) {
        return -EIO;
    }

    return 0;
}

int
boot_request_clear(void)
{
    uint8_t value = 0xFF;
    int ret;

    for (size_t i = 0; i < retention_size(bootloader_request_dev); i++) {
        ret = retention_write(bootloader_request_dev, i * sizeof(value), (void *)&value,
                              sizeof(value));
        if (ret != 0) {
            break;
        }
    }

    return ret;
}

int
boot_request_confirm_slot(uint8_t image, uint32_t slot)
{
    uint8_t value = BOOT_REQUEST_SLOT_INVALID;
    size_t req_slot;
    int ret;

    ret = boot_request_slot_find(BOOT_REQUEST_IMG_CONFIRM, image, &req_slot);
    if (ret != 0) {
        return ret;
    }

    switch (slot) {
    case 0:
        value = BOOT_REQUEST_SLOT_PRIMARY;
        break;
    case 1:
        value = BOOT_REQUEST_SLOT_SECONDARY;
        break;
    default:
        return -EINVAL;
    }

    return retention_write(bootloader_request_dev, req_slot * sizeof(value), (void *)&value,
                           sizeof(value));
}

bool
boot_request_check_confirmed_slot(uint8_t image, uint32_t slot)
{
    uint8_t value = BOOT_REQUEST_SLOT_INVALID;
    size_t req_slot;
    int ret;

    ret = boot_request_slot_find(BOOT_REQUEST_IMG_CONFIRM, image, &req_slot);
    if (ret != 0) {
        return false;
    }

    ret = retention_read(bootloader_request_dev, req_slot * sizeof(value), (void *)&value,
                         sizeof(value));
    if (ret != 0) {
        return false;
    }

    switch (value) {
    case BOOT_REQUEST_SLOT_PRIMARY:
        return (slot == 0);
    case BOOT_REQUEST_SLOT_SECONDARY:
        return (slot == 1);
    default:
        break;
    }

    return false;
}

int
boot_request_set_preferred_slot(uint8_t image, uint32_t slot)
{
    uint8_t value = BOOT_REQUEST_SLOT_INVALID;
    size_t req_slot;
    int ret;

    ret = boot_request_slot_find(BOOT_REQUEST_IMG_PREFERENCE, image, &req_slot);
    if (ret != 0) {
        return ret;
    }

    switch (slot) {
    case 0:
        value = BOOT_REQUEST_SLOT_PRIMARY;
        break;
    case 1:
        value = BOOT_REQUEST_SLOT_SECONDARY;
        break;
    default:
        return -EINVAL;
    }

    return retention_write(bootloader_request_dev, req_slot * sizeof(value), (void *)&value,
                           sizeof(value));
}

uint32_t
boot_request_get_preferred_slot(uint8_t image)
{
    uint8_t value = BOOT_REQUEST_SLOT_INVALID;
    size_t req_slot;
    int ret;

    ret = boot_request_slot_find(BOOT_REQUEST_IMG_PREFERENCE, image, &req_slot);
    if (ret != 0) {
        return BOOT_REQUEST_NO_PREFERRED_SLOT;
    }

    ret = retention_read(bootloader_request_dev, req_slot * sizeof(value), (void *)&value,
                         sizeof(value));
    if (ret != 0) {
        return BOOT_REQUEST_NO_PREFERRED_SLOT;
    }

    switch (value) {
    case BOOT_REQUEST_SLOT_PRIMARY:
        return 0;
    case BOOT_REQUEST_SLOT_SECONDARY:
        return 1;
    default:
        break;
    }

    return BOOT_REQUEST_NO_PREFERRED_SLOT;
}

int
boot_request_enter_recovery(void)
{
    uint8_t value = BOOT_REQUEST_MODE_RECOVERY;
    size_t req_slot;
    int ret;

    ret = boot_request_slot_find(BOOT_REQUEST_BOOT_MODE, BOOT_REQUEST_IMG_BOOTLOADER,
                                 &req_slot);
    if (ret != 0) {
        return ret;
    }

    return retention_write(bootloader_request_dev, req_slot * sizeof(value), (void *)&value,
                           sizeof(value));
}

bool
boot_request_detect_recovery(void)
{
    uint8_t value = BOOT_REQUEST_MODE_INVALID;
    size_t req_slot;
    int ret;

    ret = boot_request_slot_find(BOOT_REQUEST_BOOT_MODE, BOOT_REQUEST_IMG_BOOTLOADER,
                                 &req_slot);
    if (ret != 0) {
        return false;
    }

    ret = retention_read(bootloader_request_dev, req_slot * sizeof(value), (void *)&value,
                         sizeof(value));
    if ((ret == 0) && (value == BOOT_REQUEST_MODE_RECOVERY)) {
        return true;
    }

    return false;
}

int
boot_request_enter_firmware_loader(void)
{
    uint8_t value = BOOT_REQUEST_MODE_FIRMWARE_LOADER;
    size_t req_slot;
    int ret;

    ret = boot_request_slot_find(BOOT_REQUEST_BOOT_MODE, BOOT_REQUEST_IMG_BOOTLOADER,
                                 &req_slot);
    if (ret != 0) {
        return ret;
    }

    return retention_write(bootloader_request_dev, req_slot * sizeof(value), (void *)&value,
                           sizeof(value));
}

bool
boot_request_detect_firmware_loader(void)
{
    uint8_t value = BOOT_REQUEST_MODE_INVALID;
    size_t req_slot;
    int ret;

    ret = boot_request_slot_find(BOOT_REQUEST_BOOT_MODE, BOOT_REQUEST_IMG_BOOTLOADER,
                                 &req_slot);
    if (ret != 0) {
        return false;
    }

    ret = retention_read(bootloader_request_dev, req_slot * sizeof(value), (void *)&value,
                         sizeof(value));
    if ((ret == 0) && (value == BOOT_REQUEST_MODE_FIRMWARE_LOADER)) {
        return true;
    }

    return false;
}
