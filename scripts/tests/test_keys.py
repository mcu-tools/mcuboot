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

import subprocess
from click.testing import CliRunner
from imgtool import main as imgtool_main
from imgtool.main import imgtool

# all supported key types for 'keygen'
KEY_TYPES = [*imgtool_main.keygens]
KEY_ENCODINGS = [*imgtool_main.valid_encodings]
PUB_HASH_ENCODINGS = [*imgtool_main.valid_hash_encodings]
PVT_KEY_FORMATS = [*imgtool_main.valid_formats]

OPENSSL_KEY_TYPES = {
    "rsa-2048": "Private-Key: (2048 bit, 2 primes)",
    "rsa-3072": "Private-Key: (3072 bit, 2 primes)",
    "ecdsa-p256": "Private-Key: (256 bit)",
    "ecdsa-p384": "Private-Key: (384 bit)",
    "ed25519": "ED25519 Private-Key:",
    "x25519": "X25519 Private-Key:",
}

GEN_KEY_EXT = ".key"
GEN_ANOTHER_KEY_EXT = ".another.key"
PUB_KEY_EXT = ".pub"
PUB_KEY_HASH_EXT = ".pubhash"


def tmp_name(tmp_path, key_type, suffix=""):
    return tmp_path / (key_type + suffix)


@pytest.fixture(scope="session")
def tmp_path_persistent(tmp_path_factory):
    return tmp_path_factory.mktemp("keys")


@pytest.mark.parametrize("key_type", KEY_TYPES)
def test_keygen(key_type, tmp_path_persistent):
    """Generate keys by imgtool"""

    runner = CliRunner()

    gen_key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)

    assert not gen_key.exists()
    result = runner.invoke(
        imgtool, ["keygen", "--key", str(gen_key), "--type", key_type]
    )
    assert result.exit_code == 0
    assert gen_key.exists()
    assert gen_key.stat().st_size > 0

    # another key
    gen_key2 = tmp_name(tmp_path_persistent, key_type, GEN_ANOTHER_KEY_EXT)

    assert str(gen_key2) != str(gen_key)

    assert not gen_key2.exists()
    result = runner.invoke(
        imgtool, ["keygen", "--key", str(gen_key2), "--type", key_type]
    )
    assert result.exit_code == 0
    assert gen_key2.exists()
    assert gen_key2.stat().st_size > 0

    # content must be different
    assert gen_key.read_bytes() != gen_key2.read_bytes()


@pytest.mark.parametrize("key_type", KEY_TYPES)
def test_keygen_type(key_type, tmp_path_persistent):
    """Check generated keys"""
    assert key_type in OPENSSL_KEY_TYPES

    gen_key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)

    result = subprocess.run(
        ["openssl", "pkey", "-in", str(gen_key), "-check", "-noout", "-text"],
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0
    assert "Key is valid" in result.stdout
    assert OPENSSL_KEY_TYPES[key_type] in result.stdout


@pytest.mark.parametrize("key_type", KEY_TYPES)
@pytest.mark.parametrize("format", PVT_KEY_FORMATS)
def test_getpriv(key_type, format, tmp_path_persistent):
    """Get private key"""
    runner = CliRunner()

    gen_key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)

    result = runner.invoke(
        imgtool,
        [
            "getpriv",
            "--key",
            str(gen_key),
            "--format",
            format,
        ],
    )
    assert result.exit_code == 0


@pytest.mark.parametrize("key_type", KEY_TYPES)
@pytest.mark.parametrize("encoding", KEY_ENCODINGS)
def test_getpub(key_type, encoding, tmp_path_persistent):
    """Get public key"""
    runner = CliRunner()

    gen_key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
    pub_key = tmp_name(tmp_path_persistent, key_type, PUB_KEY_EXT
                       + "." + encoding)

    assert not pub_key.exists()
    result = runner.invoke(
        imgtool,
        [
            "getpub",
            "--key",
            str(gen_key),
            "--output",
            str(pub_key),
            "--encoding",
            encoding,
        ],
    )
    assert result.exit_code == 0
    assert pub_key.exists()
    assert pub_key.stat().st_size > 0


@pytest.mark.parametrize("key_type", KEY_TYPES)
@pytest.mark.parametrize("encoding", PUB_HASH_ENCODINGS)
def test_getpubhash(key_type, encoding, tmp_path_persistent):
    """Get the hash of the public key"""
    runner = CliRunner()

    gen_key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
    pub_key_hash = tmp_name(
        tmp_path_persistent, key_type, PUB_KEY_HASH_EXT + "." + encoding
    )

    assert not pub_key_hash.exists()
    result = runner.invoke(
        imgtool,
        [
            "getpubhash",
            "--key",
            str(gen_key),
            "--output",
            str(pub_key_hash),
            "--encoding",
            encoding,
        ],
    )
    assert result.exit_code == 0
    assert pub_key_hash.exists()
    assert pub_key_hash.stat().st_size > 0


@pytest.mark.parametrize("key_type", KEY_TYPES)
def test_sign_verify(key_type, tmp_path_persistent):
    """Test basic sign and verify"""
    runner = CliRunner()

    gen_key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
    wrong_key = tmp_name(tmp_path_persistent, key_type, GEN_ANOTHER_KEY_EXT)
    image = tmp_name(tmp_path_persistent, "image", "bin")
    image_signed = tmp_name(tmp_path_persistent, "image", "signed")

    with image.open("wb") as f:
        f.write(b"\x00" * 1024)

    # not all required arguments are provided
    result = runner.invoke(
        imgtool,
        [
            "sign",
            "--key",
            str(gen_key),
            str(image),
            str(image_signed),
        ],
    )
    assert result.exit_code != 0
    assert not image_signed.exists()

    result = runner.invoke(
        imgtool,
        [
            "sign",
            "--key",
            str(gen_key),
            "--align",
            "16",
            "--version",
            "1.0.0",
            "--header-size",
            "0x400",
            "--slot-size",
            "0x10000",
            "--pad-header",
            str(image),
            str(image_signed),
        ],
    )
    assert result.exit_code == 0
    assert image_signed.exists()
    assert image_signed.stat().st_size > 0

    # original key can be used to verify a signed image
    result = runner.invoke(
        imgtool,
        [
            "verify",
            "--key",
            str(gen_key),
            str(image_signed),
        ],
    )
    assert result.exit_code == 0

    # 'another' key is not valid to verify a signed image
    result = runner.invoke(
        imgtool,
        [
            "verify",
            "--key",
            str(wrong_key),
            str(image_signed),
        ],
    )
    assert result.exit_code != 0
    image_signed.unlink()
