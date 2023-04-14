/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Generated using zcbor version 0.4.0
 * https://github.com/NordicSemiconductor/zcbor
 * at: 2022-03-31 12:37:11
 * Generated with a --default-max-qty of 3
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __ZEPHYR__
#include <zcbor_decode.h>
#else
#include "zcbor_decode.h"
#endif

#include "serial_recovery_cbor.h"

#if DEFAULT_MAX_QTY != 3
#error "The type file was generated with a different default_max_qty than this file"
#endif


static bool decode_Member(
		zcbor_state_t *state, struct Member_ *result)
{
	zcbor_print("%s\r\n", __func__);
	struct zcbor_string tmp_str;
	bool int_res;

	bool tmp_result = (((zcbor_union_start_code(state) && (int_res = (((((zcbor_tstr_expect(state, ((tmp_str.value = (uint8_t *)"image", tmp_str.len = sizeof("image") - 1, &tmp_str)))))
	&& (zcbor_int32_decode(state, (&(*result)._Member_image)))) && (((*result)._Member_choice = _Member_image) || 1))
	|| (zcbor_union_elem_code(state) && ((((zcbor_tstr_expect(state, ((tmp_str.value = (uint8_t *)"data", tmp_str.len = sizeof("data") - 1, &tmp_str)))))
	&& (zcbor_bstr_decode(state, (&(*result)._Member_data)))) && (((*result)._Member_choice = _Member_data) || 1)))
	|| (zcbor_union_elem_code(state) && ((((zcbor_tstr_expect(state, ((tmp_str.value = (uint8_t *)"len", tmp_str.len = sizeof("len") - 1, &tmp_str)))))
	&& (zcbor_int32_decode(state, (&(*result)._Member_len)))) && (((*result)._Member_choice = _Member_len) || 1)))
	|| (zcbor_union_elem_code(state) && ((((zcbor_tstr_expect(state, ((tmp_str.value = (uint8_t *)"off", tmp_str.len = sizeof("off") - 1, &tmp_str)))))
	&& (zcbor_int32_decode(state, (&(*result)._Member_off)))) && (((*result)._Member_choice = _Member_off) || 1)))
	|| (zcbor_union_elem_code(state) && ((((zcbor_tstr_expect(state, ((tmp_str.value = (uint8_t *)"sha", tmp_str.len = sizeof("sha") - 1, &tmp_str)))))
	&& (zcbor_bstr_decode(state, (&(*result)._Member_sha)))) && (((*result)._Member_choice = _Member_sha) || 1)))), zcbor_union_end_code(state), int_res))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_repeated_Upload_members(
		zcbor_state_t *state, struct Upload_members *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((decode_Member(state, (&(*result)._Upload_members)))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_Upload(
		zcbor_state_t *state, struct Upload *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_map_start_decode(state) && ((zcbor_multi_decode(1, 5, &(*result)._Upload_members_count, (zcbor_decoder_t *)decode_repeated_Upload_members, state, (&(*result)._Upload_members), sizeof(struct Upload_members))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_map_end_decode(state))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}



int cbor_decode_Upload(
		const uint8_t *payload, size_t payload_len,
		struct Upload *result,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = decode_Upload(states, result);

	if (ret && (payload_len_out != NULL)) {
		*payload_len_out = MIN(payload_len,
				(size_t)states[0].payload - (size_t)payload);
	}

	if (!ret) {
		int status = zcbor_pop_error(states);
		return (status == ZCBOR_SUCCESS) ? ZCBOR_ERR_UNKNOWN : status;
	}
	return ZCBOR_SUCCESS;
}
