# Copyright (c) 2023 Arm Limited
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
from utils import CATEGORIES, parse_yaml_file


def validate_output(test_stats, skip_size, fih_level):
    if (test_stats[CATEGORIES['BOOT']] > 0
       and skip_size == "2,4,6" and fih_level == "MEDIUM"):
        raise ValueError("The number of sucessful boots was more than zero")


def main():
    parser = argparse.ArgumentParser(description='''Process a FIH test output yaml file,
     and validate no sucessfull boots have happened''')
    parser.add_argument('filename', help='yaml file to process')
    parser.add_argument('skip_size', help='instruction skip size')
    parser.add_argument('fih_level', nargs="?",
                        help='fault injection hardening level')

    args = parser.parse_args()
    test_stats = parse_yaml_file(args.filename)[0]
    validate_output(test_stats, args.skip_size, args.fih_level)


if __name__ == "__main__":
    main()
