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

import collections
import yaml

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


def parse_yaml_file(filepath):
    with open(filepath) as f:
        results = yaml.safe_load(f)

    if not results:
        raise ValueError("Failed to parse output yaml file.")

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

    return test_stats, failed_boot_last_lines, exec_fail_reasons
