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

# get mcuboot root; assumes running script is stored under REPO_DIR/ci/
REPO_DIR=$(dirname $(dirname $(realpath $0)))
pushd $(mktemp -d)

# copy mcuboot so that it is part of the docker build context
cp -r $REPO_DIR .
cp -r $REPO_DIR/ci/fih_test_docker/execute_test.sh .
cp -r $REPO_DIR/ci/fih_test_docker/Dockerfile .
./mcuboot/ci/fih_test_docker/build.sh
popd