/*
 * This file has been copied from the zcbor library.
 * Commit zcbor 0.8.1
 */

/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "zcbor_common.h"
#include "zcbor_print.h"

_Static_assert((sizeof(size_t) == sizeof(void *)),
	"This code needs size_t to be the same length as pointers.");

_Static_assert((sizeof(zcbor_state_t) >= sizeof(struct zcbor_state_constant)),
	"This code needs zcbor_state_t to be at least as large as zcbor_backups_t.");

bool zcbor_new_backup(zcbor_state_t *state, size_t new_elem_count)
{
	ZCBOR_CHECK_ERROR();

	if ((state->constant_state->current_backup)
		>= state->constant_state->num_backups) {
		ZCBOR_ERR(ZCBOR_ERR_NO_BACKUP_MEM);
	}

	state->payload_moved = false;

	(state->constant_state->current_backup)++;

	/* use the backup at current_backup - 1, since otherwise, the 0th
	 * backup would be unused. */
	size_t i = (state->constant_state->current_backup) - 1;

	memcpy(&state->constant_state->backup_list[i], state,
		sizeof(zcbor_state_t));

	state->elem_count = new_elem_count;

	zcbor_log("New backup (level %zu)\n", i);

	return true;
}


bool zcbor_process_backup(zcbor_state_t *state, uint32_t flags,
		size_t max_elem_count)
{
	ZCBOR_CHECK_ERROR();
	zcbor_state_t local_copy = *state;

	if (state->constant_state->current_backup == 0) {
		zcbor_log("No backups available.\r\n");
		ZCBOR_ERR(ZCBOR_ERR_NO_BACKUP_ACTIVE);
	}

	/* use the backup at current_backup - 1, since otherwise, the
		* 0th backup would be unused. */
	size_t i = state->constant_state->current_backup - 1;

	zcbor_log("Process backup (level %zu, flags 0x%x)\n", i, flags);

	if (flags & ZCBOR_FLAG_RESTORE) {
		if (!(flags & ZCBOR_FLAG_KEEP_PAYLOAD)) {
			if (state->constant_state->backup_list[i].payload_moved) {
				zcbor_log("Payload pointer out of date.\r\n");
				ZCBOR_ERR(ZCBOR_ERR_PAYLOAD_OUTDATED);
			}
		}
		memcpy(state, &state->constant_state->backup_list[i],
			sizeof(zcbor_state_t));
	}

	if (flags & ZCBOR_FLAG_CONSUME) {
		state->constant_state->current_backup--;
	}

	if (local_copy.elem_count > max_elem_count) {
		zcbor_log("elem_count: %zu (expected max %zu)\r\n",
			local_copy.elem_count, max_elem_count);
		ZCBOR_ERR(ZCBOR_ERR_HIGH_ELEM_COUNT);
	}

	if (flags & ZCBOR_FLAG_KEEP_PAYLOAD) {
		state->payload = local_copy.payload;
	}

	if (flags & ZCBOR_FLAG_KEEP_DECODE_STATE) {
		/* Copy decode state */
		state->decode_state = local_copy.decode_state;
	}

	return true;
}

static void update_backups(zcbor_state_t *state, uint8_t const *new_payload_end)
{
	if (state->constant_state) {
		for (unsigned int i = 0; i < state->constant_state->current_backup; i++) {
			state->constant_state->backup_list[i].payload_end = new_payload_end;
			state->constant_state->backup_list[i].payload_moved = true;
		}
	}
}


bool zcbor_union_start_code(zcbor_state_t *state)
{
	if (!zcbor_new_backup(state, state->elem_count)) {
		ZCBOR_FAIL();
	}
	return true;
}


bool zcbor_union_elem_code(zcbor_state_t *state)
{
	if (!zcbor_process_backup(state, ZCBOR_FLAG_RESTORE, state->elem_count)) {
		ZCBOR_FAIL();
	}
	return true;
}

bool zcbor_union_end_code(zcbor_state_t *state)
{
	if (!zcbor_process_backup(state, ZCBOR_FLAG_CONSUME, state->elem_count)) {
		ZCBOR_FAIL();
	}
	return true;
}

void zcbor_new_state(zcbor_state_t *state_array, size_t n_states,
		const uint8_t *payload, size_t payload_len, size_t elem_count,
		uint8_t *flags, size_t flags_bytes)
{
	state_array[0].payload = payload;
	state_array[0].payload_end = payload + payload_len;
	state_array[0].elem_count = elem_count;
	state_array[0].payload_moved = false;
	state_array[0].decode_state.indefinite_length_array = false;
#ifdef ZCBOR_MAP_SMART_SEARCH
	state_array[0].decode_state.map_search_elem_state = flags;
	state_array[0].decode_state.map_elem_count = 0;
#else
	state_array[0].decode_state.map_elems_processed = 0;
	(void)flags;
	(void)flags_bytes;
#endif
	state_array[0].constant_state = NULL;

	if (n_states < 2) {
		return;
	}

	/* Use the last state as a struct zcbor_state_constant object. */
	state_array[0].constant_state = (struct zcbor_state_constant *)&state_array[n_states - 1];
	state_array[0].constant_state->backup_list = NULL;
	state_array[0].constant_state->num_backups = n_states - 2;
	state_array[0].constant_state->current_backup = 0;
	state_array[0].constant_state->error = ZCBOR_SUCCESS;
#ifdef ZCBOR_STOP_ON_ERROR
	state_array[0].constant_state->stop_on_error = false;
#endif
	state_array[0].constant_state->manually_process_elem = false;
#ifdef ZCBOR_MAP_SMART_SEARCH
	state_array[0].constant_state->map_search_elem_state_end = flags + flags_bytes;
#endif
	if (n_states > 2) {
		state_array[0].constant_state->backup_list = &state_array[1];
	}
}

void zcbor_update_state(zcbor_state_t *state,
		const uint8_t *payload, size_t payload_len)
{
	state->payload = payload;
	state->payload_end = payload + payload_len;

	update_backups(state, state->payload_end);
}


bool zcbor_validate_string_fragments(struct zcbor_string_fragment *fragments,
		size_t num_fragments)
{
	size_t total_len = 0;

	if (fragments == NULL) {
		return false;
	}

	for (size_t i = 0; i < num_fragments; i++) {
		if (fragments[i].offset != total_len) {
			return false;
		}
		if (fragments[i].fragment.value == NULL) {
			return false;
		}
		if (fragments[i].total_len != fragments[0].total_len) {
			return false;
		}
		total_len += fragments[i].fragment.len;
		if (total_len > fragments[0].total_len) {
			return false;
		}
	}

	if (num_fragments && total_len != fragments[0].total_len) {
		return false;
	}

	if (num_fragments && (fragments[0].total_len == ZCBOR_STRING_FRAGMENT_UNKNOWN_LENGTH)) {
		for (size_t i = 0; i < num_fragments; i++) {
			fragments[i].total_len = total_len;
		}
	}

	return true;
}

bool zcbor_splice_string_fragments(struct zcbor_string_fragment *fragments,
		size_t num_fragments, uint8_t *result, size_t *result_len)
{
	size_t total_len = 0;

	if (!fragments) {
		return false;
	}

	for (size_t i = 0; i < num_fragments; i++) {
		if ((total_len > *result_len)
			|| (fragments[i].fragment.len > (*result_len - total_len))) {
			return false;
		}
		memcpy(&result[total_len],
			fragments[i].fragment.value, fragments[i].fragment.len);
		total_len += fragments[i].fragment.len;
	}

	*result_len = total_len;
	return true;
}


bool zcbor_compare_strings(const struct zcbor_string *str1,
		const struct zcbor_string *str2)
{
	return (str1 != NULL) && (str2 != NULL)
		&& (str1->value != NULL) && (str2->value != NULL) && (str1->len == str2->len)
		&& (memcmp(str1->value, str2->value, str1->len) == 0);
}


size_t zcbor_header_len(uint64_t value)
{
	if (value <= ZCBOR_VALUE_IN_HEADER) {
		return 1;
	} else if (value <= 0xFF) {
		return 2;
	} else if (value <= 0xFFFF) {
		return 3;
	} else if (value <= 0xFFFFFFFF) {
		return 5;
	} else {
		return 9;
	}
}


size_t zcbor_header_len_ptr(const void *const value, size_t value_len)
{
	uint64_t val64 = 0;

	if (value_len > sizeof(val64)) {
		return 0;
	}

	memcpy(((uint8_t*)&val64) + ZCBOR_ECPY_OFFS(sizeof(val64), value_len), value, value_len);
	return zcbor_header_len(val64);
}


int zcbor_entry_function(const uint8_t *payload, size_t payload_len,
	void *result, size_t *payload_len_out, zcbor_state_t *state, zcbor_decoder_t func,
	size_t n_states, size_t elem_count)
{
	zcbor_new_state(state, n_states, payload, payload_len, elem_count, NULL, 0);

	bool ret = func(state, result);

	if (!ret) {
		int err = zcbor_pop_error(state);

		err = (err == ZCBOR_SUCCESS) ? ZCBOR_ERR_UNKNOWN : err;
		return err;
	}

	if (payload_len_out != NULL) {
		*payload_len_out = MIN(payload_len,
				(size_t)state[0].payload - (size_t)payload);
	}
	return ZCBOR_SUCCESS;
}


/* Float16: */
#define F16_SIGN_OFFS 15 /* Bit offset of the sign bit. */
#define F16_EXPO_OFFS 10 /* Bit offset of the exponent. */
#define F16_EXPO_MSK 0x1F /* Bitmask for the exponent (right shifted by F16_EXPO_OFFS). */
#define F16_MANTISSA_MSK 0x3FF /* Bitmask for the mantissa. */
#define F16_MAX 65520 /* Lowest float32 value that rounds up to float16 infinity.
		       * (65519.996 rounds to 65504) */
#define F16_MIN_EXPO 24 /* Negative exponent of the non-zero float16 value closest to 0 (2^-24) */
#define F16_MIN (1.0f / (1 << F16_MIN_EXPO)) /* The non-zero float16 value closest to 0 (2^-24) */
#define F16_MIN_NORM (1.0f / (1 << 14)) /* The normalized float16 value closest to 0 (2^-14) */
#define F16_BIAS 15 /* The exponent bias of normalized float16 values. */

/* Float32: */
#define F32_SIGN_OFFS 31 /* Bit offset of the sign bit. */
#define F32_EXPO_OFFS 23 /* Bit offset of the exponent. */
#define F32_EXPO_MSK 0xFF /* Bitmask for the exponent (right shifted by F32_EXPO_OFFS). */
#define F32_MANTISSA_MSK 0x7FFFFF /* Bitmask for the mantissa. */
#define F32_BIAS 127 /* The exponent bias of normalized float32 values. */

/* Rounding: */
#define SUBNORM_ROUND_MSK (F32_MANTISSA_MSK | (1 << F32_EXPO_OFFS)) /* mantissa + lsb of expo for
								     * tiebreak. */
#define SUBNORM_ROUND_BIT_MSK (1 << (F32_EXPO_OFFS - 1)) /* msb of mantissa (0x400000) */
#define NORM_ROUND_MSK (F32_MANTISSA_MSK >> (F16_EXPO_OFFS - 1)) /* excess mantissa when going from
								  * float32 to float16 + 1 extra bit
								  * for tiebreak. */
#define NORM_ROUND_BIT_MSK (1 << (F32_EXPO_OFFS - F16_EXPO_OFFS - 1)) /* bit 12 (0x1000) */


float zcbor_float16_to_32(uint16_t input)
{
	uint32_t sign = input >> F16_SIGN_OFFS;
	uint32_t expo = (input >> F16_EXPO_OFFS) & F16_EXPO_MSK;
	uint32_t mantissa = input & F16_MANTISSA_MSK;

	if ((expo == 0) && (mantissa != 0)) {
		/* Subnormal float16 - convert to normalized float32 */
		return ((float)mantissa * F16_MIN) * (sign ? -1 : 1);
	} else {
		/* Normalized / zero / Infinity / NaN */
		uint32_t new_expo = (expo == 0 /* zero */) ? 0
			: (expo == F16_EXPO_MSK /* inf/NaN */) ? F32_EXPO_MSK
				: (expo + (F32_BIAS - F16_BIAS));
		uint32_t value32 = (sign << F32_SIGN_OFFS) | (new_expo << F32_EXPO_OFFS)
			| (mantissa << (F32_EXPO_OFFS - F16_EXPO_OFFS));
		float result;

		memcpy(&result, &value32, sizeof(result));
		return result;
	}
}


uint16_t zcbor_float32_to_16(float input)
{
	uint32_t value32;

	memcpy(&value32, &input, sizeof(value32));

	uint32_t sign = value32 >> F32_SIGN_OFFS;
	uint32_t expo = (value32 >> F32_EXPO_OFFS) & F32_EXPO_MSK;
	uint32_t mantissa = value32 & F32_MANTISSA_MSK;

	uint16_t value16 = (uint16_t)(sign << F16_SIGN_OFFS);

	uint32_t abs_value32 = value32 & ~(1 << F32_SIGN_OFFS);
	float abs_input;

	memcpy(&abs_input, &abs_value32, sizeof(abs_input));

	if (abs_input <= (F16_MIN / 2)) {
		/* 0 or too small for float16. Round down to 0. value16 is already correct. */
	} else if (abs_input < F16_MIN) {
		/* Round up to 2^(-24) (F16_MIN), has other rounding rules than larger values. */
		value16 |= 0x0001;
	} else if (abs_input < F16_MIN_NORM) {
		/* Subnormal float16 (normal float32) */
		uint32_t adjusted_mantissa =
			/* Adjust for the purposes of checking rounding. */
			/* The lsb of expo is needed for the cases where expo is 103 (minimum). */
			((value32 << (expo - (F32_BIAS - F16_MIN_EXPO))) & SUBNORM_ROUND_MSK);
		uint16_t rounding_bit =
			/* "Round to nearest, ties to even". */
			/* 0x400000 means ties go down towards even. (0xC00000 means ties go up.) */
			(adjusted_mantissa & SUBNORM_ROUND_BIT_MSK)
				&& (adjusted_mantissa != SUBNORM_ROUND_BIT_MSK);
		value16 |= ((uint16_t)(abs_input * (1 << 24)) + rounding_bit); /* expo is 0 */
	} else if (abs_input < F16_MAX) {
		/* Normal float16 (normal float32) */
		uint16_t rounding_bit =
			/* Bit 13 of the mantissa represents which way to round, except for the */
			/* special case where bits 0-12 and 14 are 0. */
			/* This is because of "Round to nearest, ties to even". */
			/* 0x1000 means ties go down towards even. (0x3000 means ties go up.) */
			((mantissa & NORM_ROUND_BIT_MSK)
				&& ((mantissa & NORM_ROUND_MSK) != NORM_ROUND_BIT_MSK));
		value16 |= (uint16_t)((expo - (F32_BIAS - F16_BIAS)) << F16_EXPO_OFFS);
		value16 |= (uint16_t)(mantissa >> (F32_EXPO_OFFS - F16_EXPO_OFFS));
		value16 += rounding_bit; /* Might propagate to exponent. */
	} else if (expo != F32_EXPO_MSK || !mantissa) {
		/* Infinite, or finite normal float32 too large for float16. Round up to inf. */
		value16 |= (F16_EXPO_MSK << F16_EXPO_OFFS);
	} else {
		/* NaN */
		/* Preserve msbit of mantissa. */
		uint16_t new_mantissa = (uint16_t)(mantissa >> (F32_EXPO_OFFS - F16_EXPO_OFFS));
		value16 |= (F16_EXPO_MSK << F16_EXPO_OFFS) | (new_mantissa ? new_mantissa : 1);
	}

	return value16;
}


/** Weak strnlen() implementation in case it is not available.
 *
 * This function is in the public domain, according to:
 * https://github.com/arm-embedded/gcc-arm-none-eabi.debian/blob/master/src/libiberty/strnlen.c
 */
__attribute__((__weak__))
size_t strnlen (const char *s, size_t maxlen)
{
	size_t i;

	for (i = 0; i < maxlen; ++i) {
		if (s[i] == '\0') {
			break;
		}
	}
	return i;
}
