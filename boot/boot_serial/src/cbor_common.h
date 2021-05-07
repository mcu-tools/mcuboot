/*
 * This file has been copied from the cddl-gen submodule.
 * Commit 8f9358a0b4b0e9b0cd579f0988056ef0b60760e4
 */

/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CBOR_COMMON_H__
#define CBOR_COMMON_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


/** Convenience type that allows pointing to strings directly inside the payload
 *  without the need to copy out.
 */
typedef struct
{
	const uint8_t *value;
	uint32_t len;
} cbor_string_type_t;

#ifdef CDDL_CBOR_VERBOSE
#include <sys/printk.h>
#define cbor_trace() (printk("bytes left: %d, byte: 0x%x, elem_count: 0x%zx, %s:%d\n",\
	(uint32_t)state->payload_end - (uint32_t)state->payload, *state->payload, state->elem_count,\
	__FILE__, __LINE__))
#define cbor_assert(expr, ...) \
do { \
	if (!(expr)) { \
		printk("ASSERTION \n  \"" #expr \
			"\"\nfailed at %s:%d with message:\n  ", \
			__FILE__, __LINE__); \
		printk(__VA_ARGS__);\
		return false; \
	} \
} while(0)
#define cbor_print(...) printk(__VA_ARGS__)
#else
#define cbor_trace() ((void)state)
#define cbor_assert(...)
#define cbor_print(...)
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif


struct cbor_state_backups_s;

typedef struct cbor_state_backups_s cbor_state_backups_t;

typedef struct{
union {
	uint8_t *payload_mut;
	uint8_t const *payload; /**< The current place in the payload. Will be
	                             updated when an element is correctly
	                             processed. */
};
	uint8_t const *payload_bak; /**< Temporary backup of payload. */
	uint32_t elem_count; /**< The current element is part of a LIST or a MAP,
	                          and this keeps count of how many elements are
	                          expected. This will be checked before processing
	                          and decremented if the element is correctly
	                          processed. */
	uint8_t const *payload_end; /**< The end of the payload. This will be
	                                 checked against payload before
	                                 processing each element. */
	cbor_state_backups_t *backups;
} cbor_state_t;

struct cbor_state_backups_s{
	cbor_state_t *backup_list;
	uint32_t current_backup;
	uint32_t num_backups;
};

/** Function pointer type used with multi_decode.
 *
 * This type is compatible with all decoding functions here and in the generated
 * code, except for multi_decode.
 */
typedef bool(cbor_encoder_t)(cbor_state_t *, const void *);
typedef bool(cbor_decoder_t)(cbor_state_t *, void *);

/** Enumeration representing the major types available in CBOR.
 *
 * The major type is represented in the 3 first bits of the header byte.
 */
typedef enum
{
	CBOR_MAJOR_TYPE_PINT = 0, ///! Positive Integer
	CBOR_MAJOR_TYPE_NINT = 1, ///! Negative Integer
	CBOR_MAJOR_TYPE_BSTR = 2, ///! Byte String
	CBOR_MAJOR_TYPE_TSTR = 3, ///! Text String
	CBOR_MAJOR_TYPE_LIST = 4, ///! List
	CBOR_MAJOR_TYPE_MAP  = 5, ///! Map
	CBOR_MAJOR_TYPE_TAG  = 6, ///! Semantic Tag
	CBOR_MAJOR_TYPE_PRIM = 7, ///! Primitive Type
} cbor_major_type_t;

/** Shorthand macro to check if a result is within min/max constraints.
 */
#define PTR_VALUE_IN_RANGE(type, res, min, max) \
		(((min == NULL) || (*(type *)res >= *(type *)min)) \
		&& ((max == NULL) || (*(type *)res <= *(type *)max)))

#define FAIL() \
do {\
	cbor_trace(); \
	return false; \
} while(0)


#define VALUE_IN_HEADER 23 /**! For values below this, the value is encoded
                                directly in the header. */

#define BOOL_TO_PRIM 20 ///! In CBOR, false/true have the values 20/21

#define FLAG_RESTORE 1UL
#define FLAG_DISCARD 2UL
#define FLAG_TRANSFER_PAYLOAD 4UL

bool new_backup(cbor_state_t *state, uint32_t new_elem_count);

bool restore_backup(cbor_state_t *state, uint32_t flags,
		uint32_t max_elem_count);

bool union_start_code(cbor_state_t *state);

bool union_elem_code(cbor_state_t *state);

bool union_end_code(cbor_state_t *state);

bool entry_function(const uint8_t *payload, uint32_t payload_len,
		const void *struct_ptr, uint32_t *payload_len_out,
		cbor_encoder_t func, uint32_t elem_count, uint32_t num_backups);

#endif /* CBOR_COMMON_H__ */
