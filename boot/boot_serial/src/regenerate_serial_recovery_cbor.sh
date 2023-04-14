#!/bin/bash

if [ "$1" == "--help" ] || [ "$1" == "" ]; then
	echo "Regenerate serial_recovery_cbor.c|h if the zcbor submodule is updated."
	echo "Usage: $0 <copyright>"
	echo "  e.g. $0 \"2022 Nordic Semiconductor ASA\""
	exit -1
fi

add_copy_notice() {
echo "$(printf '/*
 * This file has been %s from the zcbor library.
 * Commit %s
 */

' "$2" "$(zcbor --version)"; cat $1;)" > $1
}

echo "Copying zcbor_decode.c|h"
copy_with_copy_notice() {
	cp $1 $2
	add_copy_notice $2 "copied"
}


echo "Generating serial_recovery_cbor.c|h"
zcbor code -c serial_recovery.cddl -d -t Upload --oc serial_recovery_cbor.c --oh serial_recovery_cbor.h --time-header --copy-sources

add_copyright() {
echo "$(printf '/*
 * Copyright (c) %s
 *
 * SPDX-License-Identifier: Apache-2.0
 */

' "$2"; cat $1;)" > $1
}

add_copyright serial_recovery_cbor.c "$1"
add_copyright serial_recovery_cbor.h "$1"
add_copyright serial_recovery_cbor_types.h "$1"
add_copy_notice zcbor_decode.c "copied"
add_copy_notice zcbor_encode.c "copied"
add_copy_notice zcbor_common.c "copied"
add_copy_notice zcbor_decode.h "copied"
add_copy_notice zcbor_encode.h "copied"
add_copy_notice zcbor_common.h "copied"
