# Copyright 2024 Denis Mingulov
# Copyright 2024 Arm Limited
#
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

import re
import subprocess
from tempfile import mktemp

import pytest
from click.testing import CliRunner

from imgtool.main import imgtool
from tests.constants import KEY_TYPES, GEN_KEY_EXT, tmp_name, OPENSSL_KEY_TYPES, KEY_ENCODINGS, PUB_KEY_EXT, \
    PVT_KEY_FORMATS, KEY_LANGS, PUB_HASH_ENCODINGS, PUB_KEY_HASH_EXT, keys_dir

UNSUPPORTED_TYPES = [("openssl", "ed25519"),
                     ("openssl", "x25519"),
                     ("pkcs8", "rsa-2048"),
                     ("pkcs8", "rsa-3072"),
                     ("pkcs8", "ed25519"), ]

OPENSSL_PUBLIC_KEY_TYPES = {
    "rsa-2048": "Public-Key: (2048 bit)",
    "rsa-3072": "Public-Key: (3072 bit)",
    "ecdsa-p256": "Public-Key: (256 bit)",
    "ecdsa-p384": "Public-Key: (384 bit)",
    "ed25519": "ED25519 Public-Key:",
    "x25519": "X25519 Public-Key:",
}

GEN_ANOTHER_KEY_EXT = ".another.key"


def verify_key(key_type, key, password=""):
    """Check generated keys"""
    assert key_type in OPENSSL_KEY_TYPES

    result = subprocess.run(
        ["openssl", "pkey", "-in", str(key), "-check", "-noout", "-text", "-passin", "pass:" + password],
        capture_output=True,
        text=True,

    )
    assert result.returncode == 0
    assert "Key is valid" in result.stdout
    assert OPENSSL_KEY_TYPES[key_type] in result.stdout


def verify_public_key(key_type, encoding, key, password=""):
    """Check generated keys"""

    key_file = str(key)
    if encoding not in ['pem', 'raw']:
        # convert to raw
        # read file key to key_str
        with open(key, 'r') as file:
            hex_values = re.findall(r'0x([a-fA-F0-9]{2}),', file.read())
            byte_data = bytes(int(h, 16) for h in hex_values)

        raw_file = mktemp("key.raw")
        with open(raw_file, 'wb') as file:
            file.write(byte_data)

        key_file = raw_file

    result = subprocess.run(
        ["openssl", "pkey", "-in", key_file, "-pubin", "-pubcheck", "-noout", "-text"],
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0
    assert "Key is valid" in result.stdout
    assert OPENSSL_PUBLIC_KEY_TYPES[key_type] in result.stdout


class TestKeys:
    runner = CliRunner()
    password = "12345"  # Password for the keys in ./keys directory

    @pytest.fixture(scope="session")
    def tmp_path_persistent(self, tmp_path_factory):
        return tmp_path_factory.mktemp("keys")


class TestKeygen(TestKeys):

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_keygen(self, key_type, tmp_path_persistent):
        """Generate keys by imgtool"""

        gen_key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)

        assert not gen_key.exists()
        result = self.runner.invoke(
            imgtool, ["keygen", "--key", str(gen_key), "--type", key_type]
        )
        assert result.exit_code == 0
        assert gen_key.exists()
        assert gen_key.stat().st_size > 0
        verify_key(key_type, gen_key)

        # another key
        gen_key2 = tmp_name(tmp_path_persistent, key_type, GEN_ANOTHER_KEY_EXT)

        assert str(gen_key2) != str(gen_key)

        assert not gen_key2.exists()
        result = self.runner.invoke(
            imgtool, ["keygen", "--key", str(gen_key2), "--type", key_type]
        )
        assert result.exit_code == 0
        assert gen_key2.exists()
        assert gen_key2.stat().st_size > 0
        verify_key(key_type, gen_key2)

        # content must be different
        assert gen_key.read_bytes() != gen_key2.read_bytes()

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_keygen_with_password(self, key_type, tmp_path_persistent, monkeypatch):
        """Generate keys by imgtool with password"""

        gen_key = tmp_name(tmp_path_persistent, key_type + "_passwd", GEN_KEY_EXT)
        assert not gen_key.exists()
        monkeypatch.setattr('getpass.getpass', lambda _: self.password)
        result = self.runner.invoke(
            imgtool, ["keygen", "--key", str(gen_key), "--type", key_type, "-p"]
        )

        assert result.exit_code == 0
        assert gen_key.exists()
        assert gen_key.stat().st_size > 0
        verify_key(key_type, gen_key, self.password)

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_keygen_with_mismatching_password(self, key_type, tmp_path_persistent, monkeypatch):
        """Generate keys by imgtool with mismatching password should print error"""

        gen_key = tmp_name(tmp_path_persistent, key_type + "-null", GEN_KEY_EXT)
        assert not gen_key.exists()
        passwords = iter([self.password, 'invalid'])
        monkeypatch.setattr('getpass.getpass', lambda _: next(passwords))
        result = self.runner.invoke(
            imgtool, ["keygen", "--key", str(gen_key), "--type", key_type, "-p"],
        )

        assert result.exit_code != 0
        assert not gen_key.exists()
        assert "Passwords do not match, try again" in result.stdout


class TestGetPriv(TestKeys):

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    @pytest.mark.parametrize("key_format", PVT_KEY_FORMATS)
    def test_getpriv(self, key_type, key_format, tmp_path_persistent):
        """Get private key"""

        gen_key = keys_dir + "/" + key_type + GEN_KEY_EXT

        result = self.runner.invoke(
            imgtool,
            [
                "getpriv",
                "--key",
                str(gen_key),
                "--format",
                key_format,
            ],
        )
        assert result.exit_code == 0

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    @pytest.mark.parametrize("key_format", PVT_KEY_FORMATS)
    def test_getpriv_with_password(self, key_type, key_format, tmp_path_persistent, monkeypatch):
        """Get private key with password"""

        gen_key = keys_dir + "/" + key_type + "_passwd" + GEN_KEY_EXT

        monkeypatch.setattr('getpass.getpass', lambda _: self.password)
        result = self.runner.invoke(
            imgtool,
            [
                "getpriv",
                "--key",
                str(gen_key),
                "--format",
                key_format,
            ]
        )
        assert result.exit_code == 0

    @pytest.mark.parametrize("key_format, key_type", UNSUPPORTED_TYPES)
    def test_getpriv_unsupported(self, key_format, key_type, tmp_path_persistent):
        """Get private key for unsupported combinations should print error message"""

        gen_key = keys_dir + "/" + key_type + GEN_KEY_EXT

        result = self.runner.invoke(
            imgtool,
            [
                "getpriv",
                "--key",
                str(gen_key),
                "--format",
                key_format,
            ],
        )

        assert result.exit_code == 2
        assert "not support" in result.stdout

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    @pytest.mark.parametrize("key_format", PVT_KEY_FORMATS)
    def test_getpriv_with_invalid_pass(self, key_type, key_format, tmp_path_persistent, monkeypatch):
        """Get private key with invalid password should print 'Invalid passphrase' in stdout"""

        gen_key = keys_dir + "/" + key_type + "_passwd" + GEN_KEY_EXT

        monkeypatch.setattr('getpass.getpass', lambda _: 'invalid')
        result = self.runner.invoke(
            imgtool,
            [
                "getpriv",
                "--key",
                str(gen_key),
                "--format",
                key_format,
            ],
        )
        assert result.exit_code != 0
        assert "Invalid passphrase" in result.stdout


class TestGetPub(TestKeys):

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_getpub(self, key_type, tmp_path_persistent):
        """Get public key - Default lang is c """

        gen_key = keys_dir + "/" + key_type + GEN_KEY_EXT
        pub_key = tmp_name(tmp_path_persistent, key_type, "." + "default" + PUB_KEY_EXT)

        assert not pub_key.exists()
        result = self.runner.invoke(
            imgtool,
            [
                "getpub",
                "--key",
                gen_key,
                "--output",
                str(pub_key),
            ],
        )
        assert result.exit_code == 0
        assert pub_key.exists()
        assert pub_key.stat().st_size > 0
        verify_public_key(key_type, "c", pub_key)

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    @pytest.mark.parametrize("encoding", KEY_ENCODINGS)
    def test_getpub_with_encoding(self, key_type, encoding, tmp_path_persistent):
        """Get public key with encoding"""

        gen_key = keys_dir + "/" + key_type + GEN_KEY_EXT
        pub_key = tmp_name(tmp_path_persistent, key_type, "." + encoding + PUB_KEY_EXT)

        assert not pub_key.exists()
        result = self.runner.invoke(
            imgtool,
            [
                "getpub",
                "--key",
                gen_key,
                "--output",
                str(pub_key),
                "--encoding",
                encoding,
            ],
        )
        assert result.exit_code == 0
        assert pub_key.exists()
        assert pub_key.stat().st_size > 0
        verify_public_key(key_type, encoding, pub_key)

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    @pytest.mark.parametrize("lang", KEY_LANGS)
    def test_getpub_with_lang(self, key_type, lang, tmp_path_persistent):
        """Get public key with specified lang"""

        gen_key = keys_dir + "/" + key_type + GEN_KEY_EXT
        pub_key = tmp_name(tmp_path_persistent, key_type, PUB_KEY_EXT + "." + lang)

        assert not pub_key.exists()
        result = self.runner.invoke(
            imgtool,
            [
                "getpub",
                "--key",
                gen_key,
                "--output",
                str(pub_key),
                "--lang",
                lang,
            ],
        )
        assert result.exit_code == 0
        assert pub_key.exists()
        assert pub_key.stat().st_size > 0
        verify_public_key(key_type, lang, pub_key)

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    @pytest.mark.parametrize("encoding", KEY_ENCODINGS)
    def test_getpub_no_outfile(self, key_type, encoding, tmp_path_persistent):
        """Get public key without output file should dump only to stdout"""

        gen_key = keys_dir + "/" + key_type + GEN_KEY_EXT
        pub_key = tmp_name(tmp_path_persistent, key_type, "." + encoding + "-null" + PUB_KEY_EXT)

        assert not pub_key.exists()
        result = self.runner.invoke(
            imgtool,
            [
                "getpub",
                "--key",
                gen_key,
                "--encoding",
                encoding,
            ],
        )
        assert result.exit_code == 0
        assert len(result.stdout) != 0
        assert not pub_key.exists()

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_getpub_invalid_encoding(self, key_type, tmp_path_persistent):
        """Get public key with invalid encoding should error message in stdout"""

        gen_key = keys_dir + "/" + key_type + GEN_KEY_EXT
        pub_key = tmp_name(tmp_path_persistent, key_type, "." + PUB_KEY_EXT)

        assert not pub_key.exists()
        result = self.runner.invoke(
            imgtool,
            [
                "getpub",
                "--key",
                gen_key,
                "--output",
                str(pub_key),
                "--encoding",
                "invalid",
            ],
        )
        assert result.exit_code != 0
        assert "Invalid value for '-e' / '--encoding':" in result.stdout

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_getpub_invalid_lang(self, key_type, tmp_path_persistent):
        """Get public key with invalid lang should print error message in stdout"""

        gen_key = keys_dir + "/" + key_type + GEN_KEY_EXT
        pub_key = tmp_name(tmp_path_persistent, key_type, "." + PUB_KEY_EXT)

        assert not pub_key.exists()
        result = self.runner.invoke(
            imgtool,
            [
                "getpub",
                "--key",
                gen_key,
                "--output",
                str(pub_key),
                "--lang",
                "invalid",
            ],
        )
        assert result.exit_code != 0
        assert "Invalid value for '-l' / '--lang':" in result.stdout

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    @pytest.mark.parametrize("encoding", KEY_ENCODINGS)
    @pytest.mark.parametrize("lang", KEY_LANGS)
    def test_getpub_with_encoding_and_lang(self, key_type, encoding, lang, tmp_path_persistent):
        """Get public key with both encoding and lang should print error message"""

        gen_key = keys_dir + "/" + key_type + GEN_KEY_EXT
        pub_key = tmp_name(tmp_path_persistent, key_type, "." + lang + encoding + PUB_KEY_EXT)

        assert not pub_key.exists()
        result = self.runner.invoke(
            imgtool,
            [
                "getpub",
                "--key",
                gen_key,
                "--output",
                str(pub_key),
                "--encoding",
                encoding,
                "--lang",
                lang
            ],
        )
        assert result.exit_code != 0
        assert "Please use only one of `--encoding/-e` or `--lang/-l`" in result.stdout

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    @pytest.mark.parametrize("encoding", KEY_ENCODINGS)
    def test_getpub_invalid_pass(self, key_type, encoding, tmp_path_persistent, monkeypatch):
        """Get public key with invalid password should print 'Invalid passphrase' in stdout"""

        gen_key = keys_dir + "/" + key_type + "_passwd" + GEN_KEY_EXT
        pub_key = tmp_name(tmp_path_persistent, key_type, "." + encoding + PUB_KEY_EXT)

        monkeypatch.setattr('getpass.getpass', lambda _: 'invalid')
        result = self.runner.invoke(
            imgtool,
            [
                "getpub",
                "--key",
                gen_key,
                "--output",
                str(pub_key),
                "--encoding",
                encoding,
            ]
        )
        assert result.exit_code != 0
        assert "Invalid passphrase" in result.stdout

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    @pytest.mark.parametrize("encoding", KEY_ENCODINGS)
    def test_getpub_null_pass(self, key_type, encoding, tmp_path_persistent, monkeypatch):
        """Get public key with invalid password should print 'Invalid passphrase' in stdout"""

        gen_key = keys_dir + "/" + key_type + "_passwd" + GEN_KEY_EXT
        pub_key = tmp_name(tmp_path_persistent, key_type, "." + encoding + PUB_KEY_EXT)

        monkeypatch.setattr('getpass.getpass', lambda _: '')
        result = self.runner.invoke(
            imgtool,
            [
                "getpub",
                "--key",
                gen_key,
                "--output",
                str(pub_key),
                "--encoding",
                encoding,
            ]
        )
        assert result.exit_code != 0
        assert "Password was not given" in result.stdout


class TestGetPubHash(TestKeys):

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_getpubhash(self, key_type, tmp_path_persistent):
        """Get the hash of the public key - Default encoding is lang-c"""

        gen_key = keys_dir + "/" + key_type + GEN_KEY_EXT
        pub_key_hash = tmp_name(
            tmp_path_persistent, key_type, "." + "default" + PUB_KEY_HASH_EXT
        )

        assert not pub_key_hash.exists()
        result = self.runner.invoke(
            imgtool,
            [
                "getpubhash",
                "--key",
                str(gen_key),
                "--output",
                str(pub_key_hash),
            ],
        )
        assert result.exit_code == 0
        assert pub_key_hash.exists()
        assert pub_key_hash.stat().st_size > 0

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    @pytest.mark.parametrize("encoding", PUB_HASH_ENCODINGS)
    def test_getpubhash_with_encoding(self, key_type, encoding, tmp_path_persistent):
        """Get the hash of the public key with encoding"""

        gen_key = keys_dir + "/" + key_type + GEN_KEY_EXT
        pub_key_hash = tmp_name(
            tmp_path_persistent, key_type, "." + encoding + PUB_KEY_HASH_EXT
        )

        assert not pub_key_hash.exists()
        result = self.runner.invoke(
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
    @pytest.mark.parametrize("encoding", PUB_HASH_ENCODINGS)
    def test_getpubhash_with_no_output(self, key_type, encoding, tmp_path_persistent):
        """Get the hash of the public key without outfile should dump only to stdout"""

        gen_key = keys_dir + "/" + key_type + GEN_KEY_EXT
        pub_key_hash = tmp_name(
            tmp_path_persistent, key_type, "." + encoding + "-null" + PUB_KEY_HASH_EXT
        )

        assert not pub_key_hash.exists()
        result = self.runner.invoke(
            imgtool,
            [
                "getpubhash",
                "--key",
                str(gen_key),
                "--encoding",
                encoding,
            ],
        )
        assert result.exit_code == 0
        assert len(result.stdout) != 0
        assert not pub_key_hash.exists()

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_getpubhash_with_invalid_encoding(self, key_type, tmp_path_persistent):
        """Get the hash of the public key with encoding"""

        gen_key = keys_dir + "/" + key_type + GEN_KEY_EXT
        pub_key_hash = tmp_name(
            tmp_path_persistent, key_type, "." + "invalid" + PUB_KEY_HASH_EXT
        )

        assert not pub_key_hash.exists()
        result = self.runner.invoke(
            imgtool,
            [
                "getpubhash",
                "--key",
                str(gen_key),
                "--output",
                str(pub_key_hash),
                "--encoding",
                "invalid",
            ],
        )
        assert result.exit_code != 0
        assert "Invalid value for '-e' / '--encoding':" in result.stdout
        assert not pub_key_hash.exists()

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    @pytest.mark.parametrize("encoding", PUB_HASH_ENCODINGS)
    def test_getpubhash_with_invalid_pass(self, key_type, encoding, tmp_path_persistent, monkeypatch):
        """Get the hash of the public key with invalid password should print 'Invalid passphrase' in stdout"""

        gen_key = keys_dir + "/" + key_type + "_passwd" + GEN_KEY_EXT
        pub_key_hash = tmp_name(
            tmp_path_persistent, key_type, "." + encoding + "-null" + PUB_KEY_HASH_EXT
        )

        assert not pub_key_hash.exists()

        monkeypatch.setattr('getpass.getpass', lambda _: 'invalid')
        result = self.runner.invoke(
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
            input="invalid"
        )
        assert result.exit_code != 0
        assert not pub_key_hash.exists()
        assert "Invalid passphrase" in result.stdout
