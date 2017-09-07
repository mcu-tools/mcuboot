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

has_commits=false
for sha in $commits; do
  expected="Signed-off-by: $(git show -s --format="%an <%ae>" ${sha})"
  lines="$(git show -s --format=%B ${sha})"
  found_sob=false
  IFS=$'\n'
  for line in ${lines}; do
    stripped=$(echo $line | sed -e 's/^\s*//' | sed -e 's/\s*$//')
    if [[ $stripped == ${expected} ]]; then
      found_sob=true
      break
    fi
  done

  if [[ ${found_sob} = false ]]; then
    echo -e "No \"${expected}\" found in commit ${sha}"
    exit 1
  fi

  has_commits=true
done

if [[ ${has_commits} = false ]]; then
  echo "No commits found in this PR!"
  exit 1
fi
