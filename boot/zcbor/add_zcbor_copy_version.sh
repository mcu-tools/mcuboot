#!/bin/bash

if [ "$1" == "--help" ]; then
	echo "Add header if the zcbor files are updated."
	exit -1
fi

add_copy_notice() {
echo "$(printf '/*
 * This file has been %s from the zcbor library.
 * Commit %s
 */

' "$2" "$(zcbor --version)"; cat $1;)" > $1
}

add_copy_notice src/zcbor_decode.c "copied"
add_copy_notice src/zcbor_encode.c "copied"
add_copy_notice src/zcbor_common.c "copied"
add_copy_notice include/zcbor_decode.h "copied"
add_copy_notice include/zcbor_encode.h "copied"
add_copy_notice include/zcbor_common.h "copied"
add_copy_notice include/zcbor_print.h "copied"
add_copy_notice include/zcbor_tags.h "copied"
