/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Generated using zcbor version 0.7.0
 * https://github.com/NordicSemiconductor/zcbor
 * at: 2023-04-14 11:34:28
 * Generated with a --default-max-qty of 3
 */

#ifndef SERIAL_RECOVERY_CBOR_TYPES_H__
#define SERIAL_RECOVERY_CBOR_TYPES_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <zcbor_decode.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Which value for --default-max-qty this file was created with.
 *
 *  The define is used in the other generated file to do a build-time
 *  compatibility check.
 *
 *  See `zcbor --help` for more information about --default-max-qty
 */
#define DEFAULT_MAX_QTY 3

struct Member_ {
	union {
		struct {
			int32_t _Member_image;
		};
		struct {
			struct zcbor_string _Member_data;
		};
		struct {
			int32_t _Member_len;
		};
		struct {
			int32_t _Member_off;
		};
		struct {
			struct zcbor_string _Member_sha;
		};
	};
	enum {
		_Member_image,
		_Member_data,
		_Member_len,
		_Member_off,
		_Member_sha,
	} _Member_choice;
};

struct Upload_members {
	struct Member_ _Upload_members;
};

struct Upload {
	struct Upload_members _Upload_members[5];
	size_t _Upload_members_count;
};

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_RECOVERY_CBOR_TYPES_H__ */
