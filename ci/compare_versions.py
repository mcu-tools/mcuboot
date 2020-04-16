#!/usr/bin/env python3

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

from packaging.version import parse, LegacyVersion
import argparse
import sys

# exit with 0 if --new is equal to --old
# exit with 1 on errors
# exit with 2 if --new is newer than --old
# exit with 3 if --new is older than --old

parser = argparse.ArgumentParser()
parser.add_argument('--old', help='Version currently in use')
parser.add_argument('--new', help='New version to publish')

args = parser.parse_args()
if args.old is None or args.new is None:
    parser.print_help()
    exit(1)

old, new = parse(args.old), parse(args.new)

# only accept versions that were correctly parsed
for version in [old, new]:
    if isinstance(version, LegacyVersion):
        print("Invalid version parsed: {}".format(version))
        sys.exit(1)

if new == old:
    print("No version change")
    sys.exit(0)
elif new > old:
    print("Upgrade detected ({} > {})".format(new, old))
    sys.exit(2)

print("Downgrade detected ({} < {})".format(new, old))
sys.exit(3)
