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

import pytest

from click.testing import CliRunner
from imgtool.main import imgtool
from imgtool import imgtool_version

# all available imgtool commands
COMMANDS = [
    "create",
    "dumpinfo",
    "getpriv",
    "getpub",
    "getpubhash",
    "keygen",
    "sign",
    "verify",
    "version",
]


def test_new_command():
    """Check that no new commands had been added,
    so that tests would be updated in such case"""
    for cmd in imgtool.commands:
        assert cmd in COMMANDS


def test_help():
    """Simple test for the imgtool's help option,
    mostly just to see that it can be started"""
    runner = CliRunner()

    result_short = runner.invoke(imgtool, ["-h"])
    assert result_short.exit_code == 0

    result_long = runner.invoke(imgtool, ["--help"])
    assert result_long.exit_code == 0
    assert result_short.output == result_long.output

    # by default help should be also produced
    result_empty = runner.invoke(imgtool)
    assert result_empty.exit_code == 0
    assert result_empty.output == result_short.output


def test_version():
    """Check that some version info is produced"""
    runner = CliRunner()

    result = runner.invoke(imgtool, ["version"])
    assert result.exit_code == 0
    assert result.output == imgtool_version + "\n"

    result_help = runner.invoke(imgtool, ["version", "-h"])
    assert result_help.exit_code == 0
    assert result_help.output != result.output


def test_unknown():
    """Check that unknown command will be handled"""
    runner = CliRunner()

    result = runner.invoke(imgtool, ["unknown"])
    assert result.exit_code != 0


@pytest.mark.parametrize("command", COMMANDS)
def test_cmd_help(command):
    """Check that all commands have some help"""
    runner = CliRunner()

    result_short = runner.invoke(imgtool, [command, "-h"])
    assert result_short.exit_code == 0

    result_long = runner.invoke(imgtool, [command, "--help"])
    assert result_long.exit_code == 0

    assert result_short.output == result_long.output


@pytest.mark.parametrize("command1", COMMANDS)
@pytest.mark.parametrize("command2", COMMANDS)
def test_cmd_dif_help(command1, command2):
    """Check that all commands have some different help"""
    runner = CliRunner()

    result_general = runner.invoke(imgtool, "--help")
    assert result_general.exit_code == 0

    result_cmd1 = runner.invoke(imgtool, [command1, "--help"])
    assert result_cmd1.exit_code == 0
    assert result_cmd1.output != result_general.output

    if command1 != command2:
        result_cmd2 = runner.invoke(imgtool, [command2, "--help"])
        assert result_cmd2.exit_code == 0

        assert result_cmd1.output != result_cmd2.output
