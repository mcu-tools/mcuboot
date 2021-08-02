/*
 * This file has been generated from the cddl-gen submodule.
 * Commit 9f77837f9950da1633d22abf6181a830521a6688
 */

/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Generated with cddl_gen.py (https://github.com/NordicSemiconductor/cddl-gen)
 * at: 2021-08-02 17:09:42
 * Generated with a default_max_qty of 3
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "cbor_decode.h"
#include "serial_recovery_cbor.h"

#if DEFAULT_MAX_QTY != 3
#error "The type file was generated with a different default_max_qty than this file"
#endif


static bool decode_Member(
		cbor_state_t *state, struct Member_ *result)
{
	cbor_print("%s\n", __func__);
	cbor_string_type_t tmp_str;
	bool int_res;

	bool tmp_result = (((union_start_code(state) && (int_res = (((((tstrx_expect(state, ((tmp_str.value = (const uint8_t *)"image",
    tmp_str.len = sizeof("image") - 1, &tmp_str)))))
	&& (intx32_decode(state, (&(*result)._Member_image)))) && (((*result)._Member_choice = _Member_image) || 1))
	|| (union_elem_code(state) && ((((tstrx_expect(state, ((tmp_str.value = (const uint8_t *)"data",
    tmp_str.len = sizeof("data") - 1, &tmp_str)))))
	&& (bstrx_decode(state, (&(*result)._Member_data)))) && (((*result)._Member_choice = _Member_data) || 1)))
	|| (union_elem_code(state) && ((((tstrx_expect(state, ((tmp_str.value = (const uint8_t *)"len",
    tmp_str.len = sizeof("len") - 1, &tmp_str)))))
	&& (intx32_decode(state, (&(*result)._Member_len)))) && (((*result)._Member_choice = _Member_len) || 1)))
	|| (union_elem_code(state) && ((((tstrx_expect(state, ((tmp_str.value = (const uint8_t *)"off",
    tmp_str.len = sizeof("off") - 1, &tmp_str)))))
	&& (intx32_decode(state, (&(*result)._Member_off)))) && (((*result)._Member_choice = _Member_off) || 1)))
	|| (union_elem_code(state) && ((((tstrx_expect(state, ((tmp_str.value = (const uint8_t *)"sha",
    tmp_str.len = sizeof("sha") - 1, &tmp_str)))))
	&& (bstrx_decode(state, (&(*result)._Member_sha)))) && (((*result)._Member_choice = _Member_sha) || 1)))), union_end_code(state), int_res))));

	if (!tmp_result)
		cbor_trace();

	return tmp_result;
}

static bool decode_Upload(
		cbor_state_t *state, struct Upload *result)
{
	cbor_print("%s\n", __func__);
	bool int_res;

	bool tmp_result = (((map_start_decode(state) && (int_res = (multi_decode(1, 5, &(*result)._Upload_members_count, (void *)decode_Member, state, (&(*result)._Upload_members), sizeof(struct Member_))), ((map_end_decode(state)) && int_res)))));

	if (!tmp_result)
		cbor_trace();

	return tmp_result;
}



__attribute__((unused)) static bool type_test_decode_Upload(
		struct Upload *result)
{
	/* This function should not be called, it is present only to test that
	 * the types of the function and struct match, since this information
	 * is lost with the casts in the entry function.
	 */
	return decode_Upload(NULL, result);
}


bool cbor_decode_Upload(
		const uint8_t *payload, uint32_t payload_len,
		struct Upload *result,
		uint32_t *payload_len_out)
{
	return entry_function(payload, payload_len, (const void *)result,
		payload_len_out, (void *)decode_Upload,
		1, 2);
}
