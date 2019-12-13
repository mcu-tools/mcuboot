#!/bin/bash

if [ "$1" == "--help" ] || [ "$1" == "" ]; then
	echo "Regenerate serial_recovery_cbor.c|h if the cddl_gen submodule is updated."
	echo "Usage: $0 <copyright>"
	echo "  e.g. $0 \"2020 Nordic Semiconductor ASA\""
	exit -1
fi

add_copy_notice() {
echo "$(printf '/*
 * This file has been %s from the cddl_gen submodule.
 * Commit %s
 */

' "$2" "$(git -C ../../../ext/cddl_gen rev-parse HEAD)"; cat $1;)" > $1
}

echo "Copying cbor_decode.c|h"
copy_with_copy_notice() {
	cp $1 $2
	add_copy_notice $2 "copied"
}

copy_with_copy_notice ../../../ext/cddl_gen/src/cbor_decode.c cbor_decode.c
copy_with_copy_notice ../../../ext/cddl_gen/include/cbor_decode.h cbor_decode.h cbor_decode.h

echo "Generating serial_recovery_cbor.c|h"
python3 ../../../ext/cddl_gen/scripts/cddl_gen.py -i serial_recovery.cddl -t Upload --oc serial_recovery_cbor.c --oh serial_recovery_cbor.h --time-header

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
add_copy_notice serial_recovery_cbor.c "generated"
add_copy_notice serial_recovery_cbor.h "generated"
