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

set -e

# Get the dir this is running in and the dir the script is in.
PWD=$(pwd)
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )

# PAD is the amount of extra instructions that should be tested on each side of
# the critical region
PAD=6

MCUBOOT_AXF=$1
SKIP_SIZES=$2
DAMAGE_TYPE=$3

# Take an image and make it unbootable. This is done by replacing one of the
# strings in the image with a different string. This causes the signature check
# to fail
function damage_image
{
    IMAGEDIR=$(dirname $MCUBOOT_AXF)
    local IMAGE_NAME=tfm_s_ns_signed.bin
    local BACKUP_IMAGE_NAME=tfm_s_ns_signed.bin.orig
    local IMAGE=$IMAGEDIR/$IMAGE_NAME
    mv $IMAGE $IMAGEDIR/$BACKUP_IMAGE_NAME

    if [ "$DAMAGE_TYPE" = "SIGNATURE" ]; then
        DAMAGE_PARAM="--signature"
    elif [ "$DAMAGE_TYPE" = "IMAGE_HASH" ]; then
        DAMAGE_PARAM="--image-hash"
    else
        echo "Failed to damage image $IMAGE with param $DAMAGE_TYPE" 1>&2
        exit -1
    fi

    python3 $DIR/damage_image.py -i $IMAGEDIR/$BACKUP_IMAGE_NAME -o $IMAGE $DAMAGE_PARAM 1>&2
}

function run_test
{
    local SKIP_SIZE=$1

    $DIR/fi_make_manifest.sh $MCUBOOT_AXF > $PWD/fih_manifest.csv

    # Load the CSV FI manifest file, and output in START, END lines. Effectively
    # join START and END lines together with a comma seperator.
    REGIONS=$(sed "N;s/\(0x[[:xdigit:]]*\).*START\n\(0x[[:xdigit:]]*\).*END.*/\1,\2/g;P;D" $PWD/fih_manifest.csv)
    # Ignore the first line, which includes the CSV header
    REGIONS=$(echo "$REGIONS" | tail -n+2)

    for REGION in $REGIONS; do
        #Split the START,END pairs into the two variables
        START=$(echo $REGION | cut -d"," -f 1)
        END=$(echo $REGION | cut -d"," -f 2)

        # Apply padding, converting back to hex
        START=$(printf "0x%X" $((START - PAD)))
        END=$(printf "0x%X" $((END + PAD)))

        # Invoke the fi tester script
        $DIR/fi_tester_gdb.sh $IMAGEDIR $START $END --skip $SKIP_SIZE
    done
}

damage_image $MCUBOOT_AXF
# Run the run_test function with each skip length between min and max in turn.

IFS=', ' read -r -a sizes <<< "$SKIP_SIZES"
for size in "${sizes[@]}"; do
    echo "Run tests with skip size $size" 1>&2
    run_test $size
done
