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

parents=(`git log -n 1 --format=%p HEAD`)

if [[ "${#parents[@]}" -eq 1 ]]; then
  # CI doesn't use a merge commit
  commits=$(git show -s --format=%h ${parents})
elif [[ "${#parents[@]}" -eq 2 ]]; then
  # CI uses a merge commit, eg GH / Travis
  from="${parents[0]}"
  into="${parents[1]}"
  commits=$(git show -s --format=%h ${from}..${into})
else
  echo "Unexpected behavior, cannot verify more than 2 parent commits!"
  exit 1
fi

has_commits=false
for sha in $commits; do
  author="Signed-off-by: $(git show -s --format="%an <%ae>" ${sha})"
  committer="Signed-off-by: $(git show -s --format="%cn <%ce>" ${sha})"

  lines="$(git show -s --format=%B ${sha})"

  found_author=false
  # Don't enforce committer email on forks; this primarily avoids issues
  # running workflows on the zephyr fork, because rebases done in the GH UX
  # use the primary email of the committer, which might not match the one
  # used in git CLI.
  if [[ $GITHUB_REPOSITORY == mcu-tools/* ]]; then
    found_committer=false
  else
    found_committer=true
  fi

  IFS=$'\n'
  for line in ${lines}; do
    stripped=$(echo $line | sed -e 's/^\s*//' | sed -e 's/\s*$//')
    if [[ "${stripped}" == "${author}" ]]; then
      found_author=true
    fi
    if [[ "${stripped}" == "${committer}" ]]; then
      found_committer=true
    fi

    [[ ${found_author} == true && ${found_committer} == true ]] && break
  done

  if [[ ${found_author} == false ]]; then
    echo -e "Missing \"${author}\" in commit ${sha}"
  fi
  if [[ ${found_committer} == false ]]; then
    echo -e "Missing \"${committer}\" in commit ${sha}"
  fi
  if [[ ${found_author} == false || ${found_committer} == false ]]; then
    exit 1
  fi

  has_commits=true
done

if [[ ${has_commits} = false ]]; then
  echo "No commits found in this PR!"
  exit 1
fi
