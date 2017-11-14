#! /bin/bash

# Build the Sample hello program

# In order to build successfully, ZEPHYR_SDK_INSTALL_DIR and
# ZEPHYR_GCC_VARIANT need to be set, as well as zephyr/zephyr-env.sh
# must be sourced.

die() {
    echo error: "$@"
    exit 1
}

if [ -z "$ZEPHYR_BASE" ]; then
    die "Please setup for a Zephyr build before running this script."
fi

if [ -z "$BOARD" ]; then
    die "Please set BOARD to a valid board before running this script."
fi

make BOARD=${BOARD} -j$(nproc) hello1 || die "Build hello1"
