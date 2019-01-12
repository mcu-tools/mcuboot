#!/bin/bash

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

pushd sim

EXIT_CODE=0

if [[ ! -z $SINGLE_FEATURES ]]; then
  all_features="sig-rsa sig-ecdsa overwrite-only validate-slot0 enc-rsa enc-kw boostrap"

  if [[ $SINGLE_FEATURES =~ "none" ]]; then
    echo "Running cargo with no features"
    cargo test
    rc=$? && [ $rc -ne 0 ] && EXIT_CODE=$rc
  fi

  for feature in $all_features; do
    if [[ $SINGLE_FEATURES =~ $feature ]]; then
      echo "Running cargo for feature=\"${feature}\""
      cargo test --features $feature
      rc=$? && [ $rc -ne 0 ] && EXIT_CODE=$rc
    fi
  done
fi

if [[ ! -z $MULTI_FEATURES ]]; then
  IFS=','
  read -ra multi_features <<< "$MULTI_FEATURES"
  for features in "${multi_features[@]}"; do
    echo "Running cargo for features=\"${features}\""
    cargo test --features "$features"
    rc=$? && [ $rc -ne 0 ] && EXIT_CODE=$rc
  done
fi

popd
exit $EXIT_CODE
