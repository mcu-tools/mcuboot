/*
 * This file has been copied from the zcbor library.
 * Commit zcbor 0.8.1
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
#include <inttypes.h>
#include "zcbor_decode.h"
#include "zcbor_common.h"
#include "zcbor_print.h"


/** Return value length from additional value.
 */
static size_t additional_len(uint8_t additional)
{
	if (additional <= ZCBOR_VALUE_IN_HEADER) {
		return 0;
	} else if (ZCBOR_VALUE_IS_1_BYTE <= additional && additional <= ZCBOR_VALUE_IS_8_BYTES) {
		/* 24 => 1
		 * 25 => 2
		 * 26 => 4
		 * 27 => 8
		 */
		return 1U << (additional - ZCBOR_VALUE_IS_1_BYTE);
	}
	return 0xF;
}


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
	zcbor_major_type_t major_type = ZCBOR_MAJOR_TYPE(*state->payload);

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

static void err_restore(zcbor_state_t *state, int err)
{
	state->payload = state->payload_bak;
	state->elem_count++;
	zcbor_error(state, err);
}

#define ERR_RESTORE(err) \
do { \
	err_restore(state, err); \
	ZCBOR_FAIL(); \
} while(0)

#define FAIL_RESTORE() \
do { \
	state->payload = state->payload_bak; \
	state->elem_count++; \
	ZCBOR_FAIL(); \
} while(0)

#define PRINT_FUNC() zcbor_log("%s ", __func__);


static void endian_copy(uint8_t *dst, const uint8_t *src, size_t src_len)
{
#ifdef ZCBOR_BIG_ENDIAN
	memcpy(dst, src, src_len);
#else
	for (size_t i = 0; i < src_len; i++) {
		dst[i] = src[src_len - 1 - i];
	}
#endif /* ZCBOR_BIG_ENDIAN */
}


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
 *          big to little-endian if necessary (@ref ZCBOR_BIG_ENDIAN).
 */
static bool value_extract(zcbor_state_t *state,
		void *const result, size_t result_len)
{
	zcbor_trace(state, "value_extract");
	zcbor_assert_state(result_len != 0, "0-length result not supported.\r\n");
	zcbor_assert_state(result_len <= 8, "result sizes above 8 bytes not supported.\r\n");
	zcbor_assert_state(result != NULL, "result cannot be NULL.\r\n");

	INITIAL_CHECKS();
	ZCBOR_ERR_IF((state->elem_count == 0), ZCBOR_ERR_LOW_ELEM_COUNT);

	uint8_t additional = ZCBOR_ADDITIONAL(*state->payload);
	size_t len = additional_len(additional);
	uint8_t *result_offs = (uint8_t *)result + ZCBOR_ECPY_OFFS(result_len, MAX(1, len));

	ZCBOR_ERR_IF(additional > ZCBOR_VALUE_IS_8_BYTES, ZCBOR_ERR_ADDITIONAL_INVAL);
	ZCBOR_ERR_IF(len > result_len, ZCBOR_ERR_INT_SIZE);
	ZCBOR_ERR_IF((state->payload + len + 1) > state->payload_end,
		ZCBOR_ERR_NO_PAYLOAD);

	memset(result, 0, result_len);

	if (len == 0) {
		*result_offs = additional;
	} else {
		endian_copy(result_offs, state->payload + 1, len);

#ifdef ZCBOR_CANONICAL
		ZCBOR_ERR_IF((zcbor_header_len_ptr(result, result_len) != (len + 1)),
			ZCBOR_ERR_INVALID_VALUE_ENCODING);
#endif
	}

	state->payload_bak = state->payload;
	(state->payload) += len + 1;
	(state->elem_count)--;
	return true;
}


bool zcbor_int_decode(zcbor_state_t *state, void *result, size_t result_size)
{
	PRINT_FUNC();
	INITIAL_CHECKS();
	zcbor_major_type_t major_type = ZCBOR_MAJOR_TYPE(*state->payload);
	uint8_t *result_uint8 = (uint8_t *)result;
	int8_t *result_int8 = (int8_t *)result;

	if (major_type != ZCBOR_MAJOR_TYPE_PINT
		&& major_type != ZCBOR_MAJOR_TYPE_NINT) {
		/* Value to be read doesn't have the right type. */
		ZCBOR_ERR(ZCBOR_ERR_WRONG_TYPE);
	}

	if (!value_extract(state, result, result_size)) {
		ZCBOR_FAIL();
	}

#ifdef ZCBOR_BIG_ENDIAN
	if (result_int8[0] < 0) {
#else
	if (result_int8[result_size - 1] < 0) {
#endif
		/* Value is too large to fit in a signed integer. */
		ERR_RESTORE(ZCBOR_ERR_INT_SIZE);
	}

	if (major_type == ZCBOR_MAJOR_TYPE_NINT) {
		/* Convert from CBOR's representation by flipping all bits. */
		for (unsigned int i = 0; i < result_size; i++) {
			result_uint8[i] = (uint8_t)~result_uint8[i];
		}
	}

	return true;
}


bool zcbor_int32_decode(zcbor_state_t *state, int32_t *result)
{
	PRINT_FUNC();
	return zcbor_int_decode(state, result, sizeof(*result));
}


bool zcbor_int64_decode(zcbor_state_t *state, int64_t *result)
{
	PRINT_FUNC();
	return zcbor_int_decode(state, result, sizeof(*result));
}


bool zcbor_uint_decode(zcbor_state_t *state, void *result, size_t result_size)
{
	PRINT_FUNC();
	INITIAL_CHECKS_WITH_TYPE(ZCBOR_MAJOR_TYPE_PINT);

	if (!value_extract(state, result, result_size)) {
		zcbor_log("uint with size %zu failed.\r\n", result_size);
		ZCBOR_FAIL();
	}
	return true;
}


bool zcbor_uint32_decode(zcbor_state_t *state, uint32_t *result)
{
	PRINT_FUNC();
	return zcbor_uint_decode(state, result, sizeof(*result));
}


bool zcbor_int32_expect_union(zcbor_state_t *state, int32_t result)
{
	PRINT_FUNC();
	if (!zcbor_union_elem_code(state)) {
		ZCBOR_FAIL();
	}
	return zcbor_int32_expect(state, result);
}


bool zcbor_int64_expect_union(zcbor_state_t *state, int64_t result)
{
	PRINT_FUNC();
	if (!zcbor_union_elem_code(state)) {
		ZCBOR_FAIL();
	}
	return zcbor_int64_expect(state, result);
}


bool zcbor_uint32_expect_union(zcbor_state_t *state, uint32_t result)
{
	PRINT_FUNC();
	if (!zcbor_union_elem_code(state)) {
		ZCBOR_FAIL();
	}
	return zcbor_uint32_expect(state, result);
}


bool zcbor_uint64_expect_union(zcbor_state_t *state, uint64_t result)
{
	PRINT_FUNC();
	if (!zcbor_union_elem_code(state)) {
		ZCBOR_FAIL();
	}
	return zcbor_uint64_expect(state, result);
}


bool zcbor_int32_expect(zcbor_state_t *state, int32_t expected)
{
	PRINT_FUNC();
	return zcbor_int64_expect(state, expected);
}


bool zcbor_int32_pexpect(zcbor_state_t *state, int32_t *expected)
{
	PRINT_FUNC();
	return zcbor_int32_expect(state, *expected);
}


bool zcbor_int64_expect(zcbor_state_t *state, int64_t expected)
{
	PRINT_FUNC();
	int64_t actual;

	if (!zcbor_int64_decode(state, &actual)) {
		ZCBOR_FAIL();
	}

	if (actual != expected) {
		zcbor_log("%" PRIi64 " != %" PRIi64 "\r\n", actual, expected);
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_int64_pexpect(zcbor_state_t *state, int64_t *expected)
{
	PRINT_FUNC();
	return zcbor_int64_expect(state, *expected);
}


bool zcbor_uint64_decode(zcbor_state_t *state, uint64_t *result)
{
	PRINT_FUNC();
	return zcbor_uint_decode(state, result, sizeof(*result));
}


#ifdef ZCBOR_SUPPORTS_SIZE_T
bool zcbor_size_decode(zcbor_state_t *state, size_t *result)
{
	PRINT_FUNC();
	return zcbor_uint_decode(state, result, sizeof(*result));
}
#endif


bool zcbor_uint32_expect(zcbor_state_t *state, uint32_t expected)
{
	PRINT_FUNC();
	return zcbor_uint64_expect(state, expected);
}


bool zcbor_uint32_pexpect(zcbor_state_t *state, uint32_t *expected)
{
	PRINT_FUNC();
	return zcbor_uint32_expect(state, *expected);
}


bool zcbor_uint64_expect(zcbor_state_t *state, uint64_t expected)
{
	PRINT_FUNC();
	uint64_t actual;

	if (!zcbor_uint64_decode(state, &actual)) {
		ZCBOR_FAIL();
	}
	if (actual != expected) {
		zcbor_log("%" PRIu64 " != %" PRIu64 "\r\n", actual, expected);
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_uint64_pexpect(zcbor_state_t *state, uint64_t *expected)
{
	PRINT_FUNC();
	return zcbor_uint64_expect(state, *expected);
}


#ifdef ZCBOR_SUPPORTS_SIZE_T
bool zcbor_size_expect(zcbor_state_t *state, size_t expected)
{
	PRINT_FUNC();
	return zcbor_uint64_expect(state, expected);
}


bool zcbor_size_pexpect(zcbor_state_t *state, size_t *expected)
{
	PRINT_FUNC();
	return zcbor_size_expect(state, *expected);
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

static bool str_start_decode_with_overflow_check(zcbor_state_t *state,
		struct zcbor_string *result, zcbor_major_type_t exp_major_type)
{
	bool res = str_start_decode(state, result, exp_major_type);

	if (!res) {
		ZCBOR_FAIL();
	}

	/* Casting to size_t is safe since str_start_decode() checks that
	 * payload_end is bigger that payload. */
	if (result->len > (size_t)(state->payload_end - state->payload)) {
		zcbor_log("error: 0x%zu > 0x%zu\r\n",
			result->len,
			(state->payload_end - state->payload));
		ERR_RESTORE(ZCBOR_ERR_NO_PAYLOAD);
	}

	return true;
}


bool zcbor_bstr_start_decode(zcbor_state_t *state, struct zcbor_string *result)
{
	PRINT_FUNC();
	struct zcbor_string dummy;
	if (result == NULL) {
		result = &dummy;
	}

	if(!str_start_decode_with_overflow_check(state, result, ZCBOR_MAJOR_TYPE_BSTR)) {
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
			ZCBOR_FLAG_RESTORE | ZCBOR_FLAG_CONSUME | ZCBOR_FLAG_KEEP_PAYLOAD,
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
	PRINT_FUNC();
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
	PRINT_FUNC();
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
	zcbor_log("New fragment length %zu\r\n", result->fragment.len);

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
	zcbor_log("fragment length %zu\r\n", result->fragment.len);
	state->payload_end = state->payload + result->fragment.len;
}


bool zcbor_is_last_fragment(const struct zcbor_string_fragment *fragment)
{
	return (fragment->total_len == (fragment->offset + fragment->fragment.len));
}


static bool str_decode(zcbor_state_t *state, struct zcbor_string *result,
		zcbor_major_type_t exp_major_type)
{
	if (!str_start_decode_with_overflow_check(state, result, exp_major_type)) {
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
	if (!zcbor_compare_strings(&tmp_result, result)) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_bstr_decode(zcbor_state_t *state, struct zcbor_string *result)
{
	PRINT_FUNC();
	return str_decode(state, result, ZCBOR_MAJOR_TYPE_BSTR);
}


bool zcbor_bstr_decode_fragment(zcbor_state_t *state, struct zcbor_string_fragment *result)
{
	PRINT_FUNC();
	return str_decode_fragment(state, result, ZCBOR_MAJOR_TYPE_BSTR);
}


bool zcbor_bstr_expect(zcbor_state_t *state, struct zcbor_string *expected)
{
	PRINT_FUNC();
	return str_expect(state, expected, ZCBOR_MAJOR_TYPE_BSTR);
}


bool zcbor_tstr_decode(zcbor_state_t *state, struct zcbor_string *result)
{
	PRINT_FUNC();
	return str_decode(state, result, ZCBOR_MAJOR_TYPE_TSTR);
}


bool zcbor_tstr_decode_fragment(zcbor_state_t *state, struct zcbor_string_fragment *result)
{
	PRINT_FUNC();
	return str_decode_fragment(state, result, ZCBOR_MAJOR_TYPE_TSTR);
}


bool zcbor_tstr_expect(zcbor_state_t *state, struct zcbor_string *expected)
{
	PRINT_FUNC();
	return str_expect(state, expected, ZCBOR_MAJOR_TYPE_TSTR);
}


bool zcbor_bstr_expect_ptr(zcbor_state_t *state, char const *ptr, size_t len)
{
	PRINT_FUNC();
	struct zcbor_string zs = { .value = (const uint8_t *)ptr, .len = len };

	return zcbor_bstr_expect(state, &zs);
}


bool zcbor_tstr_expect_ptr(zcbor_state_t *state, char const *ptr, size_t len)
{
	PRINT_FUNC();
	struct zcbor_string zs = { .value = (const uint8_t *)ptr, .len = len };

	return zcbor_tstr_expect(state, &zs);
}


bool zcbor_bstr_expect_term(zcbor_state_t *state, char const *string, size_t maxlen)
{
	PRINT_FUNC();
	return zcbor_bstr_expect_ptr(state, string, strnlen(string, maxlen));
}


bool zcbor_tstr_expect_term(zcbor_state_t *state, char const *string, size_t maxlen)
{
	PRINT_FUNC();
	return zcbor_tstr_expect_ptr(state, string, strnlen(string, maxlen));
}


static bool list_map_start_decode(zcbor_state_t *state,
		zcbor_major_type_t exp_major_type)
{
	size_t new_elem_count;
	bool indefinite_length_array = false;

	INITIAL_CHECKS_WITH_TYPE(exp_major_type);

#ifndef ZCBOR_CANONICAL
	if (ZCBOR_ADDITIONAL(*state->payload) == ZCBOR_VALUE_IS_INDEFINITE_LENGTH) {
		/* Indefinite length array. */
		new_elem_count = ZCBOR_LARGE_ELEM_COUNT;
		ZCBOR_ERR_IF(state->elem_count == 0, ZCBOR_ERR_LOW_ELEM_COUNT);
		indefinite_length_array = true;
		state->payload_bak = state->payload++;
		state->elem_count--;
	} else
#endif
	{
		if (!value_extract(state, &new_elem_count, sizeof(new_elem_count))) {
			ZCBOR_FAIL();
		}
	}

	if (!zcbor_new_backup(state, new_elem_count)) {
		FAIL_RESTORE();
	}

	state->decode_state.indefinite_length_array = indefinite_length_array;

	return true;
}


bool zcbor_list_start_decode(zcbor_state_t *state)
{
	PRINT_FUNC();
	return list_map_start_decode(state, ZCBOR_MAJOR_TYPE_LIST);
}


bool zcbor_map_start_decode(zcbor_state_t *state)
{
	PRINT_FUNC();
	bool ret = list_map_start_decode(state, ZCBOR_MAJOR_TYPE_MAP);

	if (ret && !state->decode_state.indefinite_length_array) {
		if (state->elem_count >= (ZCBOR_MAX_ELEM_COUNT / 2)) {
			/* The new elem_count is too large. */
			ERR_RESTORE(ZCBOR_ERR_INT_SIZE);
		}
		state->elem_count *= 2;
	}
	return ret;
}


bool zcbor_array_at_end(zcbor_state_t *state)
{
#ifdef ZCBOR_CANONICAL
	const bool indefinite_length_array = false;
#else
	const bool indefinite_length_array = state->decode_state.indefinite_length_array;
#endif
	return ((!indefinite_length_array && (state->elem_count == 0))
		|| (indefinite_length_array
			&& (state->payload < state->payload_end)
			&& (*state->payload == 0xFF)));
}


static size_t update_map_elem_count(zcbor_state_t *state, size_t elem_count);
#ifdef ZCBOR_MAP_SMART_SEARCH
static bool allocate_map_flags(zcbor_state_t *state, size_t elem_count);
#endif


bool zcbor_unordered_map_start_decode(zcbor_state_t *state)
{
	PRINT_FUNC();
	ZCBOR_FAIL_IF(!zcbor_map_start_decode(state));

#ifdef ZCBOR_MAP_SMART_SEARCH
	state->decode_state.map_search_elem_state
		+= zcbor_flags_to_bytes(state->decode_state.map_elem_count);
#else
	state->decode_state.map_elems_processed = 0;
#endif
	state->decode_state.map_elem_count = 0;
	state->decode_state.counting_map_elems = state->decode_state.indefinite_length_array;

	if (!state->decode_state.counting_map_elems) {
		size_t old_flags = update_map_elem_count(state, state->elem_count);
#ifdef ZCBOR_MAP_SMART_SEARCH
		ZCBOR_FAIL_IF(!allocate_map_flags(state, old_flags));
#endif
		(void)old_flags;
	}

	return true;
}


/** Return the max (starting) elem_count of the current container.
 *
 *  Should only be used for unordered maps (started with @ref zcbor_unordered_map_start_decode)
 */
static size_t zcbor_current_max_elem_count(zcbor_state_t *state)
{
	return (state->decode_state.indefinite_length_array ? \
		ZCBOR_LARGE_ELEM_COUNT : state->decode_state.map_elem_count * 2);
}


static bool map_restart(zcbor_state_t *state)
{
	if (!zcbor_process_backup(state, ZCBOR_FLAG_RESTORE | ZCBOR_FLAG_KEEP_DECODE_STATE,
				ZCBOR_MAX_ELEM_COUNT)) {
		ZCBOR_FAIL();
	}

	state->elem_count = zcbor_current_max_elem_count(state);
	return true;
}


__attribute__((used))
static size_t get_current_index(zcbor_state_t *state, uint32_t index_offset)
{
	/* Subtract mode because for GET, you want the index you are pointing to, while for SET,
	 * you want the one you just processed. This only comes into play when elem_count is even. */
	return ((zcbor_current_max_elem_count(state) - state->elem_count - index_offset) / 2);
}


#ifdef ZCBOR_MAP_SMART_SEARCH
#define FLAG_MODE_GET_CURRENT 0
#define FLAG_MODE_CLEAR_CURRENT 1
#define FLAG_MODE_CLEAR_UNUSED 2

static bool manipulate_flags(zcbor_state_t *state, uint32_t mode)
{
	const size_t last_index = (state->decode_state.map_elem_count - 1);
	size_t index = (mode == FLAG_MODE_CLEAR_UNUSED) ? last_index : get_current_index(state, mode);

	ZCBOR_ERR_IF((index >= state->decode_state.map_elem_count),
		ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE);
	uint8_t *flag_byte = &state->decode_state.map_search_elem_state[index >> 3];
	uint8_t flag_mask = (uint8_t)(1 << (index & 7));

	switch(mode) {
	case FLAG_MODE_GET_CURRENT:
		return (!!(*flag_byte & flag_mask));
	case FLAG_MODE_CLEAR_CURRENT:
		*flag_byte &= ~flag_mask;
		return true;
	case FLAG_MODE_CLEAR_UNUSED:
		*flag_byte &= (uint8_t)((flag_mask << 1) - 1);
		return true;
	}
	return false;
}


static bool should_try_key(zcbor_state_t *state)
{
	return manipulate_flags(state, FLAG_MODE_GET_CURRENT);
}


bool zcbor_elem_processed(zcbor_state_t *state)
{
	return manipulate_flags(state, FLAG_MODE_CLEAR_CURRENT);
}


static bool allocate_map_flags(zcbor_state_t *state, size_t old_flags)
{
	size_t new_bytes = zcbor_flags_to_bytes(state->decode_state.map_elem_count);
	size_t old_bytes = zcbor_flags_to_bytes(old_flags);
	size_t extra_bytes = new_bytes - old_bytes;
	const uint8_t *flags_end = state->constant_state->map_search_elem_state_end;

	if (extra_bytes) {
		if ((state->decode_state.map_search_elem_state + new_bytes) > flags_end) {
			state->decode_state.map_elem_count
				= 8 * (size_t)(flags_end - state->decode_state.map_search_elem_state);
			ZCBOR_ERR(ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE);
		}

		memset(&state->decode_state.map_search_elem_state[new_bytes - extra_bytes], 0xFF, extra_bytes);
	}
	return true;
}
#else

static bool should_try_key(zcbor_state_t *state)
{
	return (state->decode_state.map_elems_processed < state->decode_state.map_elem_count);
}


bool zcbor_elem_processed(zcbor_state_t *state)
{
	if (should_try_key(state)) {
		state->decode_state.map_elems_processed++;
	}
	return true;
}
#endif


static size_t update_map_elem_count(zcbor_state_t *state, size_t elem_count)
{
	size_t old_map_elem_count = state->decode_state.map_elem_count;

	state->decode_state.map_elem_count = MAX(old_map_elem_count, elem_count / 2);
	return old_map_elem_count;
}


static bool handle_map_end(zcbor_state_t *state)
{
	state->decode_state.counting_map_elems = false;
	return map_restart(state);
}


static bool try_key(zcbor_state_t *state, void *key_result, zcbor_decoder_t key_decoder)
{
	uint8_t const *payload_bak2 = state->payload;
	size_t elem_count_bak = state->elem_count;

	if (!key_decoder(state, (uint8_t *)key_result)) {
		state->payload = payload_bak2;
		state->elem_count = elem_count_bak;
		return false;
	}

	zcbor_log("Found element at index %zu.\n", get_current_index(state, 1));
	return true;
}


bool zcbor_unordered_map_search(zcbor_decoder_t key_decoder, zcbor_state_t *state, void *key_result)
{
	PRINT_FUNC();
	/* elem_count cannot be odd since the map consists of key-value-pairs.
	 * This might mean that this function was called while pointing at a value (instead
	 * of a key). */
	ZCBOR_ERR_IF(state->elem_count & 1, ZCBOR_ERR_MAP_MISALIGNED);

	uint8_t const *payload_bak = state->payload;
	size_t elem_count = state->elem_count;

	/* Loop once through all the elements of the map. */
	do {
		if (zcbor_array_at_end(state)) {
			if (!handle_map_end(state)) {
				goto error;
			}
			continue; /* This continue is needed so the loop stops both if elem_count is
			           * at the very start or the very end of the map. */
		}

		if (state->decode_state.counting_map_elems) {
			size_t m_elem_count = ZCBOR_LARGE_ELEM_COUNT - state->elem_count + 2;
			size_t old_flags = update_map_elem_count(state, m_elem_count);
#ifdef ZCBOR_MAP_SMART_SEARCH
			ZCBOR_FAIL_IF(!allocate_map_flags(state, old_flags));
#endif
			(void)old_flags;
		}

		if (should_try_key(state) && try_key(state, key_result, key_decoder)) {
			if (!state->constant_state->manually_process_elem) {
				ZCBOR_FAIL_IF(!zcbor_elem_processed(state));
			}
			return true;
		}

		/* Skip over both the key and the value. */
		if (!zcbor_any_skip(state, NULL) || !zcbor_any_skip(state, NULL)) {
			goto error;
		}
	} while (state->elem_count != elem_count);

	zcbor_error(state, ZCBOR_ERR_ELEM_NOT_FOUND);
error:
	state->payload = payload_bak;
	state->elem_count = elem_count;
	ZCBOR_FAIL();
}


bool zcbor_search_key_bstr_ptr(zcbor_state_t *state, char const *ptr, size_t len)
{
	struct zcbor_string zs = { .value = (const uint8_t *)ptr, .len = len };

	return zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_bstr_expect, state, &zs);
}


bool zcbor_search_key_tstr_ptr(zcbor_state_t *state, char const *ptr, size_t len)
{
	struct zcbor_string zs = { .value = (const uint8_t *)ptr, .len = len };

	return zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_tstr_expect, state, &zs);
}


bool zcbor_search_key_bstr_term(zcbor_state_t *state, char const *str, size_t maxlen)
{
	return zcbor_search_key_bstr_ptr(state, str, strnlen(str, maxlen));
}


bool zcbor_search_key_tstr_term(zcbor_state_t *state, char const *str, size_t maxlen)
{
	return zcbor_search_key_tstr_ptr(state, str, strnlen(str, maxlen));
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
	size_t max_elem_count = 0;

#ifndef ZCBOR_CANONICAL
	if (state->decode_state.indefinite_length_array) {
		if (!array_end_expect(state)) {
			ZCBOR_FAIL();
		}
		max_elem_count = ZCBOR_MAX_ELEM_COUNT;
		state->decode_state.indefinite_length_array = false;
	}
#endif
	if (!zcbor_process_backup(state,
			ZCBOR_FLAG_RESTORE | ZCBOR_FLAG_CONSUME | ZCBOR_FLAG_KEEP_PAYLOAD,
			max_elem_count)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_list_end_decode(zcbor_state_t *state)
{
	PRINT_FUNC();
	return list_map_end_decode(state);
}


bool zcbor_map_end_decode(zcbor_state_t *state)
{
	PRINT_FUNC();
	return list_map_end_decode(state);
}


bool zcbor_unordered_map_end_decode(zcbor_state_t *state)
{
	/* Checking zcbor_array_at_end() ensures that check is valid.
	 * In case the map is at the end, but state->decode_state.counting_map_elems isn't updated.*/
	ZCBOR_ERR_IF(!zcbor_array_at_end(state) && state->decode_state.counting_map_elems,
			ZCBOR_ERR_ELEMS_NOT_PROCESSED);

	if (state->decode_state.map_elem_count > 0) {
#ifdef ZCBOR_MAP_SMART_SEARCH
		manipulate_flags(state, FLAG_MODE_CLEAR_UNUSED);

		for (size_t i = 0; i < zcbor_flags_to_bytes(state->decode_state.map_elem_count); i++) {
			if (state->decode_state.map_search_elem_state[i] != 0) {
				zcbor_log("unprocessed element(s) in map: [%zu] = 0x%02x\n",
						i, state->decode_state.map_search_elem_state[i]);
				ZCBOR_ERR(ZCBOR_ERR_ELEMS_NOT_PROCESSED);
			}
		}
#else
		ZCBOR_ERR_IF(should_try_key(state), ZCBOR_ERR_ELEMS_NOT_PROCESSED);
#endif
	}
	while (!zcbor_array_at_end(state)) {
		zcbor_any_skip(state, NULL);
	}
	return zcbor_map_end_decode(state);
}


bool zcbor_list_map_end_force_decode(zcbor_state_t *state)
{
	if (!zcbor_process_backup(state,
			ZCBOR_FLAG_RESTORE | ZCBOR_FLAG_CONSUME | ZCBOR_FLAG_KEEP_PAYLOAD,
			ZCBOR_MAX_ELEM_COUNT)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_simple_decode(zcbor_state_t *state, uint8_t *result)
{
	PRINT_FUNC();
	PRINT_FUNC();
	INITIAL_CHECKS_WITH_TYPE(ZCBOR_MAJOR_TYPE_SIMPLE);

	/* Simple values must be 0-23 (additional is 0-23) or 24-255 (additional is 24).
	 * Other additional values are not considered simple values. */
	ZCBOR_ERR_IF(ZCBOR_ADDITIONAL(*state->payload) > 24, ZCBOR_ERR_WRONG_TYPE);

	if (!value_extract(state, result, sizeof(*result))) {
		ZCBOR_FAIL();
	}
	return true;
}


bool zcbor_simple_expect(zcbor_state_t *state, uint8_t expected)
{
	PRINT_FUNC();
	uint8_t actual;

	if (!zcbor_simple_decode(state, &actual)) {
		ZCBOR_FAIL();
	}

	if (actual != expected) {
		zcbor_log("simple value %u != %u\r\n", actual, expected);
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}

	return true;
}


bool zcbor_simple_pexpect(zcbor_state_t *state, uint8_t *expected)
{
	PRINT_FUNC();
	return zcbor_simple_expect(state, *expected);
}


bool zcbor_nil_expect(zcbor_state_t *state, void *unused)
{
	PRINT_FUNC();
	(void)unused;
	return zcbor_simple_expect(state, 22);
}


bool zcbor_undefined_expect(zcbor_state_t *state, void *unused)
{
	PRINT_FUNC();
	(void)unused;
	return zcbor_simple_expect(state, 23);
}


bool zcbor_bool_decode(zcbor_state_t *state, bool *result)
{
	PRINT_FUNC();
	uint8_t value;

	if (!zcbor_simple_decode(state, &value)) {
		ZCBOR_FAIL();
	}
	value -= ZCBOR_BOOL_TO_SIMPLE;
	if (value > 1) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_TYPE);
	}
	*result = value;

	zcbor_log("boolval: %u\r\n", *result);
	return true;
}


bool zcbor_bool_expect(zcbor_state_t *state, bool expected)
{
	PRINT_FUNC();
	return zcbor_simple_expect(state, (uint8_t)(!!expected) + ZCBOR_BOOL_TO_SIMPLE);
}


bool zcbor_bool_pexpect(zcbor_state_t *state, bool *expected)
{
	PRINT_FUNC();
	return zcbor_bool_expect(state, *expected);
}


static bool float_check(zcbor_state_t *state, uint8_t additional_val)
{
	INITIAL_CHECKS_WITH_TYPE(ZCBOR_MAJOR_TYPE_SIMPLE);
	ZCBOR_ERR_IF(ZCBOR_ADDITIONAL(*state->payload) != additional_val, ZCBOR_ERR_FLOAT_SIZE);
	return true;
}


bool zcbor_float16_bytes_decode(zcbor_state_t *state, uint16_t *result)
{
	PRINT_FUNC();
	ZCBOR_FAIL_IF(!float_check(state, ZCBOR_VALUE_IS_2_BYTES));

	if (!value_extract(state, result, sizeof(*result))) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float16_bytes_expect(zcbor_state_t *state, uint16_t expected)
{
	PRINT_FUNC();
	uint16_t actual;

	if (!zcbor_float16_bytes_decode(state, &actual)) {
		ZCBOR_FAIL();
	}
	if (actual != expected) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_float16_bytes_pexpect(zcbor_state_t *state, uint16_t *expected)
{
	PRINT_FUNC();
	return zcbor_float16_bytes_expect(state, *expected);
}


bool zcbor_float16_decode(zcbor_state_t *state, float *result)
{
	PRINT_FUNC();
	uint16_t value16;

	if (!zcbor_float16_bytes_decode(state, &value16)) {
		ZCBOR_FAIL();
	}

	*result = zcbor_float16_to_32(value16);
	return true;
}


bool zcbor_float16_expect(zcbor_state_t *state, float expected)
{
	PRINT_FUNC();
	float actual;

	if (!zcbor_float16_decode(state, &actual)) {
		ZCBOR_FAIL();
	}
	if (actual != expected) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_float16_pexpect(zcbor_state_t *state, float *expected)
{
	PRINT_FUNC();
	return zcbor_float16_expect(state, *expected);
}


bool zcbor_float32_decode(zcbor_state_t *state, float *result)
{
	PRINT_FUNC();
	ZCBOR_FAIL_IF(!float_check(state, ZCBOR_VALUE_IS_4_BYTES));

	if (!value_extract(state, result, sizeof(*result))) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float32_expect(zcbor_state_t *state, float expected)
{
	PRINT_FUNC();
	float actual;

	if (!zcbor_float32_decode(state, &actual)) {
		ZCBOR_FAIL();
	}
	if (actual != expected) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_float32_pexpect(zcbor_state_t *state, float *expected)
{
	PRINT_FUNC();
	return zcbor_float32_expect(state, *expected);
}


bool zcbor_float16_32_decode(zcbor_state_t *state, float *result)
{
	PRINT_FUNC();
	if (zcbor_float16_decode(state, result)) {
		/* Do nothing */
	} else if (!zcbor_float32_decode(state, result)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float16_32_expect(zcbor_state_t *state, float expected)
{
	PRINT_FUNC();
	if (zcbor_float16_expect(state, expected)) {
		/* Do nothing */
	} else if (!zcbor_float32_expect(state, expected)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float16_32_pexpect(zcbor_state_t *state, float *expected)
{
	PRINT_FUNC();
	return zcbor_float16_32_expect(state, *expected);
}


bool zcbor_float64_decode(zcbor_state_t *state, double *result)
{
	PRINT_FUNC();
	ZCBOR_FAIL_IF(!float_check(state, ZCBOR_VALUE_IS_8_BYTES));

	if (!value_extract(state, result, sizeof(*result))) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float64_expect(zcbor_state_t *state, double expected)
{
	PRINT_FUNC();
	double actual;

	if (!zcbor_float64_decode(state, &actual)) {
		ZCBOR_FAIL();
	}
	if (actual != expected) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_float64_pexpect(zcbor_state_t *state, double *expected)
{
	PRINT_FUNC();
	return zcbor_float64_expect(state, *expected);
}


bool zcbor_float32_64_decode(zcbor_state_t *state, double *result)
{
	PRINT_FUNC();
	float float_result;

	if (zcbor_float32_decode(state, &float_result)) {
		*result = (double)float_result;
	} else if (!zcbor_float64_decode(state, result)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float32_64_expect(zcbor_state_t *state, double expected)
{
	PRINT_FUNC();
	if (zcbor_float64_expect(state, expected)) {
		/* Do nothing */
	} else if (!zcbor_float32_expect(state, (float)expected)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float32_64_pexpect(zcbor_state_t *state, double *expected)
{
	PRINT_FUNC();
	return zcbor_float32_64_expect(state, *expected);
}


bool zcbor_float_decode(zcbor_state_t *state, double *result)
{
	PRINT_FUNC();
	float float_result;

	if (zcbor_float16_decode(state, &float_result)) {
		*result = (double)float_result;
	} else if (zcbor_float32_decode(state, &float_result)) {
		*result = (double)float_result;
	} else if (!zcbor_float64_decode(state, result)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float_expect(zcbor_state_t *state, double expected)
{
	PRINT_FUNC();
	if (zcbor_float16_expect(state, (float)expected)) {
		/* Do nothing */
	} else if (zcbor_float32_expect(state, (float)expected)) {
		/* Do nothing */
	} else if (!zcbor_float64_expect(state, expected)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float_pexpect(zcbor_state_t *state, double *expected)
{
	PRINT_FUNC();
	return zcbor_float_expect(state, *expected);
}


bool zcbor_any_skip(zcbor_state_t *state, void *result)
{
	PRINT_FUNC();
	zcbor_assert_state(result == NULL,
			"'any' type cannot be returned, only skipped.\r\n");
	(void)result;

	INITIAL_CHECKS();
	zcbor_major_type_t major_type = ZCBOR_MAJOR_TYPE(*state->payload);
	uint8_t additional = ZCBOR_ADDITIONAL(*state->payload);
	uint64_t value = 0; /* In case of indefinite_length_array. */
	zcbor_state_t state_copy;

	memcpy(&state_copy, state, sizeof(zcbor_state_t));

	while (major_type == ZCBOR_MAJOR_TYPE_TAG) {
		uint32_t tag_dummy;

		if (!zcbor_tag_decode(&state_copy, &tag_dummy)) {
			ZCBOR_FAIL();
		}
		ZCBOR_ERR_IF(state_copy.payload >= state_copy.payload_end, ZCBOR_ERR_NO_PAYLOAD);
		major_type = ZCBOR_MAJOR_TYPE(*state_copy.payload);
		additional = ZCBOR_ADDITIONAL(*state_copy.payload);
	}

#ifdef ZCBOR_CANONICAL
	const bool indefinite_length_array = false;
#else
	const bool indefinite_length_array = ((additional == ZCBOR_VALUE_IS_INDEFINITE_LENGTH)
		&& ((major_type == ZCBOR_MAJOR_TYPE_LIST) || (major_type == ZCBOR_MAJOR_TYPE_MAP)));
#endif

	if (!indefinite_length_array && !value_extract(&state_copy, &value, sizeof(value))) {
		/* Can happen because of elem_count (or payload_end) */
		ZCBOR_FAIL();
	}

	switch (major_type) {
		case ZCBOR_MAJOR_TYPE_BSTR:
		case ZCBOR_MAJOR_TYPE_TSTR:
			/* 'value' is the length of the BSTR or TSTR.
			 * The subtraction is safe because value_extract() above
			 * checks that payload_end is greater than payload. */
			ZCBOR_ERR_IF(
				value > (uint64_t)(state_copy.payload_end - state_copy.payload),
				ZCBOR_ERR_NO_PAYLOAD);
			(state_copy.payload) += value;
			break;
		case ZCBOR_MAJOR_TYPE_MAP:
			ZCBOR_ERR_IF(value > (SIZE_MAX / 2), ZCBOR_ERR_INT_SIZE);
			value *= 2;
			/* fallthrough */
		case ZCBOR_MAJOR_TYPE_LIST:
			if (indefinite_length_array) {
				state_copy.payload++;
				value = ZCBOR_LARGE_ELEM_COUNT;
			}
			state_copy.elem_count = (size_t)value;
			state_copy.decode_state.indefinite_length_array = indefinite_length_array;
			while (!zcbor_array_at_end(&state_copy)) {
				if (!zcbor_any_skip(&state_copy, NULL)) {
					ZCBOR_FAIL();
				}
			}
			if (indefinite_length_array && !array_end_expect(&state_copy)) {
				ZCBOR_FAIL();
			}
			break;
		default:
			/* Do nothing */
			break;
	}

	state->payload = state_copy.payload;
	state->elem_count--;

	return true;
}


bool zcbor_tag_decode(zcbor_state_t *state, uint32_t *result)
{
	PRINT_FUNC();
	INITIAL_CHECKS_WITH_TYPE(ZCBOR_MAJOR_TYPE_TAG);

	if (!value_extract(state, result, sizeof(*result))) {
		ZCBOR_FAIL();
	}
	state->elem_count++;
	return true;
}


bool zcbor_tag_expect(zcbor_state_t *state, uint32_t expected)
{
	PRINT_FUNC();
	uint32_t actual;

	if (!zcbor_tag_decode(state, &actual)) {
		ZCBOR_FAIL();
	}
	if (actual != expected) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_tag_pexpect(zcbor_state_t *state, uint32_t *expected)
{
	PRINT_FUNC();
	return zcbor_tag_expect(state, *expected);
}


bool zcbor_multi_decode(size_t min_decode,
		size_t max_decode,
		size_t *num_decode,
		zcbor_decoder_t decoder,
		zcbor_state_t *state,
		void *result,
		size_t result_len)
{
	PRINT_FUNC();
	ZCBOR_CHECK_ERROR();
	for (size_t i = 0; i < max_decode; i++) {
		uint8_t const *payload_bak = state->payload;
		size_t elem_count_bak = state->elem_count;

		if (!decoder(state,
				(uint8_t *)result + i*result_len)) {
			*num_decode = i;
			state->payload = payload_bak;
			state->elem_count = elem_count_bak;
			ZCBOR_ERR_IF(i < min_decode, ZCBOR_ERR_ITERATIONS);
			zcbor_log("Found %zu elements.\r\n", i);
			return true;
		}
	}
	zcbor_log("Found %zu elements.\r\n", max_decode);
	*num_decode = max_decode;
	return true;
}


bool zcbor_present_decode(bool *present,
		zcbor_decoder_t decoder,
		zcbor_state_t *state,
		void *result)
{
	PRINT_FUNC();
	size_t num_decode = 0;
	bool retval = zcbor_multi_decode(0, 1, &num_decode, decoder, state, result, 0);

	zcbor_assert_state(retval, "zcbor_multi_decode should not fail with these parameters.\r\n");

	*present = !!num_decode;
	return retval;
}


void zcbor_new_decode_state(zcbor_state_t *state_array, size_t n_states,
		const uint8_t *payload, size_t payload_len, size_t elem_count,
		uint8_t *flags, size_t flags_bytes)
{
	zcbor_new_state(state_array, n_states, payload, payload_len, elem_count, flags, flags_bytes);
}
