/*
 * This file has been copied from the zcbor library.
 * Commit zcbor 0.7.0
 */

/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZCBOR_ENCODE_H__
#define ZCBOR_ENCODE_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "zcbor_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The zcbor_encode library provides functions for encoding CBOR data elements.
 *
 * See The README for an introduction to CBOR, including the meaning of pint,
 * nint, bstr etc.
 */


/** The following param and retval docs apply to all single value encoding functions
 *
 * @param[inout] state  The current state of the encoding.
 * @param[in]    input  The value to encode.
 *
 * @retval true   Everything is ok.
 * @retval false  If the payload is exhausted. Or an unexpected error happened.
 */

/** Encode a pint/nint. */
bool zcbor_int32_put(zcbor_state_t *state, int32_t input);
bool zcbor_int64_put(zcbor_state_t *state, int64_t input);
bool zcbor_uint32_put(zcbor_state_t *state, uint32_t input);
bool zcbor_uint64_put(zcbor_state_t *state, uint64_t input);
bool zcbor_size_put(zcbor_state_t *state, size_t input);

/** Encode a pint/nint from a pointer.
 *
 *  Can be used for bulk encoding with @ref zcbor_multi_encode.
 */
bool zcbor_int_encode(zcbor_state_t *state, const void *input_int, size_t int_size);
bool zcbor_int32_encode(zcbor_state_t *state, const int32_t *input);
bool zcbor_int64_encode(zcbor_state_t *state, const int64_t *input);
bool zcbor_uint32_encode(zcbor_state_t *state, const uint32_t *input);
bool zcbor_uint64_encode(zcbor_state_t *state, const uint64_t *input);
bool zcbor_size_encode(zcbor_state_t *state, const size_t *input);

/** Encode a bstr. */
bool zcbor_bstr_encode(zcbor_state_t *state, const struct zcbor_string *input);
/** Encode a tstr. */
bool zcbor_tstr_encode(zcbor_state_t *state, const struct zcbor_string *input);

/** Encode a pointer to a string as a bstr/tstr.
 *
 * @param[inout] state   The current state of the encoding.
 * @param[in]    string  The value to encode. A pointer to the string
 * @param[in]    len     The length of the string pointed to by @p string.
 */
static inline bool zcbor_bstr_encode_ptr(zcbor_state_t *state, const char *ptr, size_t len)
{
	const struct zcbor_string zs = { .value = (const uint8_t *)ptr, .len = len };

	return zcbor_bstr_encode(state, &zs);
}
static inline bool zcbor_tstr_encode_ptr(zcbor_state_t *state, const char *ptr, size_t len)
{
	const struct zcbor_string zs = { .value = (const uint8_t *)ptr, .len = len };

	return zcbor_tstr_encode(state, &zs);
}

/** Encode a string literal as a bstr/tstr.
 *
 * @param[inout] state   The current state of the encoding.
 * @param[in]    string  The value to encode. A string literal, e.g. "Foo", so
 *                       that sizeof(string) - 1 is the length of the string.
 */
#define zcbor_bstr_put_lit(state, string) \
	zcbor_bstr_encode_ptr(state, string, sizeof(string) - 1)
#define zcbor_tstr_put_lit(state, string) \
	zcbor_tstr_encode_ptr(state, string, sizeof(string) - 1)

/** Encode null-terminated string as a bstr/tstr.
 *
 * @param[inout] state   The current state of the encoding.
 * @param[in]    string  The value to encode. Must be a null-terminated string,
 *                       so that strlen can be used.
 */
#define zcbor_bstr_put_term(state, string) \
	zcbor_bstr_encode_ptr(state, string, strlen(string))
#define zcbor_tstr_put_term(state, string) \
	zcbor_tstr_encode_ptr(state, string, strlen(string))

/** Encode a char array literal as a bstr/tstr.
 *
 * @param[inout] state   The current state of the encoding.
 * @param[in]    string  The value to encode. An array literal, e.g. {'F', 'o', 'o'},
 *                       so that sizeof(string) is the length of the string.
 */
#define zcbor_bstr_put_arr(state, string) \
	zcbor_bstr_encode_ptr(state, string, sizeof(string))
#define zcbor_tstr_put_arr(state, string) \
	zcbor_tstr_encode_ptr(state, string, sizeof(string))

/** Encode a tag. Must be called before encoding the value being tagged. */
bool zcbor_tag_encode(zcbor_state_t *state, uint32_t input);

/** Encode a boolean primitive value. */
bool zcbor_bool_put(zcbor_state_t *state, bool input);
bool zcbor_bool_encode(zcbor_state_t *state, const bool *input);

/** Encode a float */
bool zcbor_float32_put(zcbor_state_t *state, float input);
bool zcbor_float32_encode(zcbor_state_t *state, const float *input);
bool zcbor_float64_put(zcbor_state_t *state, double input);
bool zcbor_float64_encode(zcbor_state_t *state, const double *input);

/** Encode a "nil"/"undefined" primitive value. @p unused should be NULL.
 *
 * @param[inout] state   The current state of the encoding.
 * @param[in]    unused  Unused parameter to maintain signature parity with
 *                       @ref zcbor_encoder_t.
 */
bool zcbor_nil_put(zcbor_state_t *state, const void *unused);
bool zcbor_undefined_put(zcbor_state_t *state, const void *unused);

/** Encode a bstr header.
 *
 * The rest of the string can be encoded as CBOR.
 * A state backup is created to keep track of the element count.
 * Call @ref zcbor_bstr_end_encode when done encoding the contents of the bstr.
 *
 * @param[inout] state   The current state of the encoding.
 *
 * @retval true   Header encoded correctly
 * @retval false  Header encoded incorrectly, or backup failed.
 */
bool zcbor_bstr_start_encode(zcbor_state_t *state);

/** Finalize encoding a CBOR-encoded bstr.
 *
 * Restore element count from backup.
 */
bool zcbor_bstr_end_encode(zcbor_state_t *state, struct zcbor_string *result);

/** Encode a list/map header.
 *
 * The contents of the list/map can be encoded via subsequent function calls.
 * If ZCBOR_CANONICAL is defined, a state backup is created to keep track of the
 * element count.
 * When all members have been encoded, call @ref zcbor_list_end_encode /
 * @ref zcbor_map_end_encode to close the list/map.
 *
 * @param[inout] state    The current state of the encoding.
 * @param[in]    max_num  The maximum number of members in the list/map.
 *                        This serves as a size hint for the header. Must be
 *                        equal to the max_num provided to the corresponding
 *                        @ref zcbor_list_end_encode / @ref zcbor_map_end_encode
 *                        call.
 *                        Only used when ZCBOR_CANONICAL is defined.
 */
bool zcbor_list_start_encode(zcbor_state_t *state, uint_fast32_t max_num);
bool zcbor_map_start_encode(zcbor_state_t *state, uint_fast32_t max_num);

/** Encode the end of a list/map. Do some checks and deallocate backup.
 *
 *  - Default: Adds a list terminator (0xFF) to mark the
 *    end of the list/map.
 *  - If ZCBOR_CANONICAL is defined: Instead encodes the number of members in
 *    the list/map header. If the header ends up a different size than expected,
 *    the list/map contents are moved using memmove().
 *
 * Use @ref zcbor_list_map_end_force_encode to forcibly consume the backup if
 * something has gone wrong.
 *
 * @param[inout] state    The current state of the encoding.
 * @param[in]    max_num  The maximum number of members in the list/map. Must be
 *                        equal to the max_num provided to the corresponding
 *                        @ref zcbor_list_start_encode call.
 *                        Only used when ZCBOR_CANONICAL is defined.
 */
bool zcbor_list_end_encode(zcbor_state_t *state, uint_fast32_t max_num);
bool zcbor_map_end_encode(zcbor_state_t *state, uint_fast32_t max_num);
bool zcbor_list_map_end_force_encode(zcbor_state_t *state);

/** Encode 0 or more elements with the same type and constraints.
 *
 * The encoded values are taken from the @p input array.
 *
 * The following is an example of encoding a list containing 3 INTS followed by
 * 0 to 2 bstrs:
 *
 * @code{c}
 *     uint32_t ints[3] = <initialize>;
 *     struct zcbor_string bstrs[2] = <initialize>;
 *     bool res;
 *
 *     res = zcbor_list_start_encode(state, 5);
 *     res = res && zcbor_multi_encode(3, zcbor_uint32_encode, state,
 *                  ints, sizeof(uint32_t));
 *     res = res && zcbor_multi_encode(2, zcbor_bstr_encode, state,
 *                  bstrs, sizeof(struct zcbor_string));
 *     res = res && zcbor_list_end_encode(state, 5);
 *     // check res
 * @endcode
 *
 * The @ref zcbor_encoder_t type is designed to be compatible with all single-
 * value encoder functions in this library, e.g. @ref zcbor_uint32_encode,
 * @ref zcbor_tstr_put, @ref zcbor_nil_put, etc. For _put() functions,
 * @p input will be used as a value instead of an array/pointer, so
 * @p input_len will determine how much the value changes for each call.
 * To encode the same value multiple times, use a @p input_len of 0.
 * This function can also be used with custom decoder functions, such as those
 * generated by the zcbor.py script, which for example encodes larger chunks of
 * the data at once.
 *
 * @param[in]  num_encode    The actual number of elements.
 * @param[in]  encoder       The encoder function to call under the hood. This
 *                           function will be called with the provided arguments
 *                           repeatedly until the function fails (returns false)
 *                           or until it has been called @p max_encode times.
 *                           The input pointer is moved @p input_len bytes for
 *                           each call to @p encoder, i.e. @p input refers to an
 *                           array of input variables.
 * @param[in]  input         Source of the encoded values. Must be an array of
 *                           at least @p max_encode elements.
 * @param[in]  input_len     The length of the input variables. Must be the
 *                           length of the individual elements in input.
 *
 * @retval true   If at least @p min_encode variables were correctly encoded.
 * @retval false  If @p encoder failed before having encoded @p min_encode
 *                values.
 */
bool zcbor_multi_encode(uint_fast32_t num_encode,
		zcbor_encoder_t encoder,
		zcbor_state_t *state,
		const void *input,
		uint_fast32_t result_len);

/** Works like @ref zcbor_multi_encode
 *
 * But first checks that @p num_encode is between @p min_encode and @p max_encode.
 */
bool zcbor_multi_encode_minmax(uint_fast32_t min_encode, uint_fast32_t max_encode, const uint_fast32_t *num_encode,
		zcbor_encoder_t encoder, zcbor_state_t *state, const void *input,
		uint_fast32_t input_len);

/** Runs @p encoder on @p state and @p input if @p present is true.
 *
 * Calls @ref zcbor_multi_encode under the hood.
 */
bool zcbor_present_encode(const uint_fast32_t *present,
		zcbor_encoder_t encoder,
		zcbor_state_t *state,
		const void *input);

/** See @ref zcbor_new_state() */
void zcbor_new_encode_state(zcbor_state_t *state_array, uint_fast32_t n_states,
		uint8_t *payload, size_t payload_len, uint_fast32_t elem_count);

/** Convenience macro for declaring and initializing a state with backups.
 *
 *  This gives you a state variable named @p name. The variable functions like
 *  a pointer.
 *
 *  @param[in]  name          The name of the new state variable.
 *  @param[in]  num_backups   The number of backup slots to keep in the state.
 *  @param[in]  payload       The payload to work on.
 *  @param[in]  payload_size  The size (in bytes) of @p payload.
 *  @param[in]  elem_count    The starting elem_count (typically 1).
 */
#define ZCBOR_STATE_E(name, num_backups, payload, payload_size, elem_count) \
zcbor_state_t name[((num_backups) + 2)]; \
do { \
	zcbor_new_encode_state(name, ZCBOR_ARRAY_SIZE(name), payload, payload_size, elem_count); \
} while(0)

#ifdef __cplusplus
}
#endif

#endif /* ZCBOR_ENCODE_H__ */
