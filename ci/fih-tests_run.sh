#!/bin/bash -x

# Copyright (c) 2020-2024 Arm Limited
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

source $(dirname "$0")/fih-tests_version.sh

pushd .. &&\
   git clone https://git.trustedfirmware.org/TF-M/trusted-firmware-m.git &&\
   pushd trusted-firmware-m &&\
   git checkout eb8ff0db7d657b77abcd0262d5bf7f38eb1e1cdc &&\
   source lib/ext/tf-m-tests/version.txt &&\
   popd &&\
   git clone https://git.trustedfirmware.org/TF-M/tf-m-tests.git &&\
   pushd tf-m-tests &&\
   git checkout $version &&\
   popd

if [[ $GITHUB_ACTIONS == true ]]; then
    if [[ -z $FIH_ENV ]]; then
        echo "Workflow has found no \$FIH_ENV"
        exit 1
    fi

    args=($FIH_ENV)
    len=${#args[@]}
    if [[ $len < 3 ]]; then
        echo "Invalid number of \$FIH_ENV args"
        exit 1
    fi

    BUILD_TYPE=${args[0]}
    SKIP_SIZE=${args[1]}
    DAMAGE_TYPE=${args[2]}

    if [[ $len > 3 ]]; then
        FIH_LEVEL=${args[3]}
    fi
fi

if test -z "$FIH_LEVEL"; then
    docker run --rm -v $(pwd):/root/work/tfm:rw,z mcuboot/fih-test:$FIH_IMAGE_VERSION /bin/sh -c '/root/work/tfm/mcuboot/ci/fih_test_docker/execute_test.sh $0 $1 $2' $SKIP_SIZE $BUILD_TYPE $DAMAGE_TYPE
else
    docker run --rm -v $(pwd):/root/work/tfm:rw,z mcuboot/fih-test:$FIH_IMAGE_VERSION /bin/sh -c '/root/work/tfm/mcuboot/ci/fih_test_docker/execute_test.sh $0 $1 $2 $3' $SKIP_SIZE $BUILD_TYPE $DAMAGE_TYPE $FIH_LEVEL
fi
