#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

SCRIPT_ROOTDIR=$(dirname "$(realpath "${BASH_SOURCE[0]}")")
MCUBOOT_ROOTDIR=$(realpath "${SCRIPT_ROOTDIR}/..")
ESPRESSIF_ROOT="${MCUBOOT_ROOTDIR}/boot/espressif"
IDF_PATH="${ESPRESSIF_ROOT}/hal/esp-idf"

set -eo pipefail

build_mcuboot() {
  local target=${MCUBOOT_TARGET}
  local build_dir=".build-${target}"
  local toolchain_file="${ESPRESSIF_ROOT}/tools/toolchain-${target}.cmake"
  local mcuboot_config="${ESPRESSIF_ROOT}/bootloader.conf"

  # Prepare the environment for ESP-IDF

  . "${IDF_PATH}"/export.sh

  # Build MCUboot for selected target

  cd "${MCUBOOT_ROOTDIR}" &>/dev/null
  cmake -DCMAKE_TOOLCHAIN_FILE="${toolchain_file}"  \
        -DMCUBOOT_TARGET="${target}"                \
        -DMCUBOOT_CONFIG_FILE="${mcuboot_config}"   \
        -DIDF_PATH="${IDF_PATH}"                    \
        -B "${build_dir}"                           \
        "${ESPRESSIF_ROOT}"
  cmake --build "${build_dir}"/
}

build_mcuboot
