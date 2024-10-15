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

from pathlib import Path

import pytest
from click.testing import CliRunner

from imgtool.image import Image
from imgtool.main import (
    comp_default_lp,
    comp_default_dictsize,
    comp_default_lc,
    create_lzma2_header,
    comp_default_pb,
    imgtool,
)

VERSION = '2.0.0'
HEADER_SIZE = 0x200
SLOT_SIZE = 0x7a000


@pytest.fixture
def key_file() -> Path:
    return Path(__file__).parents[2] / 'root-ec-p256.pem'


def check_if_compressed(out_file: Path) -> bool:
    # Verify output file. There should be better solution to check
    # if the output file is correctly compressed with lzma2.
    # For now we check only if the output image contains
    # some specific for lzma2 values.
    img = Image(version=VERSION, header_size=HEADER_SIZE, slot_size=SLOT_SIZE, pad_header=True)
    img.load(out_file)
    compression_header = create_lzma2_header(
        dictsize=comp_default_dictsize, pb=comp_default_pb,
        lc=comp_default_lc, lp=comp_default_lp
    )
    return compression_header in img.payload


@pytest.mark.parametrize(
    'compression, compressed',
    [
        ('lzma2', True),
        ('disabled', False)
    ]
)
def test_lzma2_compression(tmpdir: Path, key_file: Path, compression: str, compressed: bool):
    """
    Test if lzma2 compression works by running ``imgtool sign``
    command and checking expected output.
    """
    in_file = tmpdir / 'zephyr.bin'
    with in_file.open("wb") as f:
        f.write(b"hello world\x00\x00\x00\x00\x00" * 64)
    out_file: Path = tmpdir / 'zephyr_signed.bin'

    runner = CliRunner()
    result = runner.invoke(
        imgtool,
        [
            'sign',
            str(in_file),
            str(out_file),
            f'--header-size={HEADER_SIZE}',
            f'--slot-size={SLOT_SIZE}',
            f'--version={VERSION}',
            '--pad-header',
            f'--compression={compression}',
            f'--key={key_file}'
        ],
    )
    assert result.exit_code == 0
    assert out_file.exists()
    assert check_if_compressed(out_file) is compressed
