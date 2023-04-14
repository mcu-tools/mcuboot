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
#include "zcbor_decode.h"
#include "zcbor_common.h"


/** Return value length from additional value.
 */
static uint_fast32_t additional_len(uint8_t additional)
{
	if (ZCBOR_VALUE_IS_1_BYTE <= additional && additional <= ZCBOR_VALUE_IS_8_BYTES) {
		/* 24 => 1
		 * 25 => 2
		 * 26 => 4
		 * 27 => 8
		 */
		return 1U << (additional - ZCBOR_VALUE_IS_1_BYTE);
	}
	return 0;
}

/** Extract the major type, i.e. the first 3 bits of the header byte. */
#define MAJOR_TYPE(header_byte) ((zcbor_major_type_t)(((header_byte) >> 5) & 0x7))

/** Extract the additional info, i.e. the last 5 bits of the header byte. */
#define ADDITIONAL(header_byte) ((header_byte) & 0x1F)


#define FAIL_AND_DECR_IF(expr, err) \
do {\
	if (expr) { \
		(state->payload)--; \
		ZCBOR_ERR(err); \
	} \
} while(0)

static bool initial_checks(zcbor_state_t *state)
{
	ZCBOR_CHECK_ERROR();
	ZCBOR_CHECK_PAYLOAD();
	return true;
}

static bool type_check(zcbor_state_t *state, zcbor_major_type_t exp_major_type)
{
	if (!initial_checks(state)) {
		ZCBOR_FAIL();
	}
	zcbor_major_type_t major_type = MAJOR_TYPE(*state->payload);

	if (major_type != exp_major_type) {
		ZCBOR_ERR(ZCBOR_ERR_WRONG_TYPE);
	}
	return true;
}

#define INITIAL_CHECKS() \
do {\
	if (!initial_checks(state)) { \
		ZCBOR_FAIL(); \
	} \
} while(0)

#define INITIAL_CHECKS_WITH_TYPE(exp_major_type) \
do {\
	if (!type_check(state, exp_major_type)) { \
		ZCBOR_FAIL(); \
	} \
} while(0)

#define ERR_RESTORE(err) \
do { \
	state->payload = state->payload_bak; \
	state->elem_count++; \
	ZCBOR_ERR(err); \
} while(0)

#define FAIL_RESTORE() \
do { \
	state->payload = state->payload_bak; \
	state->elem_count++; \
	ZCBOR_FAIL(); \
} while(0)

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
static bool value_extract(zcbor_state_t *state,
		void *const result, uint_fast32_t result_len)
{
	zcbor_trace();
	zcbor_assert_state(result_len != 0, "0-length result not supported.\r\n");
	zcbor_assert_state(result != NULL, NULL);

	INITIAL_CHECKS();
	ZCBOR_ERR_IF((state->elem_count == 0), ZCBOR_ERR_LOW_ELEM_COUNT);

	uint8_t *u8_result  = (uint8_t *)result;
	uint8_t additional = ADDITIONAL(*state->payload);

	state->payload_bak = state->payload;
	(state->payload)++;

	memset(result, 0, result_len);
	if (additional <= ZCBOR_VALUE_IN_HEADER) {
#ifdef CONFIG_BIG_ENDIAN
		u8_result[result_len - 1] = additional;
#else
		u8_result[0] = additional;
#endif /* CONFIG_BIG_ENDIAN */
	} else {
		uint_fast32_t len = additional_len(additional);

		FAIL_AND_DECR_IF(len > result_len, ZCBOR_ERR_INT_SIZE);
		FAIL_AND_DECR_IF(len == 0, ZCBOR_ERR_ADDITIONAL_INVAL); // additional_len() did not recognize the additional value.
		FAIL_AND_DECR_IF((state->payload + len) > state->payload_end,
			ZCBOR_ERR_NO_PAYLOAD);

#ifdef CONFIG_BIG_ENDIAN
		memcpy(&u8_result[result_len - len], state->payload, len);
#else
		for (uint_fast32_t i = 0; i < len; i++) {
			u8_result[i] = (state->payload)[len - i - 1];
		}
#endif /* CONFIG_BIG_ENDIAN */

		(state->payload) += len;
	}

	(state->elem_count)--;
	return true;
}


bool zcbor_int_decode(zcbor_state_t *state, void *result_int, size_t int_size)
{
	INITIAL_CHECKS();
	zcbor_major_type_t major_type = MAJOR_TYPE(*state->payload);
	uint8_t *result_uint8 = (uint8_t *)result_int;
	int8_t *result_int8 = (int8_t *)result_int;

	if (major_type != ZCBOR_MAJOR_TYPE_PINT
		&& major_type != ZCBOR_MAJOR_TYPE_NINT) {
		/* Value to be read doesn't have the right type. */
		ZCBOR_ERR(ZCBOR_ERR_WRONG_TYPE);
	}

	if (!value_extract(state, result_int, int_size)) {
		ZCBOR_FAIL();
	}

#ifdef CONFIG_BIG_ENDIAN
	if (result_int8[0] < 0) {
#else
	if (result_int8[int_size - 1] < 0) {
#endif
		/* Value is too large to fit in a signed integer. */
		ERR_RESTORE(ZCBOR_ERR_INT_SIZE);
	}

	if (major_type == ZCBOR_MAJOR_TYPE_NINT) {
		/* Convert from CBOR's representation by flipping all bits. */
		for (int i = 0; i < int_size; i++) {
			result_uint8[i] = (uint8_t)~result_uint8[i];
		}
	}

	return true;
}


bool zcbor_int32_decode(zcbor_state_t *state, int32_t *result)
{
	return zcbor_int_decode(state, result, sizeof(*result));
}


bool zcbor_int64_decode(zcbor_state_t *state, int64_t *result)
{
	return zcbor_int_decode(state, result, sizeof(*result));
}


bool zcbor_uint32_decode(zcbor_state_t *state, uint32_t *result)
{
	INITIAL_CHECKS_WITH_TYPE(ZCBOR_MAJOR_TYPE_PINT);

	if (!value_extract(state, result, sizeof(*result))) {
		ZCBOR_FAIL();
	}
	return true;
}


bool zcbor_int32_expect_union(zcbor_state_t *state, int32_t result)
{
	if (!zcbor_union_elem_code(state)) {
		ZCBOR_FAIL();
	}
	return zcbor_int32_expect(state, result);
}


bool zcbor_int64_expect_union(zcbor_state_t *state, int64_t result)
{
	if (!zcbor_union_elem_code(state)) {
		ZCBOR_FAIL();
	}
	return zcbor_int64_expect(state, result);
}


bool zcbor_uint32_expect_union(zcbor_state_t *state, uint32_t result)
{
	if (!zcbor_union_elem_code(state)) {
		ZCBOR_FAIL();
	}
	return zcbor_uint32_expect(state, result);
}


bool zcbor_uint64_expect_union(zcbor_state_t *state, uint64_t result)
{
	if (!zcbor_union_elem_code(state)) {
		ZCBOR_FAIL();
	}
	return zcbor_uint64_expect(state, result);
}


bool zcbor_int32_expect(zcbor_state_t *state, int32_t result)
{
	return zcbor_int64_expect(state, result);
}


bool zcbor_int64_expect(zcbor_state_t *state, int64_t result)
{
	int64_t value;

	if (!zcbor_int64_decode(state, &value)) {
		ZCBOR_FAIL();
	}

	if (value != result) {
		zcbor_print("%" PRIi64 " != %" PRIi64 "\r\n", value, result);
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_uint64_decode(zcbor_state_t *state, uint64_t *result)
{
	INITIAL_CHECKS_WITH_TYPE(ZCBOR_MAJOR_TYPE_PINT);

	if (!value_extract(state, result, sizeof(*result))) {
		ZCBOR_FAIL();
	}
	return true;
}


#ifdef ZCBOR_SUPPORTS_SIZE_T
bool zcbor_size_decode(zcbor_state_t *state, size_t *result)
{
	return value_extract(state, result, sizeof(size_t));
}
#endif


bool zcbor_uint32_expect(zcbor_state_t *state, uint32_t result)
{
	return zcbor_uint64_expect(state, result);
}


bool zcbor_uint64_expect(zcbor_state_t *state, uint64_t result)
{
	uint64_t value;

	if (!zcbor_uint64_decode(state, &value)) {
		ZCBOR_FAIL();
	}
	if (value != result) {
		zcbor_print("%" PRIu64 " != %" PRIu64 "\r\n", value, result);
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


#ifdef ZCBOR_SUPPORTS_SIZE_T
bool zcbor_size_expect(zcbor_state_t *state, size_t result)
{
	return zcbor_uint64_expect(state, result);
}
#endif


static bool str_start_decode(zcbor_state_t *state,
		struct zcbor_string *result, zcbor_major_type_t exp_major_type)
{
	INITIAL_CHECKS_WITH_TYPE(exp_major_type);

	if (!value_extract(state, &result->len, sizeof(result->len))) {
		ZCBOR_FAIL();
	}

	result->value = state->payload;
	return true;
}


static bool str_overflow_check(zcbor_state_t *state, struct zcbor_string *result)
{
	if (result->len > (state->payload_end - state->payload)) {
		zcbor_print("error: 0x%zu > 0x%zu\r\n",
		result->len,
		(state->payload_end - state->payload));
		ERR_RESTORE(ZCBOR_ERR_NO_PAYLOAD);
	}
	return true;
}


bool zcbor_bstr_start_decode(zcbor_state_t *state, struct zcbor_string *result)
{
	struct zcbor_string dummy;
	if (result == NULL) {
		result = &dummy;
	}

	if(!str_start_decode(state, result, ZCBOR_MAJOR_TYPE_BSTR)) {
		ZCBOR_FAIL();
	}

	if (!str_overflow_check(state, result)) {
		ZCBOR_FAIL();
	}

	if (!zcbor_new_backup(state, ZCBOR_MAX_ELEM_COUNT)) {
		FAIL_RESTORE();
	}

	state->payload_end = result->value + result->len;
	return true;
}


bool zcbor_bstr_end_decode(zcbor_state_t *state)
{
	ZCBOR_ERR_IF(state->payload != state->payload_end, ZCBOR_ERR_PAYLOAD_NOT_CONSUMED);

	if (!zcbor_process_backup(state,
			ZCBOR_FLAG_RESTORE | ZCBOR_FLAG_CONSUME | ZCBOR_FLAG_TRANSFER_PAYLOAD,
			ZCBOR_MAX_ELEM_COUNT)) {
		ZCBOR_FAIL();
	}

	return true;
}


static void partition_fragment(const zcbor_state_t *state,
	struct zcbor_string_fragment *result)
{
	result->fragment.len = MIN(result->fragment.len,
		(size_t)state->payload_end - (size_t)state->payload);
}


static bool start_decode_fragment(zcbor_state_t *state,
	struct zcbor_string_fragment *result,
	zcbor_major_type_t exp_major_type)
{
	if(!str_start_decode(state, &result->fragment, exp_major_type)) {
		ZCBOR_FAIL();
	}

	result->offset = 0;
	result->total_len = result->fragment.len;
	partition_fragment(state, result);
	state->payload_end = state->payload + result->fragment.len;

	return true;
}

bool zcbor_bstr_start_decode_fragment(zcbor_state_t *state,
	struct zcbor_string_fragment *result)
{
	if (!start_decode_fragment(state, result, ZCBOR_MAJOR_TYPE_BSTR)) {
		ZCBOR_FAIL();
	}
	if (!zcbor_new_backup(state, ZCBOR_MAX_ELEM_COUNT)) {
		FAIL_RESTORE();
	}
	return true;
}


void zcbor_next_fragment(zcbor_state_t *state,
	struct zcbor_string_fragment *prev_fragment,
	struct zcbor_string_fragment *result)
{
	memcpy(result, prev_fragment, sizeof(*result));
	result->fragment.value = state->payload_mut;
	result->offset += prev_fragment->fragment.len;
	result->fragment.len = result->total_len - result->offset;

	partition_fragment(state, result);
	zcbor_print("New fragment length %zu\r\n", result->fragment.len);

	state->payload += result->fragment.len;
}


void zcbor_bstr_next_fragment(zcbor_state_t *state,
	struct zcbor_string_fragment *prev_fragment,
	struct zcbor_string_fragment *result)
{
	memcpy(result, prev_fragment, sizeof(*result));
	result->fragment.value = state->payload_mut;
	result->offset += prev_fragment->fragment.len;
	result->fragment.len = result->total_len - result->offset;

	partition_fragment(state, result);
	zcbor_print("fragment length %zu\r\n", result->fragment.len);
	state->payload_end = state->payload + result->fragment.len;
}


bool zcbor_is_last_fragment(const struct zcbor_string_fragment *fragment)
{
	return (fragment->total_len == (fragment->offset + fragment->fragment.len));
}


static bool str_decode(zcbor_state_t *state, struct zcbor_string *result,
		zcbor_major_type_t exp_major_type)
{
	if (!str_start_decode(state, result, exp_major_type)) {
		ZCBOR_FAIL();
	}

	if (!str_overflow_check(state, result)) {
		ZCBOR_FAIL();
	}

	state->payload += result->len;
	return true;
}


static bool str_decode_fragment(zcbor_state_t *state, struct zcbor_string_fragment *result,
		zcbor_major_type_t exp_major_type)
{
	if (!start_decode_fragment(state, result, exp_major_type)) {
		ZCBOR_FAIL();
	}

	(state->payload) += result->fragment.len;
	return true;
}


static bool str_expect(zcbor_state_t *state, struct zcbor_string *result,
		zcbor_major_type_t exp_major_type)
{
	struct zcbor_string tmp_result;

	if (!str_decode(state, &tmp_result, exp_major_type)) {
		ZCBOR_FAIL();
	}
	if ((tmp_result.len != result->len)
			|| memcmp(result->value, tmp_result.value, tmp_result.len)) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_bstr_decode(zcbor_state_t *state, struct zcbor_string *result)
{
	return str_decode(state, result, ZCBOR_MAJOR_TYPE_BSTR);
}


bool zcbor_bstr_decode_fragment(zcbor_state_t *state, struct zcbor_string_fragment *result)
{
	return str_decode_fragment(state, result, ZCBOR_MAJOR_TYPE_BSTR);
}


bool zcbor_bstr_expect(zcbor_state_t *state, struct zcbor_string *result)
{
	return str_expect(state, result, ZCBOR_MAJOR_TYPE_BSTR);
}


bool zcbor_tstr_decode(zcbor_state_t *state, struct zcbor_string *result)
{
	return str_decode(state, result, ZCBOR_MAJOR_TYPE_TSTR);
}


bool zcbor_tstr_decode_fragment(zcbor_state_t *state, struct zcbor_string_fragment *result)
{
	return str_decode_fragment(state, result, ZCBOR_MAJOR_TYPE_TSTR);
}


bool zcbor_tstr_expect(zcbor_state_t *state, struct zcbor_string *result)
{
	return str_expect(state, result, ZCBOR_MAJOR_TYPE_TSTR);
}


static bool list_map_start_decode(zcbor_state_t *state,
		zcbor_major_type_t exp_major_type)
{
	uint_fast32_t new_elem_count;
	bool indefinite_length_array = false;

	INITIAL_CHECKS_WITH_TYPE(exp_major_type);

	if (ADDITIONAL(*state->payload) == ZCBOR_VALUE_IS_INDEFINITE_LENGTH) {
		/* Indefinite length array. */
		new_elem_count = ZCBOR_LARGE_ELEM_COUNT;
		ZCBOR_ERR_IF(state->elem_count == 0, ZCBOR_ERR_LOW_ELEM_COUNT);
		indefinite_length_array = true;
		state->payload++;
		state->elem_count--;
	} else {
		if (!value_extract(state, &new_elem_count, sizeof(new_elem_count))) {
			ZCBOR_FAIL();
		}
	}

	if (!zcbor_new_backup(state, new_elem_count)) {
		FAIL_RESTORE();
	}

	state->indefinite_length_array = indefinite_length_array;

	return true;
}


bool zcbor_list_start_decode(zcbor_state_t *state)
{
	return list_map_start_decode(state, ZCBOR_MAJOR_TYPE_LIST);
}


bool zcbor_map_start_decode(zcbor_state_t *state)
{
	bool ret = list_map_start_decode(state, ZCBOR_MAJOR_TYPE_MAP);

	if (ret && !state->indefinite_length_array) {
		if (state->elem_count >= (ZCBOR_MAX_ELEM_COUNT / 2)) {
			/* The new elem_count is too large. */
			ERR_RESTORE(ZCBOR_ERR_INT_SIZE);
		}
		state->elem_count *= 2;
	}
	return ret;
}


static bool array_end_expect(zcbor_state_t *state)
{
	INITIAL_CHECKS();
	ZCBOR_ERR_IF(*state->payload != 0xFF, ZCBOR_ERR_WRONG_TYPE);

	state->payload++;
	return true;
}


static bool list_map_end_decode(zcbor_state_t *state)
{
	uint_fast32_t max_elem_count = 0;

	if (state->indefinite_length_array) {
		if (!array_end_expect(state)) {
			ZCBOR_FAIL();
		}
		max_elem_count = ZCBOR_MAX_ELEM_COUNT;
		state->indefinite_length_array = false;
	}
	if (!zcbor_process_backup(state,
			ZCBOR_FLAG_RESTORE | ZCBOR_FLAG_CONSUME | ZCBOR_FLAG_TRANSFER_PAYLOAD,
			max_elem_count)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_list_end_decode(zcbor_state_t *state)
{
	return list_map_end_decode(state);
}


bool zcbor_map_end_decode(zcbor_state_t *state)
{
	return list_map_end_decode(state);
}


bool zcbor_list_map_end_force_decode(zcbor_state_t *state)
{
	if (!zcbor_process_backup(state,
			ZCBOR_FLAG_RESTORE | ZCBOR_FLAG_CONSUME | ZCBOR_FLAG_TRANSFER_PAYLOAD,
			ZCBOR_MAX_ELEM_COUNT)) {
		ZCBOR_FAIL();
	}

	return true;
}


static bool primx_expect(zcbor_state_t *state, uint8_t result)
{
	uint32_t value;

	INITIAL_CHECKS_WITH_TYPE(ZCBOR_MAJOR_TYPE_PRIM);

	if (!value_extract(state, &value, sizeof(value))) {
		ZCBOR_FAIL();
	}

	if (value != result) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_nil_expect(zcbor_state_t *state, void *unused)
{
	if (!primx_expect(state, 22)) {
		ZCBOR_FAIL();
	}
	return true;
}


bool zcbor_undefined_expect(zcbor_state_t *state, void *unused)
{
	if (!primx_expect(state, 23)) {
		ZCBOR_FAIL();
	}
	return true;
}


bool zcbor_bool_decode(zcbor_state_t *state, bool *result)
{
	if (zcbor_bool_expect(state, false)) {
		*result = false;
	} else if (zcbor_bool_expect(state, true)) {
		*result = true;
	} else {
		ZCBOR_FAIL();
	}

	zcbor_print("boolval: %u\r\n", *result);
	return true;
}


bool zcbor_bool_expect(zcbor_state_t *state, bool result)
{
	if (!primx_expect(state, (uint8_t)(!!result) + ZCBOR_BOOL_TO_PRIM)) {
		ZCBOR_FAIL();
	}
	return true;
}


bool zcbor_float32_decode(zcbor_state_t *state, float *result)
{
	INITIAL_CHECKS_WITH_TYPE(ZCBOR_MAJOR_TYPE_PRIM);
	ZCBOR_ERR_IF(ADDITIONAL(*state->payload) != ZCBOR_VALUE_IS_4_BYTES, ZCBOR_ERR_FLOAT_SIZE);

	if (!value_extract(state, result, sizeof(*result))) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float32_expect(zcbor_state_t *state, float result)
{
	float value;

	if (!zcbor_float32_decode(state, &value)) {
		ZCBOR_FAIL();
	}
	if (value != result) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_float64_decode(zcbor_state_t *state, double *result)
{
	INITIAL_CHECKS_WITH_TYPE(ZCBOR_MAJOR_TYPE_PRIM);
	ZCBOR_ERR_IF(ADDITIONAL(*state->payload) != ZCBOR_VALUE_IS_8_BYTES, ZCBOR_ERR_FLOAT_SIZE);

	if (!value_extract(state, result, sizeof(*result))) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float64_expect(zcbor_state_t *state, double result)
{
	double value;

	if (!zcbor_float64_decode(state, &value)) {
		ZCBOR_FAIL();
	}
	if (value != result) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_float_decode(zcbor_state_t *state, double *result)
{
	float float_result;

	if (zcbor_float32_decode(state, &float_result)) {
		*result = (double)float_result;
	} else if (!zcbor_float64_decode(state, result)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float_expect(zcbor_state_t *state, double result)
{
	if (zcbor_float32_expect(state, (float)result)) {
		/* Do nothing */
	} else if (!zcbor_float64_expect(state, result)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_any_skip(zcbor_state_t *state, void *result)
{
	zcbor_assert_state(result == NULL,
			"'any' type cannot be returned, only skipped.\r\n");

	INITIAL_CHECKS();
	zcbor_major_type_t major_type = MAJOR_TYPE(*state->payload);
	uint8_t additional = ADDITIONAL(*state->payload);
	uint_fast32_t value;
	uint_fast32_t num_decode;
	uint_fast32_t temp_elem_count;
	uint_fast32_t elem_count_bak = state->elem_count;
	uint8_t const *payload_bak = state->payload;
	uint64_t tag_dummy;

	payload_bak = state->payload;

	if (!zcbor_multi_decode(0, ZCBOR_LARGE_ELEM_COUNT, &num_decode,
			(zcbor_decoder_t *)zcbor_tag_decode, state,
			(void *)&tag_dummy, 0)) {
		state->elem_count = elem_count_bak;
		state->payload = payload_bak;
		ZCBOR_FAIL();
	}

	if ((major_type == ZCBOR_MAJOR_TYPE_MAP) || (major_type == ZCBOR_MAJOR_TYPE_LIST)) {
		if (additional == ZCBOR_VALUE_IS_INDEFINITE_LENGTH) {
			ZCBOR_ERR_IF(state->elem_count == 0, ZCBOR_ERR_LOW_ELEM_COUNT);
			state->payload++;
			state->elem_count--;
			temp_elem_count = state->elem_count;
			payload_bak = state->payload;
			state->elem_count = ZCBOR_LARGE_ELEM_COUNT;
			if (!zcbor_multi_decode(0, ZCBOR_LARGE_ELEM_COUNT, &num_decode,
					(zcbor_decoder_t *)zcbor_any_skip, state,
					NULL, 0)
					|| (state->payload >= state->payload_end)
					|| !(*(state->payload++) == 0xFF)) {
				state->elem_count = elem_count_bak;
				state->payload = payload_bak;
				ZCBOR_FAIL();
			}
			state->elem_count = temp_elem_count;
			return true;
		}
	}

	if (!value_extract(state, &value, sizeof(value))) {
		/* Can happen because of elem_count (or payload_end) */
		ZCBOR_FAIL();
	}

	switch (major_type) {
		case ZCBOR_MAJOR_TYPE_BSTR:
		case ZCBOR_MAJOR_TYPE_TSTR:
			/* 'value' is the length of the BSTR or TSTR */
			if (value > (state->payload_end - state->payload)) {
				ZCBOR_ERR(ZCBOR_ERR_NO_PAYLOAD);
			}
			(state->payload) += value;
			break;
		case ZCBOR_MAJOR_TYPE_MAP:
			value *= 2; /* Because all members have a key. */
			/* Fallthrough */
		case ZCBOR_MAJOR_TYPE_LIST:
			temp_elem_count = state->elem_count;
			state->elem_count = value;
			if (!zcbor_multi_decode(value, value, &num_decode,
					(zcbor_decoder_t *)zcbor_any_skip, state,
					NULL, 0)) {
				state->elem_count = elem_count_bak;
				state->payload = payload_bak;
				ZCBOR_FAIL();
			}
			state->elem_count = temp_elem_count;
			break;
		default:
			/* Do nothing */
			break;
	}

	return true;
}


bool zcbor_tag_decode(zcbor_state_t *state, uint32_t *result)
{
	INITIAL_CHECKS_WITH_TYPE(ZCBOR_MAJOR_TYPE_TAG);

	if (!value_extract(state, result, sizeof(*result))) {
		ZCBOR_FAIL();
	}
	state->elem_count++;
	return true;
}


bool zcbor_tag_expect(zcbor_state_t *state, uint32_t result)
{
	uint32_t tag_val;

	if (!zcbor_tag_decode(state, &tag_val)) {
		ZCBOR_FAIL();
	}
	if (tag_val != result) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_multi_decode(uint_fast32_t min_decode,
		uint_fast32_t max_decode,
		uint_fast32_t *num_decode,
		zcbor_decoder_t decoder,
		zcbor_state_t *state,
		void *result,
		uint_fast32_t result_len)
{
	ZCBOR_CHECK_ERROR();
	for (uint_fast32_t i = 0; i < max_decode; i++) {
		uint8_t const *payload_bak = state->payload;
		uint_fast32_t elem_count_bak = state->elem_count;

		if (!decoder(state,
				(uint8_t *)result + i*result_len)) {
			*num_decode = i;
			state->payload = payload_bak;
			state->elem_count = elem_count_bak;
			ZCBOR_ERR_IF(i < min_decode, ZCBOR_ERR_ITERATIONS);
			zcbor_print("Found %" PRIuFAST32 " elements.\r\n", i);
			return true;
		}
	}
	zcbor_print("Found %" PRIuFAST32 " elements.\r\n", max_decode);
	*num_decode = max_decode;
	return true;
}


bool zcbor_present_decode(uint_fast32_t *present,
		zcbor_decoder_t decoder,
		zcbor_state_t *state,
		void *result)
{
	uint_fast32_t num_decode;
	bool retval = zcbor_multi_decode(0, 1, &num_decode, decoder, state, result, 0);

	zcbor_assert_state(retval, "zcbor_multi_decode should not fail with these parameters.\r\n");

	*present = num_decode;
	return retval;
}


void zcbor_new_decode_state(zcbor_state_t *state_array, uint_fast32_t n_states,
		const uint8_t *payload, size_t payload_len, uint_fast32_t elem_count)
{
	zcbor_new_state(state_array, n_states, payload, payload_len, elem_count);
}
