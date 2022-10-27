/*
 * Copyright (c) 2022 Legrand North America, LLC.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <bootutil/caps.h>

#include "mcuboot_config/mcuboot_config.h"

/**
 * @addtogroup t_mcuboot_utils
 * @{
 * @defgroup t_mcuboot
 * @}
 */

#include "caps.c"


ZTEST(mcuboot_util_caps, test_get_caps)
{
    uint32_t res = bootutil_get_caps();
    uint32_t res2 = bootutil_get_caps();

    /* Verify value repeated on successive calls */
    zassert_equal(res, res2, NULL);

#if defined(MCUBOOT_SIGN_RSA)
#if MCUBOOT_SIGN_RSA_LEN == 2048
    zassert_equal(res & BOOTUTIL_CAP_RSA2048, BOOTUTIL_CAP_RSA2048, NULL);
    res &= ~BOOTUTIL_CAP_RSA2048;
#endif
#if MCUBOOT_SIGN_RSA_LEN == 3072
    zassert_equal(res & BOOTUTIL_CAP_RSA3072, BOOTUTIL_CAP_RSA3072, NULL);
    res &= ~BOOTUTIL_CAP_RSA3072;
#endif
#endif
#if defined(MCUBOOT_SIGN_EC)
    zassert_equal(res & BOOTUTIL_CAP_ECDSA_P224, BOOTUTIL_CAP_ECDSA_P224, NULL);
    res &= ~BOOTUTIL_CAP_ECDSA_P224;
#endif
#if defined(MCUBOOT_SIGN_EC256)
    zassert_equal(res = BOOTUTIL_CAP_ECDSA_P256, BOOTUTIL_CAP_ECDSA_P256, NULL);
    res &= ~BOOTUTIL_CAP_ECDSA_P256;
#endif
#if defined(MCUBOOT_SIGN_ED25519)
    zassert_equal(res & BOOTUTIL_CAP_ED25519, BOOTUTIL_CAP_ED25519, NULL);
    res &= ~BOOTUTIL_CAP_ED25519;
#endif
#if defined(MCUBOOT_OVERWRITE_ONLY)
    zassert_equal(res & BOOTUTIL_CAP_OVERWRITE_UPGRADE, BOOTUTIL_CAP_OVERWRITE_UPGRADE, NULL);
    res &= ~BOOTUTIL_CAP_OVERWRITE_UPGRADE;
#elif defined(MCUBOOT_SWAP_USING_MOVE)
    zassert_equal(res & BOOTUTIL_CAP_SWAP_USING_MOVE, BOOTUTIL_CAP_SWAP_USING_MOVE, NULL);
    res &= ~BOOTUTIL_CAP_SWAP_USING_MOVE;
#else
    zassert_equal(res & BOOTUTIL_CAP_SWAP_USING_SCRATCH, BOOTUTIL_CAP_SWAP_USING_SCRATCH, NULL);
    res &= ~BOOTUTIL_CAP_SWAP_USING_SCRATCH;
#endif
#if defined(MCUBOOT_ENCRYPT_RSA)
    zassert_equal(res & BOOTUTIL_CAP_ENC_RSA, BOOTUTIL_CAP_ENC_RSA, NULL);
    res &= ~BOOTUTIL_CAP_ENC_RSA;
#endif
#if defined(MCUBOOT_ENCRYPT_KW)
    zassert_equal(res & BOOTUTIL_CAP_ENC_KW, BOOTUTIL_CAP_ENC_KW, NULL);
    res &= ~BOOTUTIL_CAP_ENC_KW;
#endif
#if defined(MCUBOOT_ENCRYPT_EC256)
    zassert_equal(res & BOOTUTIL_CAP_ENC_EC256 BOOTUTIL_CAP_ENC_EC256, NULL);
    res &= ~BOOTUTIL_CAP_ENC_EC256;
#endif
#if defined(MCUBOOT_ENCRYPT_X25519)
    zassert_equal(res & BOOTUTIL_CAP_ENC_X25519, BOOTUTIL_CAP_ENC_X25519, NULL);
    res &= ~BOOTUTIL_CAP_ENC_X25519;
#endif
#if defined(MCUBOOT_VALIDATE_PRIMARY_SLOT)
    zassert_equal(res & BOOTUTIL_CAP_VALIDATE_PRIMARY_SLOT, BOOTUTIL_CAP_VALIDATE_PRIMARY_SLOT, NULL);
    res &= ~BOOTUTIL_CAP_VALIDATE_PRIMARY_SLOT;
#endif
#if defined(MCUBOOT_DOWNGRADE_PREVENTION)
    zassert_equal(res & BOOTUTIL_CAP_DOWNGRADE_PREVENTION, BOOTUTIL_CAP_DOWNGRADE_PREVENTION, NULL);
    res &= ~BOOTUTIL_CAP_DOWNGRADE_PREVENTION;
#endif
#if defined(MCUBOOT_BOOTSTRAP)
    zassert_equal(res & BOOTUTIL_CAP_BOOTSTRAP, BOOTUTIL_CAP_BOOTSTRAP, NULL);
    res &= ~BOOTUTIL_CAP_BOOTSTRAP;
#endif
#if defined(MCUBOOT_AES_256)
    zassert_equal(res & BOOTUTIL_CAP_AES256, BOOTUTIL_CAP_AES256, NULL);
    res &= ~BOOTUTIL_CAP_AES256;
#endif
#if defined(MCUBOOT_RAM_LOAD)
    zassert_equal(res & BOOTUTIL_CAP_RAM_LOAD, BOOTUTIL_CAP_RAM_LOAD, NULL);
    res &= ~BOOTUTIL_CAP_RAM_LOAD;
#endif
#if defined(MCUBOOT_DIRECT_XIP)
    zassert_equal(res & BOOTUTIL_CAP_DIRECT_XIP, BOOTUTIL_CAP_DIRECT_XIP, NULL, NULL);
    res &= ~BOOTUTIL_CAP_DIRECT_XIP;
#endif
    zassert_equal(res, 0, NULL);  /* Verify no other information is leaked */
}

ZTEST(mcuboot_util_caps, test_get_num_images)
{
    uint32_t res = bootutil_get_num_images();
    uint32_t res2 = bootutil_get_num_images();

    /* Verify value repeated on successive calls */
    zassert_equal(res, res2, NULL);

#if defined(MCUBOOT_IMAGE_NUMBER)
    zassert_equal(res, MCUBOOT_IMAGE_NUMBER, NULL);
#else
    zassert_equal(res, 1, NULL);
#endif
}

ZTEST_SUITE(mcuboot_util_caps, NULL, NULL, NULL, NULL, NULL);
