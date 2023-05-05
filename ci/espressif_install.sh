#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

set -eo pipefail

install_imgtool() {
    pip install imgtool
}

install_idf() {
    pushd $HOME
    git clone --depth=1 https://github.com/espressif/esp-idf.git --branch release/v5.1
    [[ $? -ne 0 ]] && exit 1

    $HOME/esp-idf/install.sh
    [[ $? -ne 0 ]] && exit 1

    popd
}

install_imgtool
install_idf
