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

DOCKER_DIR=docker

IMAGE=fih-test:0.0.2

CACHED_IMAGE=$DOCKER_DIR/$IMAGE

[[ -f $CACHED_IMAGE ]] && (gzip -dc $CACHED_IMAGE | docker load)

if [[ $? -ne 0 ]]; then
    docker pull mcuboot/$IMAGE
    if [[ $GITHUB_ACTIONS != true ]]; then
        docker save mcuboot/$IMAGE | gzip > $CACHED_IMAGE
    fi
fi
