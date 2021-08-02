/*
 * This file has been copied from the cddl-gen submodule.
 * Commit 9f77837f9950da1633d22abf6181a830521a6688
 */

/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CBOR_ENCODE_H__
#define CBOR_ENCODE_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cbor_common.h"


/** Encode a PINT/NINT into a int32_t.
 *
 * @param[inout] state        The current state of the decoding.
 * @param[out]   result       Where to place the encoded value.
 *
 * @retval true   Everything is ok.
 * @retval false  If the payload is exhausted.
 */
bool intx32_encode(cbor_state_t *state, const int32_t *input);
bool intx32_put(cbor_state_t *state, int32_t result);

/** Encode a PINT into a uint32_t. */
bool uintx32_encode(cbor_state_t *state, const uint32_t *result);
bool uintx32_put(cbor_state_t *state, uint32_t result);

/** Encode a BSTR header.
 *
 * The rest of the string can be encoded as CBOR.
 * A state backup is created to keep track of the element count.
 *
 * @retval true   Header encoded correctly
 * @retval false  Header encoded incorrectly, or backup failed.
 */
bool bstrx_cbor_start_encode(cbor_state_t *state, const cbor_string_type_t *result);

/** Finalize encoding a CBOR-encoded BSTR.
 *
 * Restore element count from backup.
 */
bool bstrx_cbor_end_encode(cbor_state_t *state);

/** Encode a BSTR, */
bool bstrx_encode(cbor_state_t *state, const cbor_string_type_t *result);

/** Encode a TSTR. */
bool tstrx_encode(cbor_state_t *state, const cbor_string_type_t *result);

#define tstrx_put(state, string) \
	tstrx_encode(state, &(cbor_string_type_t){.value = (const uint8_t *)string, .len = (sizeof(string) - 1)})

#define tstrx_put_term(state, string) \
	tstrx_encode(state, &(cbor_string_type_t){.value = (const uint8_t *)string, .len = strlen((const char *)string)})

/** Encode a LIST header.
 *
 * The contents of the list can be decoded via subsequent function calls.
 * A state backup is created to keep track of the element count.
 */
bool list_start_encode(cbor_state_t *state, uint32_t max_num);

/** Encode a MAP header. */
bool map_start_encode(cbor_state_t *state, uint32_t max_num);

/** Encode end of a LIST. Do some checks and deallocate backup. */
bool list_end_encode(cbor_state_t *state, uint32_t max_num);

/** Encode end of a MAP. Do some checks and deallocate backup. */
bool map_end_encode(cbor_state_t *state, uint32_t max_num);

/** Encode a "nil" primitive value. result should be NULL. */
bool nilx_put(cbor_state_t *state, const void *result);

/** Encode a boolean primitive value. */
bool boolx_encode(cbor_state_t *state, const bool *result);
bool boolx_put(cbor_state_t *state, bool result);

/** Encode a float */
bool float_encode(cbor_state_t *state, double *result);
bool float_put(cbor_state_t *state, double result);

/** Dummy encode "any": Encode a "nil". input should be NULL. */
bool any_encode(cbor_state_t *state, void *input);

/** Encode a tag. */
bool tag_encode(cbor_state_t *state, uint32_t tag);

/** Encode 0 or more elements with the same type and constraints.
 *
 * @details This must not necessarily encode all elements in a list. E.g. if
 *          the list contains 3 INTS between 0 and 100 followed by 0 to 2 BSTRs
 *          with length 8, that could be done with:
 *
 *          @code{c}
 *              uint32_t int_min = 0;
 *              uint32_t int_max = 100;
 *              uint32_t bstr_size = 8;
 *              uint32_t ints[3];
 *              cbor_string_type_t bstrs[2] = <initialize here>;
 *              bool res;
 *
 *              res = list_start_encode(state, 5);
 *              // check res
 *              res = multi_encode(3, 3, &num_encode, uintx32_encode, state,
 *                           ints, 4);
 *              // check res
 *              res = multi_encode(0, 2, &num_encode, strx_encode, state,
 *                           bstrs, sizeof(cbor_string_type_t));
 *              // check res
 *              res = list_end_encode(state, 5);
 *              // check res
 *          @endcode
 *
 * @param[in]  min_encode    The minimum acceptable number of elements.
 * @param[in]  max_encode    The maximum acceptable number of elements.
 * @param[in]  num_encode    The actual number of elements.
 * @param[in]  encoder       The encoder function to call under the hood. This
 *                           function will be called with the provided arguments
 *                           repeatedly until the function fails (returns false)
 *                           or until it has been called @p max_encode times.
 *                           result is moved @p result_len bytes for each call
 *                           to @p encoder, i.e. @p result refers to an array
 *                           of result variables.
 * @param[in]  input         Source of the encoded values. Must be an array
 *                           of length at least @p max_encode.
 * @param[in]  result_len    The length of the result variables. Must be the
 *                           length of the elements in result.
 *
 * @retval true   If at least @p min_encode variables were correctly encoded.
 * @retval false  If @p encoder failed before having encoded @p min_encode
 *                values.
 */
bool multi_encode(uint32_t min_encode, uint32_t max_encode, const uint32_t *num_encode,
		cbor_encoder_t encoder, cbor_state_t *state, const void *input,
		uint32_t result_len);

bool present_encode(const uint32_t *present,
		cbor_encoder_t encoder,
		cbor_state_t *state,
		const void *input);

#endif /* CBOR_ENCODE_H__ */
