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
#include "cbor_encode.h"
#include "cbor_common.h"

_Static_assert((sizeof(size_t) == sizeof(void *)),
	"This code needs size_t to be the same length as pointers.");

uint8_t get_additional(uint32_t len, uint8_t value0)
{
	switch(len) {
		case 0: return value0;
		case 1: return 24;
		case 2: return 25;
		case 3: return 25;
		case 4: return 26;
		case 5: return 26;
		case 6: return 26;
		case 7: return 26;
		case 8: return 27;
	}

	cbor_assert(false, NULL);
	return 0;
}

static bool encode_header_byte(cbor_state_t *state,
	cbor_major_type_t major_type, uint8_t additional)
{
	if ((state->payload + 1) > state->payload_end) {
		FAIL();
	}

	cbor_assert(additional < 32, NULL);

	*(state->payload_mut++) = (major_type << 5) | (additional & 0x1F);
	return true;
}

/** Encode a single value.
 */
static bool value_encode_len(cbor_state_t *state, cbor_major_type_t major_type,
		const void *const input, uint32_t result_len)
{
	uint8_t *u8_result  = (uint8_t *)input;

	if ((state->payload + 1 + result_len) > state->payload_end) {
		FAIL();
	}

	if (!encode_header_byte(state, major_type,
				get_additional(result_len, u8_result[0]))) {
		FAIL();
	}
	state->payload_mut--;
	cbor_trace();
	state->payload_mut++;

#ifdef CONFIG_BIG_ENDIAN
	memcpy(state->payload_mut, u8_result, result_len);
	state->payload_mut += result_len;
#else
	for (; result_len > 0; result_len--) {
		*(state->payload_mut++) = u8_result[result_len - 1];
	}
#endif

	state->elem_count++;
	return true;
}


static uint32_t get_result_len(const void *const input, uint32_t max_result_len)
{
	uint8_t *u8_result  = (uint8_t *)input;
	size_t i;

	for (i = 0; i < max_result_len; i++) {
#ifdef CONFIG_BIG_ENDIAN
		size_t idx = i;
#else
		size_t idx = max_result_len - 1 - i;
#endif
		if (u8_result[idx] != 0) {
			break;
		}
	}
	max_result_len -= i;

	/* According to specification result length can be encoded on 1, 2, 4
	 * or 8 bytes.
	 */
	cbor_assert(max_result_len <= 8, "Up to 8 bytes can be used to encode length.\n");
	size_t encode_byte_cnt = 1;

	for (size_t i = 0; i <= 3; i++) {
		if (max_result_len <= encode_byte_cnt) {
			max_result_len = encode_byte_cnt;
			break;
		}

		encode_byte_cnt *= 2;
	}

	if ((max_result_len == 1) && (u8_result[0] <= VALUE_IN_HEADER)) {
		max_result_len = 0;
	}

	return max_result_len;
}


static bool value_encode(cbor_state_t *state, cbor_major_type_t major_type,
		const void *const input, uint32_t max_result_len)
{
	cbor_assert(max_result_len != 0, "0-length result not supported.\n");
	return value_encode_len(state, major_type, input,
				get_result_len(input, max_result_len));
}


bool intx32_put(cbor_state_t *state, int32_t input)
{
	cbor_major_type_t major_type;

	if (input < 0) {
		major_type = CBOR_MAJOR_TYPE_NINT;
		/* Convert from CBOR's representation. */
		input = -1 - input;
	} else {
		major_type = CBOR_MAJOR_TYPE_PINT;
		input = input;
	}

	if (!value_encode(state, major_type, &input, 4)) {
		FAIL();
	}

	return true;
}

bool intx32_encode(cbor_state_t *state, const int32_t *input)
{
	return intx32_put(state, *input);
}


static bool uint32_encode(cbor_state_t *state, const uint32_t *input,
		cbor_major_type_t major_type)
{
	if (!value_encode(state, major_type, input, 4)) {
		FAIL();
	}
	return true;
}


bool uintx32_encode(cbor_state_t *state, const uint32_t *input)
{
	if (!uint32_encode(state, input, CBOR_MAJOR_TYPE_PINT)) {
		FAIL();
	}
	return true;
}


bool uintx32_put(cbor_state_t *state, uint32_t input)
{
	if (!uint32_encode(state, &input, CBOR_MAJOR_TYPE_PINT)) {
		FAIL();
	}
	return true;
}


static bool strx_start_encode(cbor_state_t *state,
		const cbor_string_type_t *input, cbor_major_type_t major_type)
{
	if (input->value && ((get_result_len(&input->len, sizeof(input->len))
			+ 1 + input->len + (size_t)state->payload)
			> (size_t)state->payload_end)) {
		FAIL();
	}
	if (!uint32_encode(state, &input->len, major_type)) {
		FAIL();
	}

	return true;
}


static bool primx_encode(cbor_state_t *state, uint32_t input)
{
	if (!uint32_encode(state, &input, CBOR_MAJOR_TYPE_PRIM)) {
		FAIL();
	}
	return true;
}


static uint32_t remaining_str_len(cbor_state_t *state)
{
	uint32_t max_len = (size_t)state->payload_end - (size_t)state->payload;
	uint32_t result_len = get_result_len(&max_len, sizeof(uint32_t));
	return max_len - result_len - 1;
}


bool bstrx_cbor_start_encode(cbor_state_t *state, const cbor_string_type_t *result)
{
	if (!new_backup(state, 0)) {
		FAIL();
	}

	uint32_t max_len = remaining_str_len(state);

	/* Encode a dummy header */
	if (!uint32_encode(state, &max_len,
			CBOR_MAJOR_TYPE_BSTR)) {
		FAIL();
	}
	return true;
}


bool bstrx_cbor_end_encode(cbor_state_t *state)
{
	const uint8_t *payload = state->payload;

	if (!restore_backup(state, FLAG_RESTORE | FLAG_DISCARD, 0xFFFFFFFF)) {
		FAIL();
	}
	cbor_string_type_t value;

	value.value = state->payload_end - remaining_str_len(state);
	value.len = (size_t)payload - (size_t)value.value;

	/* Reencode header of list now that we know the number of elements. */
	if (!bstrx_encode(state, &value)) {
		FAIL();
	}
	return true;
}


static bool strx_encode(cbor_state_t *state,
		const cbor_string_type_t *input, cbor_major_type_t major_type)
{
	if (!strx_start_encode(state, input, major_type)) {
		FAIL();
	}
	if (input->len > (state->payload_end - state->payload)) {
		FAIL();
	}
	if (state->payload_mut != input->value) {
		memmove(state->payload_mut, input->value, input->len);
	}
	state->payload += input->len;
	return true;
}


bool bstrx_encode(cbor_state_t *state, const cbor_string_type_t *input)
{
	return strx_encode(state, input, CBOR_MAJOR_TYPE_BSTR);
}


bool tstrx_encode(cbor_state_t *state, const cbor_string_type_t *input)
{
	return strx_encode(state, input, CBOR_MAJOR_TYPE_TSTR);
}


static bool list_map_start_encode(cbor_state_t *state, uint32_t max_num,
		cbor_major_type_t major_type)
{
#ifdef CDDL_CBOR_CANONICAL
	if (!new_backup(state, 0)) {
		FAIL();
	}

	/* Encode dummy header with max number of elements. */
	if (!uint32_encode(state, &max_num, major_type)) {
		FAIL();
	}
	state->elem_count--; /* Because of dummy header. */
#else
	if (!encode_header_byte(state, major_type, 31)) {
		FAIL();
	}
#endif
	return true;
}


bool list_start_encode(cbor_state_t *state, uint32_t max_num)
{
	return list_map_start_encode(state, max_num, CBOR_MAJOR_TYPE_LIST);
}


bool map_start_encode(cbor_state_t *state, uint32_t max_num)
{
	return list_map_start_encode(state, max_num, CBOR_MAJOR_TYPE_MAP);
}


bool list_map_end_encode(cbor_state_t *state, uint32_t max_num,
			cbor_major_type_t major_type)
{
#ifdef CDDL_CBOR_CANONICAL
	uint32_t list_count = ((major_type == CBOR_MAJOR_TYPE_LIST) ?
					state->elem_count
					: (state->elem_count / 2));

	const uint8_t *payload = state->payload;
	uint32_t max_header_len = get_result_len(&max_num, 4);
	uint32_t header_len = get_result_len(&list_count, 4);

	if (!restore_backup(state, FLAG_RESTORE | FLAG_DISCARD, 0xFFFFFFFF)) {
		FAIL();
	}

	cbor_print("list_count: %d\r\n", list_count);

	/* Reencode header of list now that we know the number of elements. */
	if (!(uint32_encode(state, &list_count, major_type))) {
		FAIL();
	}

	if (max_header_len != header_len) {
		const uint8_t *start = state->payload + max_header_len - header_len;
		uint32_t body_size = payload - start;
		memmove(state->payload_mut,
			state->payload + max_header_len - header_len,
			body_size);
		/* Reset payload pointer to end of list */
		state->payload += body_size;
	} else {
		/* Reset payload pointer to end of list */
		state->payload = payload;
	}
#else
	if (!encode_header_byte(state, CBOR_MAJOR_TYPE_PRIM, 31)) {
		FAIL();
	}
#endif
	return true;
}


bool list_end_encode(cbor_state_t *state, uint32_t max_num)
{
	return list_map_end_encode(state, max_num, CBOR_MAJOR_TYPE_LIST);
}


bool map_end_encode(cbor_state_t *state, uint32_t max_num)
{
	return list_map_end_encode(state, max_num, CBOR_MAJOR_TYPE_MAP);
}


bool nilx_put(cbor_state_t *state, const void *input)
{
	(void)input;
	return primx_encode(state, 22);
}


bool boolx_encode(cbor_state_t *state, const bool *input)
{
	if (!primx_encode(state, *input + BOOL_TO_PRIM)) {
		FAIL();
	}
	return true;
}


bool boolx_put(cbor_state_t *state, bool input)
{
	if (!primx_encode(state, input + BOOL_TO_PRIM)) {
		FAIL();
	}
	return true;
}


bool double_encode(cbor_state_t *state, double *input)
{
	if (!value_encode(state, CBOR_MAJOR_TYPE_PRIM, input,
			sizeof(*input))) {
		FAIL();
	}

	return true;
}


bool double_put(cbor_state_t *state, double input)
{
	return double_encode(state, &input);
}


bool any_encode(cbor_state_t *state, void *input)
{
	return nilx_put(state, input);
}


bool tag_encode(cbor_state_t *state, uint32_t tag)
{
	if (!value_encode(state, CBOR_MAJOR_TYPE_TAG, &tag, sizeof(tag))) {
		FAIL();
	}
	state->elem_count--;

	return true;
}


bool multi_encode(uint32_t min_encode,
		uint32_t max_encode,
		const uint32_t *num_encode,
		cbor_encoder_t encoder,
		cbor_state_t *state,
		const void *input,
		uint32_t result_len)
{
	if (!PTR_VALUE_IN_RANGE(uint32_t, num_encode, NULL, &max_encode)) {
		FAIL();
	}
	for (uint32_t i = 0; i < *num_encode; i++) {
		if (!encoder(state, (const uint8_t *)input + i*result_len)) {
			FAIL();
		}
	}
	cbor_print("Found %zu elements.\n", *num_encode);
	return true;
}


bool present_encode(const uint32_t *present,
		cbor_encoder_t encoder,
		cbor_state_t *state,
		const void *input)
{
	uint32_t num_encode = *present;
	bool retval = multi_encode(0, 1, &num_encode, encoder, state, input, 0);
	return retval;
}
