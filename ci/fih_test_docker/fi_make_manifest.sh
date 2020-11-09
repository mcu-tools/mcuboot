#!/bin/bash

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

OBJDUMP=arm-none-eabi-objdump
GDB=gdb-multiarch

# Check if the ELF file specified is compatible
if test $# -eq 0 || ! file $1 | grep "ELF" | grep "ARM" | grep "32" &>/dev/null; then
    echo "Incompatible file: $1" 1>&2
    exit 1
fi

# Extract the full path
AXF_PATH=$(realpath $1)
#Dump all objects that have a name containing FIH_LABEL
ADDRESSES=$($OBJDUMP $AXF_PATH -t | grep "FIH_LABEL")
# strip all data except "address, label_name"
ADDRESSES=$(echo "$ADDRESSES" | sed "s/\([[:xdigit:]]*\).*\(FIH_LABEL_FIH_CALL_[a-zA-Z]*\)_.*/0x\1, \2/g")
# Sort by address in ascending order
ADDRESSES=$(echo "$ADDRESSES" | sort)
# In the case that there is a START followed by another START take the first one
ADDRESSES=$(echo "$ADDRESSES" | sed "N;s/\(.*START.*\)\n\(.*START.*\)/\1/;P;D")
# Same for END except take the second one
ADDRESSES=$(echo "$ADDRESSES" | sed "N;s/\(.*END.*\)\n\(.*END.*\)/\2/;P;D")

# Output in CSV format with a label
echo "Address, Type"
echo "$ADDRESSES"
