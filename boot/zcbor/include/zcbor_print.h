/*
 * This file has been copied from the zcbor library.
 * Commit zcbor 0.8.0
 */

/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZCBOR_PRINT_H__
#define ZCBOR_PRINT_H__


#ifdef __cplusplus
extern "C" {
#endif

#ifndef ZCBOR_PRINT_FUNC
#include <stdio.h>
#define zcbor_do_print(...) printf(__VA_ARGS__)
#else
#define zcbor_do_print(...) ZCBOR_PRINT_FUNC(__VA_ARGS__)
#endif

#ifdef ZCBOR_VERBOSE
#define zcbor_trace_raw(state) (zcbor_do_print("rem: %zu, cur: 0x%x, ec: 0x%zx, err: %d",\
	(size_t)state->payload_end - (size_t)state->payload, *state->payload, state->elem_count, \
	state->constant_state ? state->constant_state->error : 0))
#define zcbor_trace(state, appendix) do { \
	zcbor_trace_raw(state); \
	zcbor_do_print(", %s\n", appendix); \
} while(0)
#define zcbor_trace_file(state) do { \
	zcbor_trace_raw(state); \
	zcbor_do_print(", %s:%d\n", __FILE__, __LINE__); \
} while(0)

#define zcbor_log_assert(expr, ...) \
do { \
	zcbor_do_print("ASSERTION \n  \"" #expr \
		"\"\nfailed at %s:%d with message:\n  ", \
		__FILE__, __LINE__); \
	zcbor_do_print(__VA_ARGS__);\
} while(0)
#define zcbor_log(...) zcbor_do_print(__VA_ARGS__)
#else
#define zcbor_trace(state, appendix)
#define zcbor_trace_file(state) ((void)state)
#define zcbor_log_assert(...)
#define zcbor_log(...)
#endif

#ifdef ZCBOR_ASSERTS
#define zcbor_assert(expr, ...) \
do { \
	if (!(expr)) { \
		zcbor_log_assert(expr, __VA_ARGS__); \
		ZCBOR_FAIL(); \
	} \
} while(0)
#define zcbor_assert_state(expr, ...) \
do { \
	if (!(expr)) { \
		zcbor_log_assert(expr, __VA_ARGS__); \
		ZCBOR_ERR(ZCBOR_ERR_ASSERTION); \
	} \
} while(0)
#else
#define zcbor_assert(expr, ...)
#define zcbor_assert_state(expr, ...)
#endif

__attribute__((used))
static void zcbor_print_compare_lines(const uint8_t *str1, const uint8_t *str2, size_t size)
{
	for (size_t j = 0; j < size; j++) {
		zcbor_do_print("%x ", str1[j]);
	}
	zcbor_do_print("\r\n");
	for (size_t j = 0; j < size; j++) {
		zcbor_do_print("%x ", str2[j]);
	}
	zcbor_do_print("\r\n");
	for (size_t j = 0; j < size; j++) {
		zcbor_do_print("%x ", str1[j] != str2[j]);
	}
	zcbor_do_print("\r\n");
	zcbor_do_print("\r\n");
}

__attribute__((used))
static void zcbor_print_compare_strings(const uint8_t *str1, const uint8_t *str2, size_t size)
{
	for (size_t i = 0; i <= size / 16; i++) {
		zcbor_do_print("line %zu (char %zu)\r\n", i, i*16);
		zcbor_print_compare_lines(&str1[i*16], &str2[i*16],
			MIN(16, (size - i*16)));
	}
	zcbor_do_print("\r\n");
}

__attribute__((used))
static void zcbor_print_compare_strings_diff(const uint8_t *str1, const uint8_t *str2, size_t size)
{
	bool printed = false;
	for (size_t i = 0; i <= size / 16; i++) {
		if (memcmp(&str1[i*16], &str2[i*16], MIN(16, (size - i*16))) != 0) {
			zcbor_do_print("line %zu (char %zu)\r\n", i, i*16);
			zcbor_print_compare_lines(&str1[i*16], &str2[i*16],
				MIN(16, (size - i*16)));
			printed = true;
		}
	}
	if (printed) {
		zcbor_do_print("\r\n");
	}
}

__attribute__((used))
static const char *zcbor_error_str(int error)
{
	#define ZCBOR_ERR_CASE(err) case err: \
		return #err; /* The literal is static per C99 6.4.5 paragraph 5. */\

	switch(error) {
		ZCBOR_ERR_CASE(ZCBOR_SUCCESS)
		ZCBOR_ERR_CASE(ZCBOR_ERR_NO_BACKUP_MEM)
		ZCBOR_ERR_CASE(ZCBOR_ERR_NO_BACKUP_ACTIVE)
		ZCBOR_ERR_CASE(ZCBOR_ERR_LOW_ELEM_COUNT)
		ZCBOR_ERR_CASE(ZCBOR_ERR_HIGH_ELEM_COUNT)
		ZCBOR_ERR_CASE(ZCBOR_ERR_INT_SIZE)
		ZCBOR_ERR_CASE(ZCBOR_ERR_FLOAT_SIZE)
		ZCBOR_ERR_CASE(ZCBOR_ERR_ADDITIONAL_INVAL)
		ZCBOR_ERR_CASE(ZCBOR_ERR_NO_PAYLOAD)
		ZCBOR_ERR_CASE(ZCBOR_ERR_PAYLOAD_NOT_CONSUMED)
		ZCBOR_ERR_CASE(ZCBOR_ERR_WRONG_TYPE)
		ZCBOR_ERR_CASE(ZCBOR_ERR_WRONG_VALUE)
		ZCBOR_ERR_CASE(ZCBOR_ERR_WRONG_RANGE)
		ZCBOR_ERR_CASE(ZCBOR_ERR_ITERATIONS)
		ZCBOR_ERR_CASE(ZCBOR_ERR_ASSERTION)
		ZCBOR_ERR_CASE(ZCBOR_ERR_PAYLOAD_OUTDATED)
		ZCBOR_ERR_CASE(ZCBOR_ERR_ELEM_NOT_FOUND)
		ZCBOR_ERR_CASE(ZCBOR_ERR_MAP_MISALIGNED)
		ZCBOR_ERR_CASE(ZCBOR_ERR_ELEMS_NOT_PROCESSED)
		ZCBOR_ERR_CASE(ZCBOR_ERR_NOT_AT_END)
		ZCBOR_ERR_CASE(ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE)
		ZCBOR_ERR_CASE(ZCBOR_ERR_INVALID_VALUE_ENCODING)
	}
	#undef ZCBOR_ERR_CASE

	return "ZCBOR_ERR_UNKNOWN";
}

__attribute__((used))
static void zcbor_print_error(int error)
{
	zcbor_do_print("%s\r\n", zcbor_error_str(error));
}

#ifdef __cplusplus
}
#endif

#endif /* ZCBOR_PRINT_H__ */
