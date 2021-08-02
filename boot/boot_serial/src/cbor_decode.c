/*
 * This file has been copied from the cddl-gen submodule.
 * Commit 9f77837f9950da1633d22abf6181a830521a6688
 */

/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "cbor_decode.h"
#include "cbor_common.h"


/** Return value length from additional value.
 */
static uint32_t additional_len(uint8_t additional)
{
	if (24 <= additional && additional <= 27) {
		/* 24 => 1
		 * 25 => 2
		 * 26 => 4
		 * 27 => 8
		 */
		return 1 << (additional - 24);
	}
	return 0;
}

/** Extract the major type, i.e. the first 3 bits of the header byte. */
#define MAJOR_TYPE(header_byte) (((header_byte) >> 5) & 0x7)

/** Extract the additional info, i.e. the last 5 bits of the header byte. */
#define ADDITIONAL(header_byte) ((header_byte) & 0x1F)


#define FAIL_AND_DECR_IF(expr) \
do {\
	if (expr) { \
		(state->payload)--; \
		FAIL(); \
	} \
} while(0)

#define FAIL_IF(expr) \
do {\
	if (expr) { \
		FAIL(); \
	} \
} while(0)


#define FAIL_RESTORE() \
	state->payload = state->payload_bak; \
	state->elem_count++; \
	FAIL()

/** Get a single value.
 *
 * @details @p ppayload must point to the header byte. This function will
 *          retrieve the value (either from within the additional info, or from
 *          the subsequent bytes) and return it in the result. The result can
 *          have arbitrary length.
 *
 *          The function will also validate
 *           - Min/max constraints on the value.
 *           - That @p payload doesn't overrun past @p payload_end.
 *           - That @p elem_count has not been exhausted.
 *
 *          @p ppayload and @p elem_count are updated if the function
 *          succeeds. If not, they are left unchanged.
 *
 *          CBOR values are always big-endian, so this function converts from
 *          big to little-endian if necessary (@ref CONFIG_BIG_ENDIAN).
 */
static bool value_extract(cbor_state_t *state,
		void *const result, uint32_t result_len)
{
	cbor_trace();
	cbor_assert(result_len != 0, "0-length result not supported.\n");
	cbor_assert(result != NULL, NULL);

	FAIL_IF((state->elem_count == 0) \
		|| (state->payload >= state->payload_end));

	uint8_t *u8_result  = (uint8_t *)result;
	uint8_t additional = ADDITIONAL(*state->payload);

	state->payload_bak = state->payload;
	(state->payload)++;

	memset(result, 0, result_len);
	if (additional <= VALUE_IN_HEADER) {
#ifdef CONFIG_BIG_ENDIAN
		u8_result[result_len - 1] = additional;
#else
		u8_result[0] = additional;
#endif /* CONFIG_BIG_ENDIAN */
	} else {
		uint32_t len = additional_len(additional);

		FAIL_AND_DECR_IF(len > result_len);
		FAIL_AND_DECR_IF((state->payload + len)
				> state->payload_end);

#ifdef CONFIG_BIG_ENDIAN
		memcpy(&u8_result[result_len - len], state->payload, len);
#else
		for (uint32_t i = 0; i < len; i++) {
			u8_result[i] = (state->payload)[len - i - 1];
		}
#endif /* CONFIG_BIG_ENDIAN */

		(state->payload) += len;
	}

	(state->elem_count)--;
	return true;
}


static bool int32_decode(cbor_state_t *state, int32_t *result)
{
	FAIL_IF(state->payload >= state->payload_end);
	uint8_t major_type = MAJOR_TYPE(*state->payload);
	uint32_t uint_result;
	int32_t int_result;

	if (!value_extract(state, &uint_result, 4)) {
		FAIL();
	}

	cbor_print("uintval: %u\r\n", uint_result);
	if (uint_result >= (1 << (8*sizeof(uint_result)-1))) {
		/* Value is too large to fit in a signed integer. */
		FAIL_RESTORE();
	}

	if (major_type == CBOR_MAJOR_TYPE_NINT) {
		/* Convert from CBOR's representation. */
		int_result = -1 - uint_result;
	} else {
		int_result = uint_result;
	}

	cbor_print("val: %d\r\n", int_result);
	*result = int_result;
	return true;
}


bool intx32_decode(cbor_state_t *state, int32_t *result)
{
	FAIL_IF(state->payload >= state->payload_end);
	uint8_t major_type = MAJOR_TYPE(*state->payload);

	if (major_type != CBOR_MAJOR_TYPE_PINT
		&& major_type != CBOR_MAJOR_TYPE_NINT) {
		/* Value to be read doesn't have the right type. */
		FAIL();
	}

	if (!int32_decode(state, result)) {
		FAIL();
	}
	return true;
}

bool intx32_expect(cbor_state_t *state, int32_t result)
{
	int32_t value;

	if (!intx32_decode(state, &value)) {
		FAIL();
	}

	if (value != result) {
		cbor_print("%d != %d\r\n", value, result);
		FAIL_RESTORE();
	}
	return true;
}


static bool uint32_decode(cbor_state_t *state, uint32_t *result)
{
	if (!value_extract(state, result, 4)) {
		FAIL();
	}

	return true;
}


bool uintx32_decode(cbor_state_t *state, uint32_t *result)
{
	FAIL_IF(state->payload >= state->payload_end);
	uint8_t major_type = MAJOR_TYPE(*state->payload);

	if (major_type != CBOR_MAJOR_TYPE_PINT) {
		/* Value to be read doesn't have the right type. */
		FAIL();
	}
	if (!uint32_decode(state, result)) {
		FAIL();
	}
	return true;
}

bool uintx32_expect(cbor_state_t *state, uint32_t result)
{
	uint32_t value;

	if (!uintx32_decode(state, &value)) {
		FAIL();
	}
	if (value != result) {
		cbor_print("%u != %u\r\n", value, result);
		FAIL_RESTORE();
	}
	return true;
}

bool uintx32_expect_union(cbor_state_t *state, uint32_t result)
{
	union_elem_code(state);
	return uintx32_expect(state, result);
}


static bool strx_start_decode(cbor_state_t *state,
		cbor_string_type_t *result, cbor_major_type_t exp_major_type)
{
	FAIL_IF(state->payload >= state->payload_end);
	uint8_t major_type = MAJOR_TYPE(*state->payload);

	if (major_type != exp_major_type) {
		FAIL();
	}

	if (!uint32_decode(state, &result->len)) {
		FAIL();
	}

	if (result->len > (state->payload_end - state->payload)) {
		cbor_print("error: 0x%x > 0x%x\r\n",
		(uint32_t)result->len,
		(uint32_t)(state->payload_end - state->payload));
		FAIL_RESTORE();
	}

	result->value = state->payload;
	return true;
}

bool bstrx_cbor_start_decode(cbor_state_t *state, cbor_string_type_t *result)
{
	if(!strx_start_decode(state, result, CBOR_MAJOR_TYPE_BSTR)) {
		FAIL();
	}

	if (!new_backup(state, 0xFFFFFFFF)) {
		FAIL_RESTORE();
	}

	/* Overflow is checked in strx_start_decode() */
	state->payload_end = result->value + result->len;
	return true;
}

bool bstrx_cbor_end_decode(cbor_state_t *state)
{
	if (state->payload != state->payload_end) {
		FAIL();
	}
	if (!restore_backup(state,
			FLAG_RESTORE | FLAG_DISCARD | FLAG_TRANSFER_PAYLOAD,
			0xFFFFFFFF)) {
		FAIL();
	}

	return true;
}


bool strx_decode(cbor_state_t *state, cbor_string_type_t *result,
		cbor_major_type_t exp_major_type)
{
	if (!strx_start_decode(state, result, exp_major_type)) {
		FAIL();
	}

	/* Overflow is checked in strx_start_decode() */
	(state->payload) += result->len;
	return true;
}


bool strx_expect(cbor_state_t *state, cbor_string_type_t *result,
		cbor_major_type_t exp_major_type)
{
	cbor_string_type_t tmp_result;

	if (!strx_decode(state, &tmp_result, exp_major_type)) {
		FAIL();
	}
	if ((tmp_result.len != result->len)
			|| memcmp(result->value, tmp_result.value, tmp_result.len)) {
		FAIL_RESTORE();
	}
	return true;
}


bool bstrx_decode(cbor_state_t *state, cbor_string_type_t *result)
{
	return strx_decode(state, result, CBOR_MAJOR_TYPE_BSTR);
}


bool bstrx_expect(cbor_state_t *state, cbor_string_type_t *result)
{
	return strx_expect(state, result, CBOR_MAJOR_TYPE_BSTR);
}


bool tstrx_decode(cbor_state_t *state, cbor_string_type_t *result)
{
	return strx_decode(state, result, CBOR_MAJOR_TYPE_TSTR);
}


bool tstrx_expect(cbor_state_t *state, cbor_string_type_t *result)
{
	return strx_expect(state, result, CBOR_MAJOR_TYPE_TSTR);
}


static bool list_map_start_decode(cbor_state_t *state,
		cbor_major_type_t exp_major_type)
{
	FAIL_IF(state->payload >= state->payload_end);
	uint8_t major_type = MAJOR_TYPE(*state->payload);
	uint32_t new_elem_count;

	if (major_type != exp_major_type) {
		FAIL();
	}

	if (!uint32_decode(state, &new_elem_count)) {
		FAIL();
	}

	if (!new_backup(state, new_elem_count)) {
		FAIL_RESTORE();
	}

	return true;
}


bool list_start_decode(cbor_state_t *state)
{
	return list_map_start_decode(state, CBOR_MAJOR_TYPE_LIST);
}


bool map_start_decode(cbor_state_t *state)
{
	bool ret = list_map_start_decode(state, CBOR_MAJOR_TYPE_MAP);

	if (ret) {
		state->elem_count *= 2;
	}
	return ret;
}


bool list_map_end_decode(cbor_state_t *state)
{
	if (!restore_backup(state,
			FLAG_RESTORE | FLAG_DISCARD | FLAG_TRANSFER_PAYLOAD,
			0)) {
		FAIL();
	}

	return true;
}


bool list_end_decode(cbor_state_t *state)
{
	return list_map_end_decode(state);
}


bool map_end_decode(cbor_state_t *state)
{
	return list_map_end_decode(state);
}


static bool primx_decode(cbor_state_t *state, uint32_t *result)
{
	FAIL_IF(state->payload >= state->payload_end);
	uint8_t major_type = MAJOR_TYPE(*state->payload);

	if (major_type != CBOR_MAJOR_TYPE_PRIM) {
		/* Value to be read doesn't have the right type. */
		FAIL();
	}
	if (!uint32_decode(state, result)) {
		FAIL();
	}
	if (*result > 0xFF) {
		FAIL_RESTORE();
	}
	return true;
}

static bool primx_expect(cbor_state_t *state, uint32_t result)
{
	uint32_t value;

	if (!primx_decode(state, &value)) {
		FAIL();
	}
	if (value != result) {
		FAIL_RESTORE();
	}
	return true;
}


bool nilx_expect(cbor_state_t *state, void *result)
{
	if (!primx_expect(state, 22)) {
		FAIL();
	}
	return true;
}


bool boolx_decode(cbor_state_t *state, bool *result)
{
	uint32_t tmp_result;

	if (!primx_decode(state, &tmp_result)) {
		FAIL();
	}
	(*result) = tmp_result - BOOL_TO_PRIM;

	cbor_print("boolval: %u\r\n", *result);
	return true;
}


bool boolx_expect(cbor_state_t *state, bool result)
{
	bool value;

	if (!boolx_decode(state, &value)) {
		FAIL();
	}
	if (value != result) {
		FAIL_RESTORE();
	}
	return true;
}


bool double_decode(cbor_state_t *state, double *result)
{
	FAIL_IF(state->payload >= state->payload_end);
	uint8_t major_type = MAJOR_TYPE(*state->payload);

	if (major_type != CBOR_MAJOR_TYPE_PRIM) {
		/* Value to be read doesn't have the right type. */
		FAIL();
	}
	if (!value_extract(state, result,
			sizeof(*result))) {
		FAIL();
	}
	return true;
}


bool double_expect(cbor_state_t *state, double *result)
{
	double value;

	if (!double_decode(state, &value)) {
		FAIL();
	}
	if (value != *result) {
		FAIL_RESTORE();
	}
	return true;
}


bool any_decode(cbor_state_t *state, void *result)
{
	cbor_assert(result == NULL,
			"'any' type cannot be returned, only skipped.\n");

	FAIL_IF(state->payload >= state->payload_end);
	uint8_t major_type = MAJOR_TYPE(*state->payload);
	uint32_t value;
	uint32_t num_decode;
	void *null_result = NULL;
	uint32_t temp_elem_count;
	uint8_t const *payload_bak;

	if (!value_extract(state, &value, sizeof(value))) {
		/* Can happen because of elem_count (or payload_end) */
		FAIL();
	}

	switch (major_type) {
		case CBOR_MAJOR_TYPE_BSTR:
		case CBOR_MAJOR_TYPE_TSTR:
			(state->payload) += value;
			break;
		case CBOR_MAJOR_TYPE_MAP:
			value *= 2; /* Because all members have a key. */
			/* Fallthrough */
		case CBOR_MAJOR_TYPE_LIST:
			temp_elem_count = state->elem_count;
			payload_bak = state->payload;
			state->elem_count = value;
			if (!multi_decode(value, value, &num_decode,
					(void *)any_decode, state,
					&null_result, 0)) {
				state->elem_count = temp_elem_count;
				state->payload = payload_bak;
				FAIL();
			}
			state->elem_count = temp_elem_count;
			break;
		default:
			/* Do nothing */
			break;
	}

	return true;
}


bool tag_decode(cbor_state_t *state, uint32_t *result)
{
	FAIL_IF(state->payload >= state->payload_end);
	uint8_t major_type = MAJOR_TYPE(*state->payload);

	if (major_type != CBOR_MAJOR_TYPE_TAG) {
		/* Value to be read doesn't have the right type. */
		FAIL();
	}
	if (!uint32_decode(state, result)) {
		FAIL();
	}
	state->elem_count++;
	return true;
}


bool tag_expect(cbor_state_t *state, uint32_t result)
{
	uint32_t tag_val;

	if (!tag_decode(state, &tag_val)) {
		FAIL();
	}
	if (tag_val != result) {
		FAIL_RESTORE();
	}
	return true;
}


bool multi_decode(uint32_t min_decode,
		uint32_t max_decode,
		uint32_t *num_decode,
		cbor_decoder_t decoder,
		cbor_state_t *state,
		void *result,
		uint32_t result_len)
{
	for (uint32_t i = 0; i < max_decode; i++) {
		uint8_t const *payload_bak = state->payload;
		uint32_t elem_count_bak = state->elem_count;

		if (!decoder(state,
				(uint8_t *)result + i*result_len)) {
			*num_decode = i;
			state->payload = payload_bak;
			state->elem_count = elem_count_bak;
			if (i < min_decode) {
				FAIL();
			} else {
				cbor_print("Found %zu elements.\n", i);
			}
			return true;
		}
	}
	cbor_print("Found %zu elements.\n", max_decode);
	*num_decode = max_decode;
	return true;
}


bool present_decode(uint32_t *present,
		cbor_decoder_t decoder,
		cbor_state_t *state,
		void *result)
{
	uint32_t num_decode;
	bool retval = multi_decode(0, 1, &num_decode, decoder, state, result, 0);
	if (retval) {
		*present = num_decode;
	}
	return retval;
}
