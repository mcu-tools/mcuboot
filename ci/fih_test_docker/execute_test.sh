#!/bin/bash -x

# Copyright (c) 2020-2023 Arm Limited
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e

# Function to update/install native GCC inside the Docker container
update_native_gcc() {
    REQUIRED_MAJOR=12
    INSTALLED_MAJOR=$(gcc -dumpversion | cut -d. -f1 || echo 0)

    if [[ "$INSTALLED_MAJOR" -lt "$REQUIRED_MAJOR" ]]; then
        echo "Installing native GCC $REQUIRED_MAJOR..."
        apt-get update
        apt-get install -y --no-install-recommends gcc-$REQUIRED_MAJOR g++-$REQUIRED_MAJOR \
            cpp-$REQUIRED_MAJOR libgcc-$REQUIRED_MAJOR-dev libstdc++-$REQUIRED_MAJOR-dev
        update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-$REQUIRED_MAJOR 60
        update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-$REQUIRED_MAJOR 60
        rm -rf /var/lib/apt/lists/*
    else
        echo "Native GCC is already version $INSTALLED_MAJOR; skipping installation."
    fi
}

# Function to update/install ARM Embedded GCC inside the Docker container
update_cross_gcc() {
    ARM_GCC_URL="https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz"
    TOOLCHAIN_DIR="/opt/arm-gcc"

    # Install prerequisites
    echo "Installing prerequisites for ARM toolchain..."
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        curl libncurses5 xz-utils file
    rm -rf /var/lib/apt/lists/*

    # Download and extract
    echo "Downloading and extracting ARM Embedded GCC..."
    mkdir -p "$TOOLCHAIN_DIR"
    curl -SLf "$ARM_GCC_URL" -o /tmp/arm-gcc.tar.xz
    tar -xJf /tmp/arm-gcc.tar.xz -C "$TOOLCHAIN_DIR" --strip-components=1
    rm -f /tmp/arm-gcc.tar.xz

    # Symlink into PATH
    echo "Symlinking ARM toolchain into /usr/local/bin..."
    ln -sf "$TOOLCHAIN_DIR/bin/"* /usr/local/bin/
}

# Ensure we have the proper compiler before running tests
update_native_gcc
update_cross_gcc

source $(dirname "$0")/paths.sh

SKIP_SIZE=$1
BUILD_TYPE=$2
DAMAGE_TYPE=$3
FIH_LEVEL=$4

# Required for git am to apply patches under TF-M
git config --global user.email "docker@fih-test.com"
git config --global user.name "fih-test docker"

if test -z "$FIH_LEVEL"; then
    # Use the default level
    CMAKE_FIH_LEVEL=""
else
    CMAKE_FIH_LEVEL="-DMCUBOOT_FIH_PROFILE=\"$FIH_LEVEL\""
fi

# build TF-M with MCUBoot
mkdir -p $TFM_BUILD_PATH $TFM_SPE_BUILD_PATH

cmake -S $TFM_TESTS_PATH/tests_reg/spe \
    -B $TFM_SPE_BUILD_PATH \
    -DTFM_PLATFORM=arm/mps2/an521 \
    -DCONFIG_TFM_SOURCE_PATH=$TFM_PATH \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DTFM_TOOLCHAIN_FILE=$TFM_PATH/toolchain_GNUARM.cmake \
    -DTEST_S=ON \
    -DTEST_NS=ON \
    -DTFM_PSA_API=ON \
    -DMCUBOOT_PATH=$MCUBOOT_PATH \
    -DMCUBOOT_LOG_LEVEL=INFO \
    $CMAKE_FIH_LEVEL
cmake --build $TFM_SPE_BUILD_PATH -- install

cmake -S $TFM_TESTS_PATH/tests_reg \
    -B $TFM_BUILD_PATH \
    -DCONFIG_SPE_PATH=$TFM_SPE_BUILD_PATH/api_ns \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DTFM_TOOLCHAIN_FILE=$TFM_SPE_BUILD_PATH/api_ns/cmake/toolchain_ns_GNUARM.cmake
cmake --build $TFM_BUILD_PATH

cd $TFM_BUILD_PATH
$MCUBOOT_PATH/ci/fih_test_docker/run_fi_test.sh $BOOTLOADER_AXF_PATH $SKIP_SIZE $DAMAGE_TYPE> fih_test_output.yaml

echo ""
echo "test finished with"
echo "    - BUILD_TYPE: $BUILD_TYPE"
echo "    - FIH_LEVEL: $FIH_LEVEL"
echo "    - SKIP_SIZE: $SKIP_SIZE"
echo "    - DAMAGE_TYPE: $DAMAGE_TYPE"

python3 $MCUBOOT_PATH/ci/fih_test_docker/generate_test_report.py fih_test_output.yaml
python3 $MCUBOOT_PATH/ci/fih_test_docker/validate_output.py fih_test_output.yaml $SKIP_SIZE $FIH_LEVEL
