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

source $(dirname "$0")/paths.sh

SKIP_SIZE=$1
BUILD_TYPE=$2
DAMAGE_TYPE=$3
FIH_LEVEL=$4

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
