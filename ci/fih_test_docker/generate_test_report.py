# Copyright (c) 2020-2023 Arm Limited
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


def print_results(results):

    test_stats, failed_boot_last_lines, exec_fail_reasons = results

    print("{:s}: {:d}.".format(CATEGORIES['TOTAL'], test_stats[CATEGORIES['TOTAL']]))
    print("{:s} ({:d}):".format(CATEGORIES['SUCCESS'], test_stats[CATEGORIES['SUCCESS']]))
    print("    {:s}: ({:d}):".format(CATEGORIES['ADDRES_NOEXEC'], test_stats[CATEGORIES['ADDRES_NOEXEC']]))
    test_with_skip = test_stats[CATEGORIES['SUCCESS']] - test_stats[CATEGORIES['ADDRES_NOEXEC']]
    print("    {:s}: ({:d}):".format(CATEGORIES['SKIPPED'], test_with_skip))
    print("    {:s} ({:d}):".format(CATEGORIES['NO_BOOT'], test_with_skip - test_stats[CATEGORIES['BOOT']]))
    for last_line in failed_boot_last_lines.keys():
        print("        last line: {:s} ({:d})".format(last_line, failed_boot_last_lines[last_line]))
    print("    {:s} ({:d})".format(CATEGORIES['BOOT'], test_stats[CATEGORIES['BOOT']]))
    print("{:s} ({:d}):".format(CATEGORIES['FAILED'], test_stats[CATEGORIES['TOTAL']] - test_stats[CATEGORIES['SUCCESS']]))
    for reason in exec_fail_reasons.keys():
        print("    {:s} ({:d})".format(reason, exec_fail_reasons[reason]))


def main():
    parser = argparse.ArgumentParser(description='''Process a FIH test output yaml file, and output a human readable report''')
    parser.add_argument('filename', help='yaml file to process')

    args = parser.parse_args()
    results = parse_yaml_file(args.filename)
    print_results(results)


if __name__ == "__main__":
    main()
