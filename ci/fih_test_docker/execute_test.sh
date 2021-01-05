#!/bin/bash -x

# Copyright (c) 2020 Arm Limited
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

WORKING_DIRECTORY=/root/work/tfm
MCUBOOT_PATH=$WORKING_DIRECTORY/mcuboot
TFM_DIR=$WORKING_DIRECTORY/trusted-firmware-m
TFM_BUILD_DIR=$TFM_DIR/build

SKIP_SIZE=$1
BUILD_TYPE=$2
DAMAGE_TYPE=$3
FIH_LEVEL=$4

source ~/.bashrc

if test -z "$FIH_LEVEL"; then
    # Use the default level
    CMAKE_FIH_LEVEL=""
else
    CMAKE_FIH_LEVEL="-DMCUBOOT_FIH_PROFILE=\"$FIH_LEVEL\""
fi

# build TF-M with MCUBoot
mkdir -p $TFM_BUILD_DIR
cd $TFM_DIR
cmake -B $TFM_BUILD_DIR \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DTFM_TOOLCHAIN_FILE=toolchain_GNUARM.cmake \
    -DTFM_PLATFORM=mps2/an521 \
    -DTEST_NS=ON \
    -DTEST_S=ON \
    -DTFM_PSA_API=ON \
    -DMCUBOOT_PATH=$MCUBOOT_PATH \
    -DMCUBOOT_LOG_LEVEL=INFO \
    -DTFM_TEST_REPO_VERSION=93ce2f59c0c4a9cba6062834496b5f45deee4010 \
    $CMAKE_FIH_LEVEL \
    .
cd $TFM_BUILD_DIR
make -j install

BOOTLOADER_AXF='./install/outputs/MPS2/AN521/bl2.axf'

$MCUBOOT_PATH/ci/fih_test_docker/run_fi_test.sh $BOOTLOADER_AXF $SKIP_SIZE $DAMAGE_TYPE> fih_test_output.yaml

echo ""
echo "test finished with"
echo "    - BUILD_TYPE: $BUILD_TYPE"
echo "    - FIH_LEVEL: $FIH_LEVEL"
echo "    - SKIP_SIZE: $SKIP_SIZE"
echo "    - DAMAGE_TYPE: $DAMAGE_TYPE"

python3 $MCUBOOT_PATH/ci/fih_test_docker/generate_test_report.py fih_test_output.yaml
