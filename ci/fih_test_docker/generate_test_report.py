# Copyright (c) 2020 Arm Limited
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
import yaml
import collections

CATEGORIES = {
        'TOTAL': 'Total tests run',
        'SUCCESS': 'Tests executed successfully',
        'FAILED': 'Tests failed to execute successfully',
        # the execution never reached the address
        'ADDRES_NOEXEC': 'Address was not executed',
        # The address was successfully skipped by the debugger
        'SKIPPED': 'Address was skipped',
        'NO_BOOT': 'System not booted (desired behaviour)',
        'BOOT': 'System booted (undesired behaviour)'
}


def print_results(results):
    test_stats = collections.Counter()
    failed_boot_last_lines = collections.Counter()
    exec_fail_reasons = collections.Counter()

    for test in results:
        test = test["skip_test"]

        test_stats.update([CATEGORIES['TOTAL']])

        if test["test_exec_ok"]:
            test_stats.update([CATEGORIES['SUCCESS']])

            if "skipped" in test.keys() and not test["skipped"]:
                # The debugger didn't stop at this address
                test_stats.update([CATEGORIES['ADDRES_NOEXEC']])
                continue

            if test["boot"]:
                test_stats.update([CATEGORIES['BOOT']])
                continue

            failed_boot_last_lines.update([test["last_line"]])
        else:
            exec_fail_reasons.update([test["test_exec_fail_reason"]])

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

    with open(args.filename) as output_yaml_file:
        results = yaml.safe_load(output_yaml_file)

        if results:
            print_results(results)
        else:
            print("Failed to parse output yaml file.")


if __name__ == "__main__":
    main()
