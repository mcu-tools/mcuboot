/*
 * This file has been generated from the cddl_gen submodule.
 * Commit 9d911cf0c7c9f13b5a9fdd5ed6c1012df21e5576
 */

/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Generated with cddl_gen.py (https://github.com/oyvindronningstad/cddl_gen)
 * at: 2020-05-13 12:19:04
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "cbor_decode.h"
#include "serial_recovery_cbor.h"


static bool decode_Member(
		cbor_decode_state_t *p_state, void * p_result, void * p_min_value,
		void * p_max_value)
{
	cbor_decode_print("decode_Member\n");
	uint8_t const * p_payload_bak;
	size_t elem_count_bak;
	_Member_t* p_type_result = (_Member_t*)p_result;

	bool result = (((p_payload_bak = p_state->p_payload) && ((elem_count_bak = p_state->elem_count) || 1) && ((((strx_decode(p_state, &((*p_type_result)._Member_image_key), NULL, NULL))&& !memcmp("image", (*p_type_result)._Member_image_key.value, (*p_type_result)._Member_image_key.len)
	&& (intx32_decode(p_state, &((*p_type_result)._Member_image), NULL, NULL))) && (((*p_type_result)._Member_choice = _Member_image) || 1))
	|| ((p_state->p_payload = p_payload_bak) && ((p_state->elem_count = elem_count_bak) || 1) && (((strx_decode(p_state, &((*p_type_result)._Member_data_key), NULL, NULL))&& !memcmp("data", (*p_type_result)._Member_data_key.value, (*p_type_result)._Member_data_key.len)
	&& (strx_decode(p_state, &((*p_type_result)._Member_data), NULL, NULL))) && (((*p_type_result)._Member_choice = _Member_data) || 1)))
	|| ((p_state->p_payload = p_payload_bak) && ((p_state->elem_count = elem_count_bak) || 1) && (((strx_decode(p_state, &((*p_type_result)._Member_len_key), NULL, NULL))&& !memcmp("len", (*p_type_result)._Member_len_key.value, (*p_type_result)._Member_len_key.len)
	&& (intx32_decode(p_state, &((*p_type_result)._Member_len), NULL, NULL))) && (((*p_type_result)._Member_choice = _Member_len) || 1)))
	|| ((p_state->p_payload = p_payload_bak) && ((p_state->elem_count = elem_count_bak) || 1) && (((strx_decode(p_state, &((*p_type_result)._Member_off_key), NULL, NULL))&& !memcmp("off", (*p_type_result)._Member_off_key.value, (*p_type_result)._Member_off_key.len)
	&& (intx32_decode(p_state, &((*p_type_result)._Member_off), NULL, NULL))) && (((*p_type_result)._Member_choice = _Member_off) || 1)))
	|| ((p_state->p_payload = p_payload_bak) && ((p_state->elem_count = elem_count_bak) || 1) && (((strx_decode(p_state, &((*p_type_result)._Member_sha_key), NULL, NULL))&& !memcmp("sha", (*p_type_result)._Member_sha_key.value, (*p_type_result)._Member_sha_key.len)
	&& (strx_decode(p_state, &((*p_type_result)._Member_sha), NULL, NULL))) && (((*p_type_result)._Member_choice = _Member_sha) || 1))))));

	if (!result)
	{
		cbor_decode_trace();
	}

	return result;
}

static bool decode_Upload(
		cbor_decode_state_t *p_state, void * p_result, void * p_min_value,
		void * p_max_value)
{
	cbor_decode_print("decode_Upload\n");
	size_t temp_elem_counts[2];
	size_t *p_temp_elem_count = temp_elem_counts;
	Upload_t* p_type_result = (Upload_t*)p_result;

	bool result = (((list_start_decode(p_state, &(*(p_temp_elem_count++)), 1, 5))
	&& multi_decode(1, 5, &(*p_type_result)._Upload_members_count, (void*)decode_Member, p_state, &((*p_type_result)._Upload_members), NULL, NULL, sizeof(_Member_t))
	&& ((p_state->elem_count = *(--p_temp_elem_count)) || 1)));

	if (!result)
	{
		cbor_decode_trace();
	}

	p_state->elem_count = temp_elem_counts[0];
	return result;
}


bool cbor_decode_Upload(const uint8_t * p_payload, size_t payload_len, Upload_t * p_result)
{
	cbor_decode_state_t state = {
		.p_payload = p_payload,
		.p_payload_end = p_payload + payload_len,
		.elem_count = 1
	};

	return decode_Upload(&state, p_result, NULL, NULL);
}
