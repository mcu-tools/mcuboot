/*
 * Generated using zcbor version 0.4.99
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 3
 */

#ifndef SERIAL_RECOVERY_ECHO_TYPES_H__
#define SERIAL_RECOVERY_ECHO_TYPES_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <zcbor_decode.h>

/** Which value for --default-max-qty this file was created with.
 *
 *  The define is used in the other generated file to do a build-time
 *  compatibility check.
 *
 *  See `zcbor --help` for more information about --default-max-qty
 */
#define DEFAULT_MAX_QTY 3

struct Echo {
	struct zcbor_string _Echo_d;
};


#endif /* SERIAL_RECOVERY_ECHO_TYPES_H__ */
