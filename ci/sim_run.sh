#!/bin/bash -x

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

GET_FEATURES="$(pwd)/ci/get_features.py"
CARGO_TOML="$(pwd)/sim/Cargo.toml"

pushd sim

all_features="$(${GET_FEATURES} ${CARGO_TOML})"
[ $? -ne 0 ] && exit 1

EXIT_CODE=0

if [[ ! -z $SINGLE_FEATURES ]]; then
  if [[ $SINGLE_FEATURES =~ "none" ]]; then
    echo "Running cargo with no features"
    time cargo test --no-run
    time cargo test
    rc=$? && [ $rc -ne 0 ] && EXIT_CODE=$rc
  fi

  for feature in $all_features; do
    if [[ $SINGLE_FEATURES =~ $feature ]]; then
      echo "Running cargo for feature=\"${feature}\""
      time cargo test --no-run --features $feature
      time cargo test --features $feature
      rc=$? && [ $rc -ne 0 ] && EXIT_CODE=$rc
    fi
  done
fi

if [[ ! -z $MULTI_FEATURES ]]; then
  IFS=','
  read -ra multi_features <<< "$MULTI_FEATURES"

  # psa crypto tests require single thread mode
  TEST_ARGS=''
  for features in "${multi_features[@]}"; do
    if [[ $features =~ "psa" ]]; then
        TEST_ARGS='--test-threads=1'
        break
    fi
  done

  for features in "${multi_features[@]}"; do
    echo "Running cargo for features=\"${features}\""
    time cargo test --no-run --features "$features" -- $TEST_ARGS
    time cargo test --features "$features" -- $TEST_ARGS
    rc=$? && [ $rc -ne 0 ] && EXIT_CODE=$rc
  done
fi

popd
exit $EXIT_CODE
