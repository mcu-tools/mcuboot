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

import base64
import struct
from pathlib import Path

import pytest
from click.testing import CliRunner
from imgtool import image
from imgtool.main import imgtool

VERSION = '1.2.3'
HEADER_SIZE = 32
SLOT_SIZE = 4096


@pytest.fixture
def key_file():
    return Path(__file__).parents[2] / 'root-ec-p256.pem'


def parse_tlvs(data, endian="<"):
    header = data[:image.IMAGE_HEADER_SIZE]
    _, _, hdr_sz, _, img_sz, flags = struct.unpack(endian + "IIHHII", header[:20])
    tlv_off = hdr_sz + img_sz

    magic, tlv_tot = struct.unpack(endian + "HH", data[tlv_off:tlv_off + image.TLV_INFO_SIZE])
    if magic == image.TLV_PROT_INFO_MAGIC:
        tlv_off += tlv_tot
        magic, tlv_tot = struct.unpack(endian + "HH", data[tlv_off:tlv_off + image.TLV_INFO_SIZE])

    assert magic == image.TLV_INFO_MAGIC

    tlv_end = tlv_off + tlv_tot
    tlv_off += image.TLV_INFO_SIZE
    tlvs = []
    while tlv_off < tlv_end:
        tlv_type, _, tlv_len = struct.unpack(endian + "BBH", data[tlv_off:tlv_off + image.TLV_SIZE])
        tlv_off += image.TLV_SIZE
        payload = data[tlv_off:tlv_off + tlv_len]
        tlvs.append((tlv_type, payload))
        tlv_off += tlv_len

    return flags, tlvs


@pytest.mark.parametrize(
    "keylen, kek_bytes, expected_len, enc_flag",
    [
        (128, bytes(range(16)), 24, image.IMAGE_F["ENCRYPTED_AES128"]),
        (256, bytes(range(32)), 40, image.IMAGE_F["ENCRYPTED_AES256"]),
    ],
)
def test_encrypt_aes_kw(keylen, kek_bytes, expected_len, enc_flag, tmpdir):
    runner = CliRunner()

    infile = tmpdir / "in.bin"
    outfile = tmpdir / "out.bin"
    kekfile = tmpdir / "kek.b64"

    with infile.open("wb") as f:
        f.write(bytes([0x11]) * 256)
    with kekfile.open("wb") as f:
        f.write(base64.b64encode(kek_bytes))

    result = runner.invoke(
        imgtool,
        [
            "sign",
            f"--header-size={HEADER_SIZE}",
            f"--slot-size={SLOT_SIZE}",
            f"--version={VERSION}",
            "--pad-header",
            f"--encrypt={kekfile}",
            f"--encrypt-keylen={keylen}",
            str(infile),
            str(outfile),
        ],
    )
    assert result.exit_code == 0
    assert outfile.exists()

    with outfile.open("rb") as f:
        data = f.read()
    flags, tlvs = parse_tlvs(data)
    assert flags & enc_flag

    enckw_tlvs = [payload for tlv_type, payload in tlvs if tlv_type == image.TLV_VALUES["ENCKW"]]
    assert len(enckw_tlvs) == 1
    assert len(enckw_tlvs[0]) == expected_len


@pytest.mark.parametrize(
    "keylen, kek_bytes",
    [
        (128, bytes(range(16))),
        (256, bytes(range(32))),
    ],
)
def test_encrypt_aes_kw_with_signing_key(keylen, kek_bytes, tmpdir, key_file):
    runner = CliRunner()

    infile = tmpdir / "in.bin"
    outfile = tmpdir / "out.bin"
    kekfile = tmpdir / "kek.b64"

    with infile.open("wb") as f:
        f.write(bytes([0x22]) * 256)
    with kekfile.open("wb") as f:
        f.write(base64.b64encode(kek_bytes))

    result = runner.invoke(
        imgtool,
        [
            "sign",
            f"--header-size={HEADER_SIZE}",
            f"--slot-size={SLOT_SIZE}",
            f"--version={VERSION}",
            "--pad-header",
            f"--encrypt={kekfile}",
            f"--encrypt-keylen={keylen}",
            f"--key={key_file}",
            str(infile),
            str(outfile),
        ],
    )
    assert result.exit_code == 0
    assert outfile.exists()
