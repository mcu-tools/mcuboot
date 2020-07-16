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

TFM_DIR=/root/work/tfm/trusted-firmware-m
TFM_BUILD_DIR=$TFM_DIR/build
MCUBOOT_AXF=install/outputs/MPS2/AN521/bl2.axf
SIGNED_TFM_BIN=install/outputs/MPS2/AN521/tfm_s_ns_signed.bin
QEMU_LOG_FILE=qemu.log
QEMU_PID_FILE=qemu_pid.txt

source ~/.bashrc

# build TF-M with MCUBoot
mkdir -p $TFM_BUILD_DIR
cd $TFM_DIR
cmake -B $TFM_BUILD_DIR \
    -DCMAKE_BUILD_TYPE=Release \
    -DTFM_TOOLCHAIN_FILE=toolchain_GNUARM.cmake \
    -DTFM_PLATFORM=mps2/an521 \
    -DTEST_NS=ON \
    -DTEST_S=ON \
    -DTFM_PSA_API=ON \
    -DMCUBOOT_PATH=$MCUBOOT_PATH \
    -DMCUBOOT_LOG_LEVEL=INFO \
    .
cd $TFM_BUILD_DIR
make -j install

# Run MCUBoot and TF-M in QEMU
/usr/bin/qemu-system-arm \
    -M mps2-an521 \
    -kernel $MCUBOOT_AXF \
    -device loader,file=$SIGNED_TFM_BIN,addr=0x10080000 \
    -chardev file,id=char0,path=$QEMU_LOG_FILE \
    -serial chardev:char0 \
    -display none \
    -pidfile $QEMU_PID_FILE \
    -daemonize

sleep 7

cat $QEMU_LOG_FILE