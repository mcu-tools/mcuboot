#!/bin/bash

# Copyright (c) 2021 Arm Limited
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

function skip_instruction {

    local SKIP_ADDRESS=$1
    local SKIP_SIZE=$2

    # Parse the ASM instruction from the address using gdb
    INSTR=$($GDB $AXF_FILE --batch -ex "disassemble $SKIP_ADDRESS" | grep "^ *$SKIP_ADDRESS" | sed "s/.*:[ \t]*\(.*\)$/\1/g")
    # Parse the C line from the address using gdb
    LINE=$($GDB $AXF_FILE --batch -ex "info line *$SKIP_ADDRESS" | sed "s/Line \([0-9]*\).*\"\(.*\)\".*/\2:\1/g")

    # Sometimes an address is in the middle of a 4 byte instruction. In that case
    # don't run the test
    if test "$INSTR" == ""; then
        return
    fi

    # Print out the meta-info about the test, in YAML
    echo "- skip_test:"
    echo "    addr: $SKIP_ADDRESS"
    echo "    asm:  \"$INSTR\""
    echo "    line: \"$LINE\""
    echo "    skip: $SKIP_SIZE"
    # echo -ne "$SKIP_ADDRESS | $INSTR...\t"

    cat >commands.gdb <<EOF
target remote localhost: 1234
b *$SKIP_ADDRESS
continue&
eval "shell sleep 0.5"
interrupt
if \$pc == $SKIP_ADDRESS
    echo "Stopped at breakpoint"
else
    echo "Failed to stop at breakpoint"
end
echo "PC before increase:"
print \$pc
set \$pc += $SKIP_SIZE
echo "PC after increase:"
print \$pc
detach
eval "shell sleep 0.5"
EOF

    echo -n '.' 1>&2

    # start qemu, dump the serial output to $QEMU_LOG_FILE
    QEMU_LOG_FILE=qemu.log
    QEMU_PID_FILE=qemu_pid.txt
    rm -f $QEMU_PID_FILE $QEMU_LOG_FILE
    /usr/bin/qemu-system-arm \
        -M mps2-an521 \
        -s -S \
        -kernel $IMAGE_DIR/bl2.axf \
        -device loader,file=$IMAGE_DIR/tfm_s_ns_signed.bin,addr=0x10080000 \
        -chardev file,id=char0,path=$QEMU_LOG_FILE \
        -serial chardev:char0 \
        -display none \
        -pidfile $QEMU_PID_FILE \
        -daemonize

    # start qemu, skip the instruction, and continue execution
    $GDB < ./commands.gdb &>gdb_out.txt

    # kill qemu
    kill -9 `cat $QEMU_PID_FILE`

    # If "Secure image initializing" is seen the TFM booted, which means that a skip
    # managed to defeat the signature check. Write out whether the image booted or
    # not to the log in YAML
    if cat $QEMU_LOG_FILE | grep -i "Starting bootloader" &>/dev/null; then
        # bootloader started successfully
        if cat gdb_out.txt | grep -i "Stopped at breakpoint" &>/dev/null; then
            # The target was stopped at the desired address
            if cat $QEMU_LOG_FILE | grep -i "Secure image initializing" &>/dev/null; then
                echo "    test_exec_ok: True"
                echo "    skipped: True"
                echo "    boot: True"

                #print the address that was skipped, and some context to the console
                echo "" 1>&2
                echo "Boot success: address: $SKIP_ADDRESS skipped: $SKIP_SIZE" 1>&2
                arm-none-eabi-objdump -d $IMAGE_DIR/bl2.axf --start-address=$SKIP_ADDRESS -S | tail -n +7 | head -n 14 1>&2
                echo "" 1>&2
                echo "" 1>&2
            else
                LAST_LINE=`tail -n 1 $QEMU_LOG_FILE | tr -dc '[:print:]'`
                echo "    test_exec_ok: True"
                echo "    skipped: True"
                echo "    boot: False"
                echo "    last_line: \"$LAST_LINE\" "
            fi
        else
            # The target was not stopped at the desired address.
            # The most probable reason is that the instruction for that address is
            # on a call path that is not taken in this run (e.g. error handling)
            if cat $QEMU_LOG_FILE | grep -i "Secure image initializing" &>/dev/null; then
                # The image booted, although it shouldn't happen as the test is to
                # be run with a corrupt image.
                echo "    test_exec_ok: False"
                echo "    test_exec_fail_reason: \"No instructions were skipped (e.g. branch was not executed), but booted successfully\""
            else
                # the execution didn't stop at the address (e.g. the instruction
                # is on a branch that is not taken)
                echo "    test_exec_ok: True"
                echo "    skipped: False"
            fi
        fi
    else
        # failed before the first printout
        echo "    test_exec_ok: True"
        echo "    skipped: True"
        echo "    boot: False"
        echo "    last_line: 'N/A' "
    fi
}

# Inform how the script is used
usage() {
    echo "$0 <image_dir> <start_addr> [<end_addr>] [(-s | --skip) <skip_len>]"
}

#defaults
SKIP=2
BIN_DIR=$(pwd)/install/outputs/MPS2/AN521
AXF_FILE=$BIN_DIR/bl2.axf
GDB=gdb-multiarch
BOOTLOADER=true

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -s|--skip)
        SKIP="$2"
        shift
        shift
        ;;
        -h|--help)
        usage
        exit 0
        ;;
        *)
        if test -z "$IMAGE_DIR"; then
            IMAGE_DIR=$1
        elif test -z "$START"; then
            START=$1
        elif test -z "$END"; then
            END=$1
        else
            usage
            exit 1
        fi
        shift
        ;;
    esac
done

# Check that image directory, start and end address have been supplied
if test -z "$IMAGE_DIR"; then
    usage
    exit 2
fi

if test -z "$START"; then
    usage
    exit 2
fi

if test -z "$END"; then
    END=$START
fi

if test -z "$SKIP"; then
    SKIP='2'
fi

# Create the start-end address range (step 2)
ADDRS=$(printf '0x%x\n' $(seq "$START" 2 "$END"))

# For each address run the skip_instruction function on it
for ADDR in $ADDRS; do
    skip_instruction $ADDR $SKIP
done
