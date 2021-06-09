/*
 * This file has been generated from the cddl-gen submodule.
 * Commit 8f9358a0b4b0e9b0cd579f0988056ef0b60760e4
 */

/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Generated with cddl_gen.py (https://github.com/NordicSemiconductor/cddl-gen)
 * at: 2021-05-10 09:40:43
 * Generated with a default_max_qty of 3
 */

#ifndef SERIAL_RECOVERY_CBOR_H__
#define SERIAL_RECOVERY_CBOR_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "cbor_decode.h"
#include "types_serial_recovery_cbor.h"

#if DEFAULT_MAX_QTY != 3
#error "The type file was generated with a different default_max_qty than this file"
#endif


bool cbor_decode_Upload(
		const uint8_t *payload, uint32_t payload_len,
		struct Upload *result,
		uint32_t *payload_len_out);


#endif /* SERIAL_RECOVERY_CBOR_H__ */
