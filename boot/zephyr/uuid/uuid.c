/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <bootutil/mcuboot_uuid.h>

#define IMAGE_ID_COUNT CONFIG_UPDATEABLE_IMAGE_NUMBER
#define CID_INIT(index, label) \
	static const struct image_uuid label = {{ \
		MCUBOOT_UUID_CID_IMAGE_## index ##_VALUE \
	}}
#define CID_CONFIG(index) UTIL_CAT(CONFIG_MCUBOOT_UUID_CID_IMAGE_, index)
#define CID_DEFINE(index, prefix) \
	IF_ENABLED(CID_CONFIG(index), (CID_INIT(index, prefix##index)))

#define CID_CONDITION(index, label) \
	if (image_id == index) { \
		*uuid_cid = &label; \
		FIH_RET(FIH_SUCCESS); \
	}
#define CID_CHECK(index, prefix) \
	IF_ENABLED(CID_CONFIG(index), (CID_CONDITION(index, prefix##index)))

static fih_ret boot_uuid_compare(const struct image_uuid *uuid1, const struct image_uuid *uuid2)
{
	return fih_ret_encode_zero_equality(memcmp(uuid1->raw, uuid2->raw,
					    ARRAY_SIZE(uuid1->raw)));
}

#ifdef CONFIG_MCUBOOT_UUID_CID
LISTIFY(IMAGE_ID_COUNT, CID_DEFINE, (;), uuid_cid_image_);

static fih_ret boot_uuid_cid_get(uint32_t image_id, const struct image_uuid **uuid_cid)
{
	if (uuid_cid != NULL) {
		LISTIFY(IMAGE_ID_COUNT, CID_CHECK, (), uuid_cid_image_)
	}

	FIH_RET(FIH_FAILURE);
}
#endif /* CONFIG_MCUBOOT_UUID_CID */

fih_ret boot_uuid_init(void)
{
	FIH_RET(FIH_SUCCESS);
}

#ifdef CONFIG_MCUBOOT_UUID_VID
fih_ret boot_uuid_vid_match(uint32_t image_id, const struct image_uuid *uuid_vid)
{
	const struct image_uuid uuid_vid_c = {{
		MCUBOOT_UUID_VID_VALUE
	}};

	return boot_uuid_compare(uuid_vid, &uuid_vid_c);
}
#endif /* CONFIG_MCUBOOT_UUID_VID */

#ifdef CONFIG_MCUBOOT_UUID_CID
fih_ret boot_uuid_cid_match(uint32_t image_id, const struct image_uuid *uuid_cid)
{
	FIH_DECLARE(ret_code, FIH_FAILURE);
	const struct image_uuid *exp_uuid_cid = NULL;

	FIH_CALL(boot_uuid_cid_get, ret_code, image_id, &exp_uuid_cid);
	if (FIH_NOT_EQ(ret_code, FIH_SUCCESS) && FIH_NOT_EQ(ret_code, FIH_FAILURE)) {
		FIH_RET(FIH_FAILURE);
	}

	return boot_uuid_compare(uuid_cid, exp_uuid_cid);
}
#endif /* CONFIG_MCUBOOT_UUID_CID */
