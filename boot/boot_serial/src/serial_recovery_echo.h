/*
 * Generated using zcbor version 0.4.99
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 3
 */

#ifndef SERIAL_RECOVERY_ECHO_H__
#define SERIAL_RECOVERY_ECHO_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __ZEPHYR__
#include <zcbor_decode.h>
#else
#include "zcbor_decode.h"
#endif

#include "serial_recovery_echo_types.h"

#if DEFAULT_MAX_QTY != 3
#error "The type file was generated with a different default_max_qty than this file"
#endif


int cbor_decode_Echo(
		const uint8_t *payload, size_t payload_len,
		struct Echo *result,
		size_t *payload_len_out);


#endif /* SERIAL_RECOVERY_ECHO_H__ */
