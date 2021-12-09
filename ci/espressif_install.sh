#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

SCRIPT_ROOTDIR=$(dirname "$(realpath "${BASH_SOURCE[0]}")")
MCUBOOT_ROOTDIR=$(realpath "${SCRIPT_ROOTDIR}/..")
ESPRESSIF_ROOT="${MCUBOOT_ROOTDIR}/boot/espressif"
IDF_PATH="${ESPRESSIF_ROOT}/hal/esp-idf"

set -eo pipefail

install_imgtool() {
    pip install imgtool
}

install_idf() {
    "${IDF_PATH}"/install.sh
}

install_imgtool
install_idf
