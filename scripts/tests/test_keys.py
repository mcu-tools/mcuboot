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

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

import pytest
from click.testing import CliRunner
from imgtool import keys
from imgtool import main as imgtool_main
from imgtool.main import imgtool

if sys.version_info >= (3, 11):
    from typing import assert_type
else:
    from typing_extensions import assert_type

if TYPE_CHECKING:
    from pathlib import Path

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
PUB_ONLY_PEM_EXT = ".pub_only.pem"


def tmp_name(tmp_path: Path, key_type: str, suffix: str = "") -> Path:
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


def _ensure_pub_only_pem(
    runner: CliRunner, gen_key: Path, pub_only_pem: Path
) -> None:
    if pub_only_pem.exists():
        return
    assert gen_key.exists(), (
        f"Expected generated key to exist before extracting public key: "
        f"{gen_key}"
    )
    result = runner.invoke(
        imgtool,
        (
            "getpub",
            "--key",
            str(gen_key),
            "--output",
            str(pub_only_pem),
            "--encoding",
            "pem",
        ),
    )
    assert result.exit_code == 0
    assert pub_only_pem.read_bytes().startswith(b"-----BEGIN PUBLIC KEY-----")


@pytest.mark.parametrize("key_type", KEY_TYPES)
@pytest.mark.parametrize("encoding", KEY_ENCODINGS)
def test_getpub_from_pub_only_pem(
    key_type: str, encoding: str, tmp_path_persistent: Path
) -> None:
    """Get public key when input is itself a public-only PEM"""
    runner = CliRunner()

    gen_key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
    pub_only_pem = tmp_name(tmp_path_persistent, key_type, PUB_ONLY_PEM_EXT)
    _ensure_pub_only_pem(runner, gen_key, pub_only_pem)

    from_priv = tmp_name(tmp_path_persistent, key_type, ".from_priv_pub." + encoding)
    from_pub = tmp_name(tmp_path_persistent, key_type, ".from_pub_pub." + encoding)

    for src, out in ((gen_key, from_priv), (pub_only_pem, from_pub)):
        result = runner.invoke(
            imgtool,
            (
                "getpub",
                "--key",
                str(src),
                "--output",
                str(out),
                "--encoding",
                encoding,
            ),
        )
        assert result.exit_code == 0
        assert out.exists()
        assert out.stat().st_size > 0

    assert from_priv.read_bytes() == from_pub.read_bytes()


@pytest.mark.parametrize("key_type", KEY_TYPES)
@pytest.mark.parametrize("encoding", PUB_HASH_ENCODINGS)
def test_getpubhash_from_pub_only_pem(
    key_type: str, encoding: str, tmp_path_persistent: Path
) -> None:
    """Get public-key hash when input is itself a public-only PEM"""
    runner = CliRunner()

    gen_key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
    pub_only_pem = tmp_name(tmp_path_persistent, key_type, PUB_ONLY_PEM_EXT)
    _ensure_pub_only_pem(runner, gen_key, pub_only_pem)

    from_priv = tmp_name(tmp_path_persistent, key_type, ".from_priv_hash." + encoding)
    from_pub = tmp_name(tmp_path_persistent, key_type, ".from_pub_hash." + encoding)

    for src, out in ((gen_key, from_priv), (pub_only_pem, from_pub)):
        result = runner.invoke(
            imgtool,
            (
                "getpubhash",
                "--key",
                str(src),
                "--output",
                str(out),
                "--encoding",
                encoding,
            ),
        )
        assert result.exit_code == 0
        assert out.exists()
        assert out.stat().st_size > 0

    assert from_priv.read_bytes() == from_pub.read_bytes()


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


@pytest.mark.parametrize("key_type", KEY_TYPES)
def test_sign_with_pub_only_pem_fails_cleanly(
    key_type: str, tmp_path_persistent: Path
) -> None:
    """Signing with a pub-only PEM raises click.UsageError, not AttributeError"""
    runner = CliRunner()

    gen_key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
    pub_only_pem = tmp_name(tmp_path_persistent, key_type, PUB_ONLY_PEM_EXT)
    _ensure_pub_only_pem(runner, gen_key, pub_only_pem)

    image = tmp_name(tmp_path_persistent, key_type, ".pubonly.bin")
    image_signed = tmp_name(tmp_path_persistent, key_type, ".pubonly.signed")
    with image.open("wb") as f:
        f.write(b"\x00" * 1024)

    result = runner.invoke(
        imgtool,
        (
            "sign",
            "--key",
            str(pub_only_pem),
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
        ),
    )
    assert result.exit_code == 2
    assert isinstance(result.exception, SystemExit)
    assert "public-only" in result.output
    assert not image_signed.exists()


def test_signing_protocols_narrow_via_isinstance() -> None:
    """Runtime smoke test for signing Protocol discrimination.

    The real contract is the explicit PayloadSigner/DigestSigner
    inheritance on each private-key wrapper plus @override on their
    sign methods, both enforced by mypy. This test adds runtime
    coverage plus, via assert_type after isinstance narrowing from
    `object`, a non-tautological check that mypy recognizes the
    runtime_checkable Protocols.
    """
    priv_ed = keys.Ed25519.generate()
    priv_rsa = keys.RSA.generate()
    priv_ec256 = keys.ECDSA256P1.generate()
    priv_ec384 = keys.ECDSA384P1.generate()
    priv_x = keys.X25519.generate()

    # Static: narrowing from `object` proves mypy recognizes each Protocol
    obj_ed: object = priv_ed
    assert isinstance(obj_ed, keys.DigestSigner)
    assert_type(obj_ed, keys.DigestSigner)

    obj_rsa: object = priv_rsa
    assert isinstance(obj_rsa, keys.PayloadSigner)
    assert_type(obj_rsa, keys.PayloadSigner)

    # Runtime: each private wrapper satisfies its expected Protocol only
    for priv, expected, other in (
        (priv_ed, keys.DigestSigner, keys.PayloadSigner),
        (priv_rsa, keys.PayloadSigner, keys.DigestSigner),
        (priv_ec256, keys.PayloadSigner, keys.DigestSigner),
        (priv_ec384, keys.PayloadSigner, keys.DigestSigner),
        (priv_x, keys.DigestSigner, keys.PayloadSigner),
    ):
        assert isinstance(priv, expected)
        assert not isinstance(priv, other)

    # Runtime: public wrappers satisfy neither signing Protocol
    for pub in (
        keys.Ed25519Public(priv_ed.key.public_key()),
        keys.RSAPublic(priv_rsa.key.public_key()),
        keys.ECDSA256P1Public(priv_ec256.key.public_key()),
        keys.ECDSA384P1Public(priv_ec384.key.public_key()),
        keys.X25519Public(priv_x.key.public_key()),
    ):
        assert not isinstance(pub, keys.DigestSigner)
        assert not isinstance(pub, keys.PayloadSigner)
