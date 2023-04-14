/*
 * This file has been copied from the zcbor library.
 * Commit zcbor 0.7.0
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
#include "zcbor_encode.h"
#include "zcbor_common.h"

_Static_assert((sizeof(size_t) == sizeof(void *)),
	"This code needs size_t to be the same length as pointers.");


static uint8_t log2ceil(uint_fast32_t val)
{
	switch(val) {
		case 1: return 0;
		case 2: return 1;
		case 3: return 2;
		case 4: return 2;
		case 5: return 3;
		case 6: return 3;
		case 7: return 3;
		case 8: return 3;
	}

	zcbor_print("Should not come here.\r\n");
	return 0;
}

static uint8_t get_additional(uint_fast32_t len, uint8_t value0)
{
	return len == 0 ? value0 : (uint8_t)(24 + log2ceil(len));
}

static bool encode_header_byte(zcbor_state_t *state,
	zcbor_major_type_t major_type, uint8_t additional)
{
	ZCBOR_CHECK_ERROR();
	ZCBOR_CHECK_PAYLOAD();

	zcbor_assert_state(additional < 32, NULL);

	*(state->payload_mut++) = (uint8_t)((major_type << 5) | (additional & 0x1F));
	return true;
}


static uint_fast32_t get_encoded_len(const void *const result, uint_fast32_t result_len);


/** Encode a single value.
 */
static bool value_encode_len(zcbor_state_t *state, zcbor_major_type_t major_type,
		const void *const result, uint_fast32_t result_len)
{
	uint8_t *u8_result  = (uint8_t *)result;
	uint_fast32_t encoded_len = get_encoded_len(result, result_len);

	if ((state->payload + 1 + encoded_len) > state->payload_end) {
		ZCBOR_ERR(ZCBOR_ERR_NO_PAYLOAD);
	}

	if (!encode_header_byte(state, major_type,
				get_additional(encoded_len, u8_result[0]))) {
		ZCBOR_FAIL();
	}
	state->payload_mut--;
	zcbor_trace();
	state->payload_mut++;

#ifdef CONFIG_BIG_ENDIAN
	memcpy(state->payload_mut, u8_result, encoded_len);
	state->payload_mut += encoded_len;
#else
	for (; encoded_len > 0; encoded_len--) {
		*(state->payload_mut++) = u8_result[encoded_len - 1];
	}
#endif /* CONFIG_BIG_ENDIAN */

	state->elem_count++;
	return true;
}


static uint_fast32_t get_result_len(const void *const input, uint_fast32_t max_result_len)
{
	uint8_t *u8_result  = (uint8_t *)input;
	uint_fast32_t len = max_result_len;

	for (; len > 0; len--) {
#ifdef CONFIG_BIG_ENDIAN
		if (u8_result[max_result_len - len] != 0) {
#else
		if (u8_result[len - 1] != 0) {
#endif /* CONFIG_BIG_ENDIAN */
			break;
		}
	}

	/* Round up to nearest power of 2. */
	return len <= 2 ? len : (uint8_t)(1 << log2ceil(len));
}


static const void *get_result(const void *const input, uint_fast32_t max_result_len,
	uint_fast32_t result_len)
{
#ifdef CONFIG_BIG_ENDIAN
	return &((uint8_t *)input)[max_result_len - (result_len ? result_len : 1)];
#else
	return input;
#endif
}


static uint_fast32_t get_encoded_len(const void *const result, uint_fast32_t result_len)
{
	const uint8_t *u8_result  = (const uint8_t *)result;

	if ((result_len == 1) && (u8_result[0] <= ZCBOR_VALUE_IN_HEADER)) {
		return 0;
	}
	return result_len;
}


static bool value_encode(zcbor_state_t *state, zcbor_major_type_t major_type,
		const void *const input, uint_fast32_t max_result_len)
{
	zcbor_assert_state(max_result_len != 0, "0-length result not supported.\r\n");

	uint_fast32_t result_len = get_result_len(input, max_result_len);
	const void *const result = get_result(input, max_result_len, result_len);

	return value_encode_len(state, major_type, result, result_len);
}


bool zcbor_int_encode(zcbor_state_t *state, const void *input_int, size_t int_size)
{
	zcbor_major_type_t major_type;
	uint8_t input_buf[8];
	const uint8_t *input_uint8 = input_int;
	const int8_t *input_int8 = input_int;
	const uint8_t *input = input_int;

	if (int_size > sizeof(int64_t)) {
		ZCBOR_ERR(ZCBOR_ERR_INT_SIZE);
	}

#ifdef CONFIG_BIG_ENDIAN
	if (input_int8[0] < 0) {
#else
	if (input_int8[int_size - 1] < 0) {
#endif
		major_type = ZCBOR_MAJOR_TYPE_NINT;

		/* Convert to CBOR's representation by flipping all bits. */
		for (int i = 0; i < int_size; i++) {
			input_buf[i] = (uint8_t)~input_uint8[i];
		}
		input = input_buf;
	} else {
		major_type = ZCBOR_MAJOR_TYPE_PINT;
	}

	if (!value_encode(state, major_type, input, int_size)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_int32_encode(zcbor_state_t *state, const int32_t *input)
{
	return zcbor_int_encode(state, input, sizeof(*input));
}


bool zcbor_int64_encode(zcbor_state_t *state, const int64_t *input)
{
	return zcbor_int_encode(state, input, sizeof(*input));
}


static bool uint32_encode(zcbor_state_t *state, const uint32_t *input,
		zcbor_major_type_t major_type)
{
	if (!value_encode(state, major_type, input, 4)) {
		ZCBOR_FAIL();
	}
	return true;
}


bool zcbor_uint32_encode(zcbor_state_t *state, const uint32_t *input)
{
	if (!uint32_encode(state, input, ZCBOR_MAJOR_TYPE_PINT)) {
		ZCBOR_FAIL();
	}
	return true;
}


static bool uint64_encode(zcbor_state_t *state, const uint64_t *input,
		zcbor_major_type_t major_type)
{
	if (!value_encode(state, major_type, input, 8)) {
		ZCBOR_FAIL();
	}
	return true;
}


bool zcbor_uint64_encode(zcbor_state_t *state, const uint64_t *input)
{
	if (!uint64_encode(state, input, ZCBOR_MAJOR_TYPE_PINT)) {
		ZCBOR_FAIL();
	}
	return true;
}


bool zcbor_int32_put(zcbor_state_t *state, int32_t input)
{
	return zcbor_int_encode(state, &input, sizeof(input));
}


bool zcbor_int64_put(zcbor_state_t *state, int64_t input)
{
	return zcbor_int_encode(state, &input, sizeof(input));
}


bool zcbor_uint32_put(zcbor_state_t *state, uint32_t input)
{
	return zcbor_uint64_put(state, input);
}


bool zcbor_uint64_put(zcbor_state_t *state, uint64_t input)
{
	if (!uint64_encode(state, &input, ZCBOR_MAJOR_TYPE_PINT)) {
		ZCBOR_FAIL();
	}
	return true;
}


#ifdef ZCBOR_SUPPORTS_SIZE_T
bool zcbor_size_put(zcbor_state_t *state, size_t input)
{
	return zcbor_uint64_put(state, input);
}


bool zcbor_size_encode(zcbor_state_t *state, const size_t *input)
{
	return zcbor_size_put(state, *input);
}
#endif

static bool str_start_encode(zcbor_state_t *state,
		const struct zcbor_string *input, zcbor_major_type_t major_type)
{
	if (input->value && ((get_result_len(&input->len, sizeof(input->len))
			+ 1 + input->len + (size_t)state->payload)
			> (size_t)state->payload_end)) {
		ZCBOR_ERR(ZCBOR_ERR_NO_PAYLOAD);
	}
	if (!value_encode(state, major_type, &input->len, sizeof(input->len))) {
		ZCBOR_FAIL();
	}

	return true;
}


static bool primitive_put(zcbor_state_t *state, uint32_t input)
{
	if (!uint32_encode(state, &input, ZCBOR_MAJOR_TYPE_PRIM)) {
		ZCBOR_FAIL();
	}
	return true;
}


static size_t remaining_str_len(zcbor_state_t *state)
{
	size_t max_len = (size_t)state->payload_end - (size_t)state->payload;
	size_t result_len = get_result_len(&max_len, sizeof(max_len));

	return max_len - result_len - 1;
}


bool zcbor_bstr_start_encode(zcbor_state_t *state)
{
	if (!zcbor_new_backup(state, 0)) {
		ZCBOR_FAIL();
	}

	uint64_t max_len = remaining_str_len(state);

	/* Encode a dummy header */
	if (!uint64_encode(state, &max_len,
			ZCBOR_MAJOR_TYPE_BSTR)) {
		ZCBOR_FAIL();
	}
	return true;
}


bool zcbor_bstr_end_encode(zcbor_state_t *state, struct zcbor_string *result)
{
	const uint8_t *payload = state->payload;
	struct zcbor_string dummy_value;

	if (result == NULL) {
		/* Use a dummy value for the sake of the length calculation below.
		 * Will not be returned.
		 */
		result = &dummy_value;
	}

	if (!zcbor_process_backup(state, ZCBOR_FLAG_RESTORE | ZCBOR_FLAG_CONSUME, 0xFFFFFFFF)) {
		ZCBOR_FAIL();
	}

	result->value = state->payload_end - remaining_str_len(state);
	result->len = (size_t)payload - (size_t)result->value;

	/* Reencode header of list now that we know the number of elements. */
	if (!zcbor_bstr_encode(state, result)) {
		ZCBOR_FAIL();
	}
	return true;
}


static bool str_encode(zcbor_state_t *state,
		const struct zcbor_string *input, zcbor_major_type_t major_type)
{
	if (input->len > (state->payload_end - state->payload)) {
		ZCBOR_ERR(ZCBOR_ERR_NO_PAYLOAD);
	}
	if (!str_start_encode(state, input, major_type)) {
		ZCBOR_FAIL();
	}
	if (state->payload_mut != input->value) {
		/* Use memmove since string might be encoded into the same space
		 * because of bstrx_cbor_start_encode/bstrx_cbor_end_encode. */
		memmove(state->payload_mut, input->value, input->len);
	}
	state->payload += input->len;
	return true;
}


bool zcbor_bstr_encode(zcbor_state_t *state, const struct zcbor_string *input)
{
	return str_encode(state, input, ZCBOR_MAJOR_TYPE_BSTR);
}


bool zcbor_tstr_encode(zcbor_state_t *state, const struct zcbor_string *input)
{
	return str_encode(state, input, ZCBOR_MAJOR_TYPE_TSTR);
}


static bool list_map_start_encode(zcbor_state_t *state, uint_fast32_t max_num,
		zcbor_major_type_t major_type)
{
#ifdef ZCBOR_CANONICAL
	if (!zcbor_new_backup(state, 0)) {
		ZCBOR_FAIL();
	}

	/* Encode dummy header with max number of elements. */
	if (!value_encode(state, major_type, &max_num, sizeof(max_num))) {
		ZCBOR_FAIL();
	}
	state->elem_count--; /* Because of dummy header. */
#else
	if (!encode_header_byte(state, major_type, ZCBOR_VALUE_IS_INDEFINITE_LENGTH)) {
		ZCBOR_FAIL();
	}
#endif
	return true;
}


bool zcbor_list_start_encode(zcbor_state_t *state, uint_fast32_t max_num)
{
	return list_map_start_encode(state, max_num, ZCBOR_MAJOR_TYPE_LIST);
}


bool zcbor_map_start_encode(zcbor_state_t *state, uint_fast32_t max_num)
{
	return list_map_start_encode(state, max_num, ZCBOR_MAJOR_TYPE_MAP);
}


#ifdef ZCBOR_CANONICAL
static uint_fast32_t get_encoded_len2(const void *const input, uint_fast32_t max_result_len)
{
	uint_fast32_t result_len = get_result_len(input, max_result_len);
	const void *const result = get_result(input, max_result_len, result_len);

	return get_encoded_len(result, result_len);
}
#endif


static bool list_map_end_encode(zcbor_state_t *state, uint_fast32_t max_num,
			zcbor_major_type_t major_type)
{
#ifdef ZCBOR_CANONICAL
	uint_fast32_t list_count = ((major_type == ZCBOR_MAJOR_TYPE_LIST) ?
					state->elem_count
					: (state->elem_count / 2));

	const uint8_t *payload = state->payload;

	uint_fast32_t max_header_len = get_encoded_len2(&max_num, 4);
	uint_fast32_t header_len = get_encoded_len2(&list_count, 4);

	if (!zcbor_process_backup(state, ZCBOR_FLAG_RESTORE | ZCBOR_FLAG_CONSUME, 0xFFFFFFFF)) {
		ZCBOR_FAIL();
	}

	zcbor_print("list_count: %" PRIuFAST32 "\r\n", list_count);


	/** If max_num is smaller than the actual number of encoded elements,
	  * the value_encode() below will corrupt the data if the encoded
	  * header is larger than the previously encoded header. */
	if (header_len > max_header_len) {
		zcbor_print("max_num too small.\r\n");
		ZCBOR_ERR(ZCBOR_ERR_HIGH_ELEM_COUNT);
	}

	/* Reencode header of list now that we know the number of elements. */
	if (!(value_encode(state, major_type, &list_count, sizeof(list_count)))) {
		ZCBOR_FAIL();
	}

	if (max_header_len != header_len) {
		const uint8_t *start = state->payload + max_header_len - header_len;
		size_t body_size = (size_t)payload - (size_t)start;

		memmove(state->payload_mut, start, body_size);
		/* Reset payload pointer to end of list */
		state->payload += body_size;
	} else {
		/* Reset payload pointer to end of list */
		state->payload = payload;
	}
#else
	if (!encode_header_byte(state, ZCBOR_MAJOR_TYPE_PRIM, ZCBOR_VALUE_IS_INDEFINITE_LENGTH)) {
		ZCBOR_FAIL();
	}
#endif
	return true;
}


bool zcbor_list_end_encode(zcbor_state_t *state, uint_fast32_t max_num)
{
	return list_map_end_encode(state, max_num, ZCBOR_MAJOR_TYPE_LIST);
}


bool zcbor_map_end_encode(zcbor_state_t *state, uint_fast32_t max_num)
{
	return list_map_end_encode(state, max_num, ZCBOR_MAJOR_TYPE_MAP);
}


bool zcbor_list_map_end_force_encode(zcbor_state_t *state)
{
#ifdef ZCBOR_CANONICAL
	if (!zcbor_process_backup(state, ZCBOR_FLAG_RESTORE | ZCBOR_FLAG_CONSUME,
			ZCBOR_MAX_ELEM_COUNT)) {
		ZCBOR_FAIL();
	}
#endif
	return true;
}


bool zcbor_nil_put(zcbor_state_t *state, const void *unused)
{
	(void)unused;
	return primitive_put(state, 22);
}


bool zcbor_undefined_put(zcbor_state_t *state, const void *unused)
{
	(void)unused;
	return primitive_put(state, 23);
}


bool zcbor_bool_encode(zcbor_state_t *state, const bool *input)
{
	if (!primitive_put(state, (uint32_t)(*input + ZCBOR_BOOL_TO_PRIM))) {
		ZCBOR_FAIL();
	}
	return true;
}


bool zcbor_bool_put(zcbor_state_t *state, bool input)
{
	if (!primitive_put(state, (uint32_t)(input + ZCBOR_BOOL_TO_PRIM))) {
		ZCBOR_FAIL();
	}
	return true;
}


bool zcbor_float64_encode(zcbor_state_t *state, const double *input)
{
	if (!value_encode_len(state, ZCBOR_MAJOR_TYPE_PRIM, input,
			sizeof(*input))) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float64_put(zcbor_state_t *state, double input)
{
	return zcbor_float64_encode(state, &input);
}


bool zcbor_float32_encode(zcbor_state_t *state, const float *input)
{
	if (!value_encode_len(state, ZCBOR_MAJOR_TYPE_PRIM, input,
			sizeof(*input))) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float32_put(zcbor_state_t *state, float input)
{
	return zcbor_float32_encode(state, &input);
}


bool zcbor_tag_encode(zcbor_state_t *state, uint32_t tag)
{
	if (!value_encode(state, ZCBOR_MAJOR_TYPE_TAG, &tag, sizeof(tag))) {
		ZCBOR_FAIL();
	}
	state->elem_count--;

	return true;
}


bool zcbor_multi_encode_minmax(uint_fast32_t min_encode,
		uint_fast32_t max_encode,
		const uint_fast32_t *num_encode,
		zcbor_encoder_t encoder,
		zcbor_state_t *state,
		const void *input,
		uint_fast32_t result_len)
{

	if ((*num_encode >= min_encode) && (*num_encode <= max_encode)) {
		return zcbor_multi_encode(*num_encode, encoder, state, input, result_len);
	} else {
		ZCBOR_ERR(ZCBOR_ERR_ITERATIONS);
	}
}

bool zcbor_multi_encode(uint_fast32_t num_encode,
		zcbor_encoder_t encoder,
		zcbor_state_t *state,
		const void *input,
		uint_fast32_t result_len)
{
	ZCBOR_CHECK_ERROR();
	for (uint_fast32_t i = 0; i < num_encode; i++) {
		if (!encoder(state, (const uint8_t *)input + i*result_len)) {
			ZCBOR_FAIL();
		}
	}
	zcbor_print("Encoded %" PRIuFAST32 " elements.\n", num_encode);
	return true;
}


bool zcbor_present_encode(const uint_fast32_t *present,
		zcbor_encoder_t encoder,
		zcbor_state_t *state,
		const void *input)
{
	return zcbor_multi_encode(!!*present, encoder, state, input, 0);
}


void zcbor_new_encode_state(zcbor_state_t *state_array, uint_fast32_t n_states,
		uint8_t *payload, size_t payload_len, uint_fast32_t elem_count)
{
	zcbor_new_state(state_array, n_states, payload, payload_len, elem_count);
}
