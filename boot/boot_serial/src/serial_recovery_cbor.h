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

#ifndef SERIAL_RECOVERY_CBOR_H__
#define SERIAL_RECOVERY_CBOR_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "zcbor_decode.h"
#include "serial_recovery_cbor_types.h"

#if DEFAULT_MAX_QTY != 3
#error "The type file was generated with a different default_max_qty than this file"
#endif


int cbor_decode_Upload(
		const uint8_t *payload, size_t payload_len,
		struct Upload *result,
		size_t *payload_len_out);


#endif /* SERIAL_RECOVERY_CBOR_H__ */
