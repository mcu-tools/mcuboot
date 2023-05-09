/*
 * This file has been copied from the zcbor library.
 * Commit zcbor 0.7.0
 */

/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZCBOR_COMMON_H__
#define ZCBOR_COMMON_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Convenience type that allows pointing to strings directly inside the payload
 *  without the need to copy out.
 */
struct zcbor_string {
	const uint8_t *value;
	size_t len;
};


/** Type representing a string fragment.
 *
 * Don't modify any member variables, or subsequent calls may fail.
**/
struct zcbor_string_fragment {
	struct zcbor_string fragment; ///! Location and length of the fragment.
	size_t offset;                ///! The offset in the full string at which this fragment belongs.
	size_t total_len;             ///! The total length of the string this fragment is a part of.
};


/** Size to use in struct zcbor_string_fragment when the real size is unknown. */
#define ZCBOR_STRING_FRAGMENT_UNKNOWN_LENGTH SIZE_MAX

#ifdef ZCBOR_VERBOSE
#include <zephyr/sys/printk.h>
#define zcbor_trace() (printk("bytes left: %zu, byte: 0x%x, elem_count: 0x%" PRIxFAST32 ", err: %d, %s:%d\n",\
	(size_t)state->payload_end - (size_t)state->payload, *state->payload, state->elem_count, \
	state->constant_state ? state->constant_state->error : 0, __FILE__, __LINE__))

#define zcbor_print_assert(expr, ...) \
do { \
	printk("ASSERTION \n  \"" #expr \
		"\"\nfailed at %s:%d with message:\n  ", \
		__FILE__, __LINE__); \
	printk(__VA_ARGS__);\
} while(0)
#define zcbor_print(...) printk(__VA_ARGS__)
#else
#define zcbor_trace() ((void)state)
#define zcbor_print_assert(...)
#define zcbor_print(...)
#endif

#ifdef ZCBOR_ASSERTS
#define zcbor_assert(expr, ...) \
do { \
	if (!(expr)) { \
		zcbor_print_assert(expr, __VA_ARGS__); \
		ZCBOR_FAIL(); \
	} \
} while(0)
#define zcbor_assert_state(expr, ...) \
do { \
	if (!(expr)) { \
		zcbor_print_assert(expr, __VA_ARGS__); \
		ZCBOR_ERR(ZCBOR_ERR_ASSERTION); \
	} \
} while(0)
#else
#define zcbor_assert(expr, ...)
#define zcbor_assert_state(expr, ...)
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef ZCBOR_ARRAY_SIZE
#define ZCBOR_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#if SIZE_MAX <= UINT64_MAX
/** The ZCBOR_SUPPORTS_SIZE_T will be defined if processing of size_t type variables directly
 * with zcbor_size_ functions is supported.
**/
#define ZCBOR_SUPPORTS_SIZE_T
#else
#warning "zcbor: Unsupported size_t encoding size"
#endif

struct zcbor_state_constant;

typedef struct {
union {
	uint8_t *payload_mut;
	uint8_t const *payload; /**< The current place in the payload. Will be
	                             updated when an element is correctly
	                             processed. */
};
	uint8_t const *payload_bak; /**< Temporary backup of payload. */
	uint_fast32_t elem_count; /**< The current element is part of a LIST or a MAP,
	                               and this keeps count of how many elements are
	                               expected. This will be checked before processing
	                               and decremented if the element is correctly
	                               processed. */
	uint8_t const *payload_end; /**< The end of the payload. This will be
	                                 checked against payload before
	                                 processing each element. */
	bool indefinite_length_array; /**< Is set to true if the decoder is currently
	                                   decoding the contents of an indefinite-
	                                   length array. */
	bool payload_moved; /**< Is set to true while the state is stored as a backup
	                         if @ref zcbor_update_state is called, since that function
	                         updates the payload_end of all backed-up states. */
	struct zcbor_state_constant *constant_state; /**< The part of the state that is
	                                                  not backed up and duplicated. */
} zcbor_state_t;

struct zcbor_state_constant {
	zcbor_state_t *backup_list;
	uint_fast32_t current_backup;
	uint_fast32_t num_backups;
	int error;
#ifdef ZCBOR_STOP_ON_ERROR
	bool stop_on_error;
#endif
};

/** Function pointer type used with zcbor_multi_decode.
 *
 * This type is compatible with all decoding functions here and in the generated
 * code, except for zcbor_multi_decode.
 */
typedef bool(zcbor_encoder_t)(zcbor_state_t *, const void *);
typedef bool(zcbor_decoder_t)(zcbor_state_t *, void *);

/** Enumeration representing the major types available in CBOR.
 *
 * The major type is represented in the 3 first bits of the header byte.
 */
typedef enum
{
	ZCBOR_MAJOR_TYPE_PINT = 0, ///! Positive Integer
	ZCBOR_MAJOR_TYPE_NINT = 1, ///! Negative Integer
	ZCBOR_MAJOR_TYPE_BSTR = 2, ///! Byte String
	ZCBOR_MAJOR_TYPE_TSTR = 3, ///! Text String
	ZCBOR_MAJOR_TYPE_LIST = 4, ///! List
	ZCBOR_MAJOR_TYPE_MAP  = 5, ///! Map
	ZCBOR_MAJOR_TYPE_TAG  = 6, ///! Semantic Tag
	ZCBOR_MAJOR_TYPE_PRIM = 7, ///! Primitive Type
} zcbor_major_type_t;


/** Convenience macro for failing out of a decoding/encoding function.
*/
#define ZCBOR_FAIL() \
do {\
	zcbor_trace(); \
	return false; \
} while(0)

#define ZCBOR_ERR(err) \
do { \
	zcbor_error(state, err); \
	ZCBOR_FAIL(); \
} while(0)

#define ZCBOR_ERR_IF(expr, err) \
do {\
	if (expr) { \
		ZCBOR_ERR(err); \
	} \
} while(0)

#define ZCBOR_CHECK_PAYLOAD() \
	ZCBOR_ERR_IF(state->payload >= state->payload_end, ZCBOR_ERR_NO_PAYLOAD)

#ifdef ZCBOR_STOP_ON_ERROR
#define ZCBOR_CHECK_ERROR()  \
do { \
	if (!zcbor_check_error(state)) { \
		ZCBOR_FAIL(); \
	} \
} while(0)
#else
#define ZCBOR_CHECK_ERROR()
#endif

#define ZCBOR_VALUE_IN_HEADER 23 ///! Values below this are encoded directly in the header.
#define ZCBOR_VALUE_IS_1_BYTE 24 ///! The next 1 byte contains the value.
#define ZCBOR_VALUE_IS_2_BYTES 25 ///! The next 2 bytes contain the value.
#define ZCBOR_VALUE_IS_4_BYTES 26 ///! The next 4 bytes contain the value.
#define ZCBOR_VALUE_IS_8_BYTES 27 ///! The next 8 bytes contain the value.
#define ZCBOR_VALUE_IS_INDEFINITE_LENGTH 31 ///! The list or map has indefinite length, and will instead be terminated by a 0xFF token.

#define ZCBOR_BOOL_TO_PRIM ((uint8_t)20) ///! In CBOR, false/true have the values 20/21

#define ZCBOR_FLAG_RESTORE 1UL ///! Restore from the backup. Overwrite the current state with the state from the backup.
#define ZCBOR_FLAG_CONSUME 2UL ///! Consume the backup. Remove the backup from the stack of backups.
#define ZCBOR_FLAG_TRANSFER_PAYLOAD 4UL ///! Keep the pre-restore payload after restoring.

#define ZCBOR_SUCCESS 0
#define ZCBOR_ERR_NO_BACKUP_MEM 1
#define ZCBOR_ERR_NO_BACKUP_ACTIVE 2
#define ZCBOR_ERR_LOW_ELEM_COUNT 3
#define ZCBOR_ERR_HIGH_ELEM_COUNT 4
#define ZCBOR_ERR_INT_SIZE 5
#define ZCBOR_ERR_FLOAT_SIZE 6
#define ZCBOR_ERR_ADDITIONAL_INVAL 7 ///! > 27
#define ZCBOR_ERR_NO_PAYLOAD 8
#define ZCBOR_ERR_PAYLOAD_NOT_CONSUMED 9
#define ZCBOR_ERR_WRONG_TYPE 10
#define ZCBOR_ERR_WRONG_VALUE 11
#define ZCBOR_ERR_WRONG_RANGE 12
#define ZCBOR_ERR_ITERATIONS 13
#define ZCBOR_ERR_ASSERTION 14
#define ZCBOR_ERR_UNKNOWN 31

/** The largest possible elem_count. */
#ifdef UINT_FAST32_MAX
#define ZCBOR_MAX_ELEM_COUNT UINT_FAST32_MAX
#else
#define ZCBOR_MAX_ELEM_COUNT ((uint_fast32_t)(-1L))
#endif

/** Initial value for elem_count for when it just needs to be large. */
#define ZCBOR_LARGE_ELEM_COUNT (ZCBOR_MAX_ELEM_COUNT - 16)


/** Values defined by RFC8949 via www.iana.org/assignments/cbor-tags/cbor-tags.xhtml */
enum zcbor_rfc8949_tag {
	ZCBOR_TAG_TIME_TSTR =       0, ///! text string       Standard date/time string
	ZCBOR_TAG_TIME_NUM =        1, ///! integer or float  Epoch-based date/time
	ZCBOR_TAG_UBIGNUM_BSTR =    2, ///! byte string       Unsigned bignum
	ZCBOR_TAG_BIGNUM_BSTR =     3, ///! byte string       Negative bignum
	ZCBOR_TAG_DECFRAC_ARR =     4, ///! array             Decimal fraction
	ZCBOR_TAG_BIGFLOAT_ARR =    5, ///! array             Bigfloat
	ZCBOR_TAG_2BASE64URL =     21, ///! (any)             Expected conversion to base64url encoding
	ZCBOR_TAG_2BASE64 =        22, ///! (any)             Expected conversion to base64 encoding
	ZCBOR_TAG_2BASE16 =        23, ///! (any)             Expected conversion to base16 encoding
	ZCBOR_TAG_BSTR =           24, ///! byte string       Encoded CBOR data item
	ZCBOR_TAG_URI_TSTR =       32, ///! text string       URI
	ZCBOR_TAG_BASE64URL_TSTR = 33, ///! text string       base64url
	ZCBOR_TAG_BASE64_TSTR =    34, ///! text string       base64
	ZCBOR_TAG_MIME_TSTR =      36, ///! text string       MIME message
	ZCBOR_TAG_CBOR =        55799, ///! (any)             Self-described CBOR
};


/** Take a backup of the current state. Overwrite the current elem_count. */
bool zcbor_new_backup(zcbor_state_t *state, uint_fast32_t new_elem_count);

/** Consult the most recent backup. In doing so, check whether elem_count is
 *  less than or equal to max_elem_count.
 *  Also, take action based on the flags (See ZCBOR_FLAG_*).
 */
bool zcbor_process_backup(zcbor_state_t *state, uint32_t flags, uint_fast32_t max_elem_count);

/** Convenience function for starting encoding/decoding of a union.
 *
 *  That is, for attempting to encode, or especially decode, multiple options.
 *  Makes a new backup.
 */
bool zcbor_union_start_code(zcbor_state_t *state);

/** Convenience function before encoding/decoding one element of a union.
 *
 *  Call this before attempting each option.
 *  Restores the backup, without consuming it.
 */
bool zcbor_union_elem_code(zcbor_state_t *state);

/** Convenience function before encoding/decoding one element of a union.
 *
 *  Consumes the backup without restoring it.
 */
bool zcbor_union_end_code(zcbor_state_t *state);

/** Initialize a state with backups.
 *  As long as n_states is more than 1, one of the states in the array is used
 *  as a struct zcbor_state_constant object.
 *  If there is no struct zcbor_state_constant (n_states == 1), error codes are
 *  not available.
 *  This means that you get a state with (n_states - 2) backups.
 *  payload, payload_len, and elem_count are used to initialize the first state.
 *  in the array, which is the state that can be passed to cbor functions.
 */
void zcbor_new_state(zcbor_state_t *state_array, uint_fast32_t n_states,
		const uint8_t *payload, size_t payload_len, uint_fast32_t elem_count);

#ifdef ZCBOR_STOP_ON_ERROR
/** Check stored error and fail if present, but only if stop_on_error is true. */
static inline bool zcbor_check_error(const zcbor_state_t *state)
{
	struct zcbor_state_constant  *cs = state->constant_state;
	return !(cs && cs->stop_on_error && cs->error);
}
#endif

/** Return the current error state, replacing it with SUCCESS. */
static inline int zcbor_pop_error(zcbor_state_t *state)
{
	if (!state->constant_state) {
		return ZCBOR_SUCCESS;
	}
	int err = state->constant_state->error;

	state->constant_state->error = ZCBOR_SUCCESS;
	return err;
}

/** Look at current error state without altering it */
static inline int zcbor_peek_error(const zcbor_state_t *state)
{
	if (!state->constant_state) {
		return ZCBOR_SUCCESS;
	} else {
		return state->constant_state->error;
	}
}

/** Write the provided error to the error state. */
static inline void zcbor_error(zcbor_state_t *state, int err)
{
#ifdef ZCBOR_STOP_ON_ERROR
	if (zcbor_check_error(state))
#endif
	{
		if (state->constant_state) {
			state->constant_state->error = err;
		}
	}
}

/** Whether the current payload is exhausted. */
static inline bool zcbor_payload_at_end(const zcbor_state_t *state)
{
	return (state->payload == state->payload_end);
}

/** Update the current payload pointer (and payload_end).
 *
 *  For use when the payload is divided into multiple chunks.
 *
 *  This function also updates all backups to the new payload_end.
 *  This sets a flag so that if a backup is processed with the flag
 *  @ref ZCBOR_FLAG_RESTORE, but without the flag
 *  @ref ZCBOR_FLAG_TRANSFER_PAYLOAD since this would cause an invalid state.
 *
 *  @param[inout]  state              The current state, will be updated with
 *                                    the new payload pointer.
 *  @param[in]     payload            The new payload chunk.
 *  @param[in]     payload_len        The length of the new payload chunk.
 */
void zcbor_update_state(zcbor_state_t *state,
		const uint8_t *payload, size_t payload_len);

/** Check that the provided fragments are complete and in the right order.
 *
 *  If the total length is not known, the total_len can have the value
 *  @ref ZCBOR_STRING_FRAGMENT_UNKNOWN_LENGTH. If so, all fragments will be
 *  updated with the actual total length.
 *
 *  @param[in]  fragments      An array of string fragments. Cannot be NULL.
 *  @param[in]  num_fragments  The number of fragments in @p fragments.
 *
 *  @retval  true   If the fragments are in the right order, and there are no
 *                  fragments missing.
 *  @retval  false  If not all fragments have the same total_len, or gaps are
 *                  found, or if any fragment value is NULL.
 */
bool zcbor_validate_string_fragments(struct zcbor_string_fragment *fragments,
		uint_fast32_t num_fragments);

/** Assemble the fragments into a single string.
 *
 *  The fragments are copied in the order they appear, without regard for
 *  offset or total_len. To ensure that the fragments are correct, first
 *  validate with @ref zcbor_validate_string_fragments.
 *
 *  @param[in]     fragments      An array of string fragments. Cannot be NULL.
 *  @param[in]     num_fragments  The number of fragments in @p fragments.
 *  @param[out]    result         The buffer to place the assembled string into.
 *  @param[inout]  result_len     In: The length of the @p result.
 *                                Out: The length of the assembled string.
 *
 *  @retval  true   On success.
 *  @retval  false  If the assembled string would be larger than the buffer.
 *                  The buffer might still be written to.
 */
bool zcbor_splice_string_fragments(struct zcbor_string_fragment *fragments,
		uint_fast32_t num_fragments, uint8_t *result, size_t *result_len);

#ifdef __cplusplus
}
#endif

#endif /* ZCBOR_COMMON_H__ */
