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

MAIN_BRANCH=master

# ignores last commit because travis/gh creates a merge commit
commits=$(git log --format=%h ${MAIN_BRANCH}..HEAD | tail -n +2)

for sha in $commits; do
  actual=$(git show -s --format=%B ${sha} | sed '/^$/d' | tail -n 1)
  expected="Signed-off-by: $(git show -s --format="%an <%ae>" ${sha})"

  if [ "${actual}" != "${expected}" ]; then
    echo -e "${sha} is missing or using an invalid \"Signed-off-by:\" line"
    exit 1
  fi
done
