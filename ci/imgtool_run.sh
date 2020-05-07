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

if [[ $TRAVIS_PULL_REQUEST != "false" || $TRAVIS_BRANCH != "master" ]]; then
    echo "Either a PR or not \"master\" branch, exiting"
    exit 0
fi

IMGTOOL_VER_PREFIX="\+imgtool_version = "
IMGTOOL_VER_FILE="imgtool/__init__.py"
DIST_DIR="dist"

if [[ -z "$TWINE_TOKEN" ]]; then
    echo "\$TWINE_TOKEN must be set in travis settings"
    exit 0
fi

cd scripts/

last_release=$(pip show imgtool | grep "Version: " | cut -d" " -f2)
repo_version=$(grep "imgtool_version = " imgtool/__init__.py | sed 's/^.* = "\(.*\)"/\1/g')

python3 ../ci/compare_versions.py --old $last_release --new $repo_version
rc=$?

if [[ $rc -eq 0 ]]; then
    echo "Imgtool version not changed; will not publish"
    exit 0
elif [[ $rc -eq 1 ]]; then
    echo "Error parsing versions"
    exit 1
elif [[ $rc -eq 3 ]]; then
    echo "Imgtool downgrade detected!"
    exit 1
fi

rm -rf $DIST_DIR
python setup.py sdist bdist_wheel

twine upload --username __token__ --password "${TWINE_TOKEN}" "${DIST_DIR}/*"
