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
from pathlib import Path

import pytest
from click.testing import CliRunner
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.keywrap import aes_key_unwrap
from imgtool import image, keys
from imgtool.dumpinfo import _read_imginfo
from imgtool.main import imgtool

VERSION = '1.2.3'
HEADER_SIZE = 32
SLOT_SIZE = 4096
PAYLOAD = bytes(range(256)) + b'\x55'


def sign_image(tmp_path, kek_bytes, *options):
    infile = tmp_path / "in.bin"
    outfile = tmp_path / "out.bin"
    kekfile = tmp_path / "kek.b64"
    infile.write_bytes(PAYLOAD)
    kekfile.write_bytes(base64.b64encode(kek_bytes))

    result = CliRunner().invoke(
        imgtool,
        [
            "sign",
            f"--header-size={HEADER_SIZE}",
            f"--slot-size={SLOT_SIZE}",
            f"--version={VERSION}",
            "--pad-header",
            f"--encrypt={kekfile}",
            *options,
            str(infile),
            str(outfile),
        ],
    )
    return result, outfile


@pytest.mark.parametrize(
    "signing_key", [None, "rsa-2048", "rsa-3072", "ec-p256", "ec-p384", "ed25519"],
)
@pytest.mark.parametrize("keylen", [128, 256])
def test_encrypt_aes_kw_round_trip(keylen, signing_key, tmp_path):
    kek_bytes = bytes(range(keylen // 8))
    keyfile = Path(__file__).parents[2] / f"root-{signing_key}.pem" if signing_key else None
    result, outfile = sign_image(
        tmp_path, kek_bytes, f"--encrypt-keylen={keylen}",
        *([f"--key={keyfile}"] if keyfile else []),
    )
    assert result.exit_code == 0, result.output

    data = outfile.read_bytes()
    info = _read_imginfo(outfile)
    header = info["header"]
    tlv_area = info["tlv_area"]
    assert tlv_area["tlv_hdr"]["magic"] == image.TLV_INFO_MAGIC
    enc_flags = image.IMAGE_F["ENCRYPTED_AES128"] | image.IMAGE_F["ENCRYPTED_AES256"]
    assert header["flags"] & enc_flags == image.IMAGE_F[f"ENCRYPTED_AES{keylen}"]

    enckw_tlvs = [tlv["data"] for tlv in tlv_area["tlvs"]
                  if tlv["type"] == image.TLV_VALUES["ENCKW"]]
    assert len(enckw_tlvs) == 1
    assert len(enckw_tlvs[0]) == keylen // 8 + 8
    plainkey = aes_key_unwrap(kek_bytes, enckw_tlvs[0])
    assert len(plainkey) == keylen // 8

    header_size = header["hdr_size"]
    payload_end = header_size + header["img_size"]
    decryptor = Cipher(algorithms.AES(plainkey), modes.CTR(bytes(16))).decryptor()
    plaintext = decryptor.update(data[header_size:payload_end]) + decryptor.finalize()
    assert plaintext == PAYLOAD + bytes(-len(PAYLOAD) % 16)

    decrypted = tmp_path / "decrypted.bin"
    decrypted.write_bytes(data[:header_size] + plaintext + data[payload_end:])
    key = keys.load(keyfile) if keyfile else None
    assert image.Image.verify(decrypted, key)[0] == image.VerifyResult.OK


@pytest.mark.parametrize("kek_length", [0, 15, 17, 24, 31, 33])
def test_encrypt_aes_kw_invalid_key_length(kek_length, tmp_path):
    result, outfile = sign_image(tmp_path, bytes(range(kek_length)))

    assert result.exit_code == 2
    assert isinstance(result.exception, SystemExit)
    assert f"Invalid AES key length: {kek_length} bytes" in result.output
    assert "Expected 16 or 32 bytes after base64 decode." in result.output
    assert not outfile.exists()


@pytest.mark.parametrize("keylen, kek_length", [(128, 32), (256, 16)])
def test_encrypt_aes_kw_mismatched_key_length(keylen, kek_length, tmp_path):
    result, outfile = sign_image(
        tmp_path, bytes(range(kek_length)), f"--encrypt-keylen={keylen}",
    )
    assert result.exit_code == 2
    assert "AES-KW KEK size must match --encrypt-keylen" in result.output
    assert not outfile.exists()


@pytest.mark.parametrize("kek_length", [16, 32])
def test_load_aes_kw_requires_opt_in(kek_length, tmp_path):
    kek_bytes = bytes(range(kek_length))
    kekfile = tmp_path / "kek.b64"
    kekfile.write_bytes(base64.b64encode(kek_bytes))
    with pytest.raises(ValueError):
        keys.load(kekfile)
    key = keys.load(kekfile, allow_aes=True)
    assert isinstance(key, keys.AESKWKey)
    assert key.kek == kek_bytes
