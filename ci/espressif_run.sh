#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

SCRIPT_ROOTDIR=$(dirname "$(realpath "${BASH_SOURCE[0]}")")
MCUBOOT_ROOTDIR=$(realpath "${SCRIPT_ROOTDIR}/..")
ESPRESSIF_ROOT="${MCUBOOT_ROOTDIR}/boot/espressif"
IDF_PATH="${HOME}/esp-idf"

set -eo pipefail

prepare_environment() {
  # Prepare the environment for ESP-IDF

  . "${IDF_PATH}"/export.sh
}

build_mcuboot() {
  local target=${1}
  local feature=${2}
  local img_num=${3}
  local build_dir=".build-${target}"
  local toolchain_file="${ESPRESSIF_ROOT}/tools/toolchain-${target}.cmake"

  if [ -n "$img_num" ]; then
    img_num="-${img_num}"
  fi
  local mcuboot_config="${ESPRESSIF_ROOT}/port/${target}/bootloader${img_num}.conf"

  if [ -n "${feature}" ]; then
    mcuboot_config="${mcuboot_config};${ESPRESSIF_ROOT}/ci_configs/${feature}.conf"
    build_dir=".build-${target}-${feature}"
  fi

  # Build MCUboot for selected target

  cd "${MCUBOOT_ROOTDIR}" &>/dev/null
  cmake -DCMAKE_TOOLCHAIN_FILE="${toolchain_file}"  \
        -DMCUBOOT_TARGET="${target}"                \
        -DMCUBOOT_CONFIG_FILE="${mcuboot_config}"   \
        -DESP_HAL_PATH="${IDF_PATH}"                \
        -B "${build_dir}"                           \
        "${ESPRESSIF_ROOT}"
  cmake --build "${build_dir}"/
}

prepare_environment

if [ -n "${MCUBOOT_FEATURES}" ]; then
  IFS=','
  read -ra target_list <<< "${MCUBOOT_TARGETS}"
  read img_num <<< "${MCUBOOT_IMG_NUM}"
  for target in "${target_list[@]}"; do
    read -ra feature_list <<< "${MCUBOOT_FEATURES}"
    for feature in "${feature_list[@]}"; do
      echo "Building MCUboot for \"${target}\" with support for \"${feature}\""
      build_mcuboot "${target}" "${feature}" "${img_num}"
    done
  done
fi
