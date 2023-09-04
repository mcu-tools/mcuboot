/*
 * This file has been copied from the zcbor library.
 * Commit zcbor 0.7.0
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
#include <assert.h>
#include "zcbor_common.h"

static_assert((sizeof(size_t) == sizeof(void *)),
	"This code needs size_t to be the same length as pointers.");

static_assert((sizeof(zcbor_state_t) >= sizeof(struct zcbor_state_constant)),
	"This code needs zcbor_state_t to be at least as large as zcbor_backups_t.");

bool zcbor_new_backup(zcbor_state_t *state, uint_fast32_t new_elem_count)
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
	uint_fast32_t i = (state->constant_state->current_backup) - 1;

	memcpy(&state->constant_state->backup_list[i], state,
		sizeof(zcbor_state_t));

	state->elem_count = new_elem_count;

	return true;
}


bool zcbor_process_backup(zcbor_state_t *state, uint32_t flags,
		uint_fast32_t max_elem_count)
{
	const uint8_t *payload = state->payload;
	const uint_fast32_t elem_count = state->elem_count;

	ZCBOR_CHECK_ERROR();

	if (state->constant_state->current_backup == 0) {
		zcbor_print("No backups available.\r\n");
		ZCBOR_ERR(ZCBOR_ERR_NO_BACKUP_ACTIVE);
	}

	if (flags & ZCBOR_FLAG_RESTORE) {
		/* use the backup at current_backup - 1, since otherwise, the
		 * 0th backup would be unused. */
		uint_fast32_t i = state->constant_state->current_backup - 1;

		if (!(flags & ZCBOR_FLAG_TRANSFER_PAYLOAD)) {
			if (state->constant_state->backup_list[i].payload_moved) {
				zcbor_print("Payload pointer out of date.\r\n");
				ZCBOR_FAIL();
			}
		}
		memcpy(state, &state->constant_state->backup_list[i],
			sizeof(zcbor_state_t));
	}

	if (flags & ZCBOR_FLAG_CONSUME) {
		state->constant_state->current_backup--;
	}

	if (elem_count > max_elem_count) {
		zcbor_print("elem_count: %" PRIuFAST32 " (expected max %" PRIuFAST32 ")\r\n",
			elem_count, max_elem_count);
		ZCBOR_ERR(ZCBOR_ERR_HIGH_ELEM_COUNT);
	}

	if (flags & ZCBOR_FLAG_TRANSFER_PAYLOAD) {
		state->payload = payload;
	}

	return true;
}

static void update_backups(zcbor_state_t *state, uint8_t const *new_payload_end)
{
	if (state->constant_state) {
		for (int i = 0; i < state->constant_state->current_backup; i++) {
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

void zcbor_new_state(zcbor_state_t *state_array, uint_fast32_t n_states,
		const uint8_t *payload, size_t payload_len, uint_fast32_t elem_count)
{
	state_array[0].payload = payload;
	state_array[0].payload_end = payload + payload_len;
	state_array[0].elem_count = elem_count;
	state_array[0].indefinite_length_array = false;
	state_array[0].payload_moved = false;
	state_array[0].constant_state = NULL;

	if(n_states < 2) {
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
		uint_fast32_t num_fragments)
{
	size_t total_len = 0;

	if (fragments == NULL) {
		return false;
	}

	for (uint_fast32_t i = 0; i < num_fragments; i++) {
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
		for (uint_fast32_t i = 0; i < num_fragments; i++) {
			fragments[i].total_len = total_len;
		}
	}

	return true;
}

bool zcbor_splice_string_fragments(struct zcbor_string_fragment *fragments,
		uint_fast32_t num_fragments, uint8_t *result, size_t *result_len)
{
	size_t total_len = 0;

	if (!fragments) {
		return false;
	}

	for (uint_fast32_t i = 0; i < num_fragments; i++) {
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
