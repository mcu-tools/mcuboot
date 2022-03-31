/*
 * This file has been copied from the cddl-gen submodule.
 * Commit 9f77837f9950da1633d22abf6181a830521a6688
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
#include "cbor_common.h"

_Static_assert((sizeof(size_t) == sizeof(void *)),
	"This code needs size_t to be the same length as pointers.");

bool new_backup(cbor_state_t *state, uint32_t new_elem_count)
{
	if ((state->backups->current_backup + 1)
		>= state->backups->num_backups) {
		FAIL();
	}

	uint32_t i = ++(state->backups->current_backup);
	memcpy(&state->backups->backup_list[i], state,
		sizeof(cbor_state_t));

	state->elem_count = new_elem_count;

	return true;
}


bool restore_backup(cbor_state_t *state, uint32_t flags,
		uint32_t max_elem_count)
{
	const uint8_t *payload = state->payload;
	const uint32_t elem_count = state->elem_count;

	if (state->backups->current_backup == 0) {
		FAIL();
	}

	if (flags & FLAG_RESTORE) {
		uint32_t i = state->backups->current_backup;

		memcpy(state, &state->backups->backup_list[i],
			sizeof(cbor_state_t));
	}

	if (flags & FLAG_DISCARD) {
		state->backups->current_backup--;
	}

	if (elem_count > max_elem_count) {
		cbor_print("elem_count: %d (expected max %d)\r\n",
			elem_count, max_elem_count);
		FAIL();
	}

	if (flags & FLAG_TRANSFER_PAYLOAD) {
		state->payload = payload;
	}

	return true;
}


bool union_start_code(cbor_state_t *state)
{
	if (!new_backup(state, state->elem_count)) {
		FAIL();
	}
	return true;
}


bool union_elem_code(cbor_state_t *state)
{
	if (!restore_backup(state, FLAG_RESTORE, state->elem_count)) {
		FAIL();
	}
	return true;
}

bool union_end_code(cbor_state_t *state)
{
	if (!restore_backup(state, FLAG_DISCARD, state->elem_count)) {
		FAIL();
	}
	return true;
}

bool entry_function(const uint8_t *payload, uint32_t payload_len,
		const void *struct_ptr, uint32_t *payload_len_out,
		cbor_encoder_t func, uint32_t elem_count, uint32_t num_backups)
{
	cbor_state_t state = {
		.payload = payload,
		.payload_end = payload + payload_len,
		.elem_count = elem_count,
	};

	cbor_state_t state_backups[num_backups + 1];

	cbor_state_backups_t backups = {
		.backup_list = state_backups,
		.current_backup = 0,
		.num_backups = num_backups + 1,
	};

	state.backups = &backups;

	bool result = func(&state, struct_ptr);

	if (result && (payload_len_out != NULL)) {
		*payload_len_out = MIN(payload_len,
				(size_t)state.payload - (size_t)payload);
	}
	return result;
}
