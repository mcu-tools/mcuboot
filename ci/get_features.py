#!/usr/bin/env python3

# Copyright 2020 JUUL Labs
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

import argparse
import os.path
import sys

try:
    import tomllib
except ModuleNotFoundError:
    import tomli as tomllib


def main() -> int:
    parser = argparse.ArgumentParser(description='Print features from a Cargo.toml.')
    parser.add_argument('infile', help='Input file to parse')

    args = parser.parse_args()
    if not os.path.isfile(args.infile):
        print("File not found")
        return 1

    try:
        with open(args.infile) as file:
            cargo_toml = file.read()
    except Exception:
        print(f"Error reading \"{args.infile}\"")
        return 1

    config = tomllib.loads(cargo_toml)
    if 'features' not in config:
        print("Missing \"[features]\" section")
        return 1

    print(" ".join([k for k in config['features'] if k != 'default']))
    return 0


if __name__ == "__main__":
    sys.exit(main())
