# Copyright 2017 Linaro Limited
# Copyright 2024 Arm Limited
#
# SPDX-License-Identifier: Apache-2.0
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

"""
Semi Semantic Versioning

Implements a subset of semantic versioning that is supportable by the image
header.
"""
import re
import sys
from collections import namedtuple

SemiSemVersion = namedtuple('SemiSemVersion', ['major', 'minor', 'revision',
                                               'build'])

version_re = re.compile(
    r"""^([1-9]\d*|0)(\.([1-9]\d*|0)(\.([1-9]\d*|0)(\+([1-9]\d*|0))?)?)?$""")


def decode_version(text):
    """Decode the version string, which should be of the form maj.min.rev+build
    """
    m = version_re.match(text)
    if m:
        result = SemiSemVersion(
                int(m.group(1)) if m.group(1) else 0,
                int(m.group(3)) if m.group(3) else 0,
                int(m.group(5)) if m.group(5) else 0,
                int(m.group(7)) if m.group(7) else 0)
        return result
    else:
        msg = "Invalid version number, should be maj.min.rev+build with later "
        msg += "parts optional"
        raise ValueError(msg)


if __name__ == '__main__':
    if len(sys.argv) > 1:
        print(decode_version(sys.argv[1]))
    else:
        print("Requires an argument, e.g. '1.0.0'")
