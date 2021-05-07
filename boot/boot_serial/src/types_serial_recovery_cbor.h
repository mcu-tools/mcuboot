/*
 * This file has been generated from the cddl-gen submodule.
 * Commit 8f9358a0b4b0e9b0cd579f0988056ef0b60760e4
 */

/*
 * Generated with cddl_gen.py (https://github.com/NordicSemiconductor/cddl-gen)
 * at: 2021-05-10 09:40:43
 * Generated with a default_max_qty of 3
 */

#ifndef TYPES_SERIAL_RECOVERY_CBOR_H__
#define TYPES_SERIAL_RECOVERY_CBOR_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "cbor_decode.h"

#define DEFAULT_MAX_QTY 3

struct Member_ {
 	union {
		struct {
			int32_t _Member_image;
		};
		struct {
			cbor_string_type_t _Member_data;
		};
		struct {
			int32_t _Member_len;
		};
		struct {
			int32_t _Member_off;
		};
		struct {
			cbor_string_type_t _Member_sha;
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

struct Upload {
 	struct Member_ _Upload_members[5];
	uint32_t _Upload_members_count;
};


#endif /* TYPES_SERIAL_RECOVERY_CBOR_H__ */
