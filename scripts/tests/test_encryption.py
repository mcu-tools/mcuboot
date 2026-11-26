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

import os
import struct
from pathlib import Path

import pytest
from click.testing import CliRunner
from cryptography.hazmat.primitives import hashes, hmac, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from imgtool.main import imgtool

ROOT = Path(__file__).parents[2]
HEADER_SIZE = 0x200
SLOT_SIZE = 0x20000
TLV_INFO_MAGIC = 0x6907
TLV_PROT_INFO_MAGIC = 0x6908
IMAGE_F_ENCRYPTED_AES128 = 0x04
IMAGE_F_ENCRYPTED_AES256 = 0x08
TLV_ENC_EC384 = 0x35


def sign(tmp_path, args):
    infile = tmp_path / 'in.bin'
    outfile = tmp_path / 'out.bin'
    plain = os.urandom(4096)
    infile.write_bytes(plain)
    result = CliRunner().invoke(imgtool, [
        'sign', '--pad-header', '-H', hex(HEADER_SIZE), '-S', hex(SLOT_SIZE),
        '-v', '1.0.0', '--align', '4', *args, str(infile), str(outfile)])
    return result, plain, outfile


def parse_image(data):
    _, _, hdr_size, prot_size, img_size, flags = struct.unpack_from('<IIHHII', data)
    off = hdr_size + img_size
    tlvs = {}
    for magic in (TLV_PROT_INFO_MAGIC, TLV_INFO_MAGIC):
        if off + 4 > len(data):
            break
        info_magic, total = struct.unpack_from('<HH', data, off)
        if info_magic != magic:
            continue
        end = off + total
        off += 4
        while off < end:
            kind, length = struct.unpack_from('<HH', data, off)
            tlvs[kind] = data[off + 4:off + 4 + length]
            off += 4 + length
    return flags, data[hdr_size:hdr_size + img_size], tlvs


def unwrap_ecies_p384(enctlv, keylen):
    with open(ROOT / 'enc-ec384-priv.pem', 'rb') as f:
        priv = serialization.load_pem_private_key(f.read(), password=None)
    peer = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP384R1(), enctlv[:97])
    shared = priv.exchange(ec.ECDH(), peer)
    derived = HKDF(algorithm=hashes.SHA384(), length=keylen + 48, salt=None,
                   info=b'MCUBoot_ECIES_v1').derive(shared)
    tag = enctlv[97:97 + 48]
    cipherkey = enctlv[97 + 48:]
    mac = hmac.HMAC(derived[keylen:], hashes.SHA384())
    mac.update(cipherkey)
    mac.verify(tag)
    dec = Cipher(algorithms.AES(derived[:keylen]), modes.CTR(bytes(16))).decryptor()
    return dec.update(cipherkey) + dec.finalize()


@pytest.mark.parametrize('keylen, flag', [(128, IMAGE_F_ENCRYPTED_AES128),
                                          (256, IMAGE_F_ENCRYPTED_AES256)])
def test_ecies_p384_round_trip(tmp_path, keylen, flag):
    result, plain, outfile = sign(tmp_path, [
        '-k', str(ROOT / 'root-ec-p384.pem'),
        '-E', str(ROOT / 'enc-ec384-pub.pem'),
        '--encrypt-keylen', str(keylen)])
    assert result.exit_code == 0, result.output

    flags, body, tlvs = parse_image(outfile.read_bytes())
    assert flags & flag
    enctlv = tlvs[TLV_ENC_EC384]
    assert len(enctlv) == 97 + 48 + keylen // 8

    aes_key = unwrap_ecies_p384(enctlv, keylen // 8)
    dec = Cipher(algorithms.AES(aes_key), modes.CTR(bytes(16))).decryptor()
    assert dec.update(body) + dec.finalize() == plain


@pytest.mark.parametrize('hmac_sha', ['256', '512'])
def test_ecies_p384_rejects_other_hmac(tmp_path, hmac_sha):
    result, _, _ = sign(tmp_path, [
        '-k', str(ROOT / 'root-ec-p384.pem'),
        '-E', str(ROOT / 'enc-ec384-pub.pem'),
        '--hmac-sha', hmac_sha])
    assert result.exit_code != 0
    assert 'HMAC-SHA384' in result.output


def test_hmac_sha384_needs_ecies_p384(tmp_path):
    result, _, _ = sign(tmp_path, [
        '-k', str(ROOT / 'root-ec-p256.pem'),
        '-E', str(ROOT / 'enc-ec256-pub.pem'),
        '--hmac-sha', '384'])
    assert result.exit_code != 0
    assert 'HMAC-SHA384' in result.output
