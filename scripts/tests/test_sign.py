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

import io
import os
from contextlib import redirect_stdout
from pathlib import Path

import pytest
from click.testing import CliRunner

from imgtool.dumpinfo import dump_imginfo
from imgtool.main import imgtool
from tests.constants import KEY_TYPES, GEN_KEY_EXT, PUB_KEY_EXT, tmp_name, images_dir, keys_dir

try:
    KEY_TYPES.remove("x25519")  # x25519 is not used for signing, so directory does not contain any such image
except ValueError:
    pass


def assert_image_signed(result, image_signed, verify=True):
    assert result.exit_code == 0
    assert image_signed.exists()
    assert image_signed.stat().st_size > 0

    # Verify unless specified otherwise
    if verify:
        runner = CliRunner()

        result = runner.invoke(
            imgtool, ["verify", str(image_signed), ],
        )
        assert result.exit_code == 0


class TestSign:
    image = None
    image_signed = None
    runner = CliRunner()
    key = None

    @pytest.fixture(scope="class")
    def tmp_path_persistent(self, tmp_path_factory):
        return tmp_path_factory.mktemp("keys")

    @pytest.fixture(autouse=True)
    def setup(self, tmp_path_persistent):
        """Generate keys and images for testing"""

        self.image = Path(images_dir + "/zero.bin")
        self.image_signed = tmp_name(tmp_path_persistent, "image", ".signed")

    @pytest.fixture(autouse=True)
    def teardown(self):
        if os.path.exists(str(self.image_signed)):
            os.remove(str(self.image_signed))


class TestSignBasic(TestSign):

    def test_sign_basic(self, tmp_path_persistent):
        """Test basic sign and verify"""

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x2500",
                "--pad-header",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed)

    @pytest.mark.parametrize("max_align", ("8", "16", "32"))
    def test_sign_max_align(self, tmp_path_persistent, max_align):
        """Test sign with max align"""

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x2500",
                "--pad-header",
                "--max-align",
                max_align,
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed)

    @pytest.mark.parametrize("version, expected", [
        ("0.0.0", 0),
        ("1.0.0", 0),
        ("12.2.34", 0),
        ("12.3.45+67", 0),
        ("0.0.0+0", 0),
        ("asd", 2),
        ("2.2.a", 2),
        ("a.2.3", 2),
        ("1.2.3.4", 2),
        ("0.0.0+00", 2),

    ])
    def test_sign_validations_version(self, tmp_path_persistent, version, expected):
        """Test Signature validation"""

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                version,
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code == expected

    @pytest.mark.parametrize("sec_counter, expected", [
        ("auto", 0),
        ("1", 0),
        ("2", 0),
        ("inv", 2),

    ])
    def test_sign_validations_security_counter(self, tmp_path_persistent, sec_counter, expected):
        """Test security counter validation"""

        # validate security counter
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--security-counter",
                sec_counter,
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code == expected

    @pytest.mark.parametrize(
        ("header_size", "expected"),
        ((0, 2),
         (64, 0),
         ("invalid", 2),),
    )
    def test_sign_validations_header_size(self, tmp_path_persistent, header_size, expected):
        """Test header size validation"""

        # Header size validations
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                str(header_size),
                "--slot-size",
                "0x10000",
                "--pad-header",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code == expected

    @pytest.mark.parametrize("dependencies, expected", [
        ("(1, 1.2.3+0)", 0),
        ("", 2),
        ("(1, 1.2.3+0)(2)", 2),
        ("(1, 1.)", 2),
        ("1, 1.2)", 2),

    ])
    def test_sign_dependencies(self, tmp_path_persistent, dependencies, expected):
        """Test with dependencies"""

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                str(self.image),
                str(self.image_signed),
                "--dependencies",
                dependencies,
            ],
        )
        assert result.exit_code == expected

    def test_sign_header_not_padded(self, tmp_path_persistent):
        """Test with un-padded header with non-zero start should fail"""

        self.image = Path(images_dir + "/one.bin")

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x2500",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert "Header padding was not requested" in result.output

    def test_sign_pad(self, tmp_path_persistent):
        """Test sign pad"""

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x2500",
                "--pad-header",
                "--pad",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed)

    @pytest.mark.parametrize("erased_val", ("0", "0xff"))
    def test_sign_pad_erased_val(self, tmp_path_persistent, erased_val):
        """Test sign pad"""

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x2500",
                "--pad-header",
                "--pad",
                "--erased-val",
                erased_val,
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed)

        # Capture dumpinfo stdout to check value used for padding
        f = io.StringIO()
        with redirect_stdout(f):
            dump_imginfo(self.image_signed)
        result = f.getvalue()

        assert f"padding ({erased_val}" in result

    def test_sign_confirm(self, tmp_path_persistent):
        """Test with confirm"""

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--confirm",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code == 0

    def test_sign_max_sectors(self, tmp_path_persistent):
        """Test with max sectors"""

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--max-sectors",
                "256",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed)

    @pytest.mark.parametrize("boot_record, expected", [
        ("BABE", 0),
        ("DEADBEEF", 0),
        ("DEADBEEFF00D", 0),
        ("DEADBEEFF00DB", 2),
        ("DEADBEEFF00DBABE", 2),
    ])
    def test_sign_boot_record(self, tmp_path_persistent, boot_record, expected):
        """Test with boot record"""

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x2500",
                "--pad-header",
                "--boot-record",
                boot_record,
                str(self.image),
                str(self.image_signed),
            ],
        )
        if expected != 0:
            assert result.exit_code != 0
            assert "is too long" in result.stdout
        else:
            assert_image_signed(result, self.image_signed)

    def test_sign_overwrite_only(self, tmp_path_persistent):
        """Test sign overwrite"""

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--overwrite-only",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed)

    @pytest.mark.parametrize("endian", ("little", "big"))
    def test_sign_endian(self, tmp_path_persistent, endian):
        """Test sign endian"""

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x2500",
                "--pad-header",
                "--endian",
                endian,
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed, verify=False)

    def test_sign_rom_fixed(self, tmp_path_persistent):
        """Test sign with rom fixed"""

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x2500",
                "--pad-header",
                "--rom-fixed",
                "1",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed)

    @pytest.mark.parametrize("vector", ("digest", "payload",))
    def test_sign_vector_to_sign(self, tmp_path_persistent, vector):
        """Test with vector to sign"""

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x2500",
                "--pad-header",
                "--vector-to-sign",
                vector,
                str(self.image),
                str(self.image_signed),
            ],
        )
        # Don't verify as output is not expected to be a valid image
        assert_image_signed(result, self.image_signed, verify=False)


class TestSignKey(TestSign):
    @pytest.fixture(autouse=True)
    def setup_key(self, request, tmp_path_persistent, key_type):
        """Generate keys for testing"""
        self.key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
        self.runner.invoke(
            imgtool, ["keygen", "--key", str(self.key), "--type", key_type]
        )

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_sign_missing_args(self, key_type, tmp_path_persistent):
        """Test basic sign with missing args should fail"""

        key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)

        # not all required arguments are provided
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                key,
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert not self.image_signed.exists()
        assert "Error: Missing option" in result.stdout

    @pytest.mark.parametrize("key_format", ("hash", "full",))
    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_sign_pub_key_format(self, key_type, tmp_path_persistent, key_format):
        """Test with pub key format"""

        loaded_key = keys_dir + key_type + ".key"

        result = self.runner.invoke(
            imgtool,
            [
                "sign",

                "--key",
                loaded_key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x2500",
                "--pad-header",
                "--public-key-format",
                key_format,
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed)

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_sign_fix_sig(self, tmp_path_persistent, key_type):
        """Test with fix signature"""

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--fix-sig",
                "/dev/null",
                "--fix-sig-pubkey",
                self.key,
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed)

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                self.key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--fix-sig",
                "/dev/null",
                "--fix-sig-pubkey",
                self.key,
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert "Can not sign using key and provide fixed-signature at" in result.output

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--fix-sig",
                "/dev/null",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert "public key of the fixed signature is not specified" in result.output

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_sign_sig_out(self, tmp_path_persistent, key_type):
        """Test signature output"""

        key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
        self.runner.invoke(
            imgtool, ["keygen", "--key", str(key), "--type", key_type]
        )
        sig_file = tmp_name(tmp_path_persistent, "signature", "sig")

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                str(self.image),
                str(self.image_signed),
                "--sig-out",
                str(sig_file),
            ],
        )
        assert_image_signed(result, self.image_signed)

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_sign_load_addr(self, key_type, tmp_path_persistent):
        """Test sign with load addr"""

        loaded_key = keys_dir + key_type + ".key"

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                loaded_key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x2500",
                "--pad-header",
                "--load-addr",
                "1",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed)

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_sign_load_addr_rom_fixed(self, key_type, tmp_path_persistent):
        """Test sign with both load addr and rom fixed should fail"""

        loaded_key = keys_dir + key_type + ".key"

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                loaded_key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x2500",
                "--pad-header",
                "--load-addr",
                "1",
                "--rom-fixed",
                "1",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert "Can not set rom_fixed and load_addr at the same time" in result.output

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_sign_custom_tlv(self, tmp_path_persistent, key_type):
        """Test signing with custom TLV"""

        key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)

        # Sign
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--custom-tlv",
                "0x00a0",
                "0x00ff",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed)

        # Sign - TLV value in octal
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--custom-tlv",
                "256",
                "64",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code == 0

        # Fail - same TLV twice
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--custom-tlv",
                "0x00a0",
                "0x00ff",
                "--custom-tlv",
                "0x00a0",
                "0x00ff",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert "already exists" in result.stdout

        # Fail - predefined TLV
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--custom-tlv",
                "0x00a0",
                "0x00ff",
                "--custom-tlv",
                "0x0010",
                "0x00ff",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert "conflicts with predefined TLV" in result.stdout

        # Fail - odd TLV length
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--custom-tlv",
                "0x00a0",
                "0x00fff",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert "Custom TLV length is odd" in result.stdout

        #  Fail - TLV < 0x00a0
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--custom-tlv",
                "159",  # MIN is 160
                "DEAD",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert "Invalid custom TLV type value" in result.stdout

        #  Fail - TLV > 0xfffe
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--custom-tlv",
                "65535",  # MAX is 65534
                "DEAD",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert "Invalid custom TLV type value" in result.stdout

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_sign_pad_sig(self, tmp_path_persistent, key_type):
        """Test pad signature"""

        key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--pad-sig",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed)


class TestSignEncrypt(TestSign):

    @pytest.fixture(autouse=True)
    def setup_enc(self, tmp_path_persistent, key_type):
        """Generate keys and images for testing"""
        self.key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
        self.runner.invoke(
            imgtool, ["keygen", "--key", str(self.key), "--type", key_type]
        )

        encoding = "pem"
        self.pub_key = tmp_name(tmp_path_persistent, key_type, PUB_KEY_EXT + "." + encoding)

        self.runner.invoke(
            imgtool, ["keygen", "--key", self.key, "--type", key_type]
        )

        # Get public pair for key
        self.runner.invoke(
            imgtool,
            [
                "getpub",
                "--key",
                self.key,
                "--output",
                self.pub_key,
                "--encoding",
                encoding,
            ],
        )

    @pytest.mark.parametrize("key_len", ("128", "256",))
    @pytest.mark.parametrize("align", ("1", "2", "4", "8", "16", "32",))
    @pytest.mark.parametrize("key_type", KEY_TYPES[:-1])
    def test_sign_enc_key(self, tmp_path_persistent, key_type, align, key_len):
        """Test signing with encryption"""

        encoding = "pem"
        key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
        pub_key = tmp_name(tmp_path_persistent, key_type, PUB_KEY_EXT + "." + encoding)

        # Sign and encrypt
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                key,
                "--encrypt",
                pub_key,
                "--align",
                align,
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--encrypt-keylen",
                key_len,
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed, verify=False)

        # Encryption key is not public should fail
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                key,
                "--encrypt",
                key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0

    @pytest.mark.parametrize("align", ("1", "2", "4", "8", "16", "32",))
    @pytest.mark.parametrize("key_type", KEY_TYPES[:-1])
    def test_sign_enc_key_non_16(self, tmp_path_persistent, key_type, align):
        """Test signing with encryption with file with size that is not divisible by 16"""

        encoding = "pem"
        key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
        pub_key = tmp_name(tmp_path_persistent, key_type, PUB_KEY_EXT + "." + encoding)
        self.image = tmp_name(tmp_path_persistent, "1023", ".bin")
        with self.image.open("wb") as f:
            f.write(b"\x00" * 1023)

        # Sign and encrypt
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                key,
                "--encrypt",
                pub_key,
                "--align",
                align,
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                str(self.image),
                str(self.image_signed),
            ],
        )
        # Don't verify as verify command does not support encrypted images yet
        assert_image_signed(result, self.image_signed, verify=False)

    @pytest.mark.parametrize("key_type", ("ed25519",))
    def test_sign_enc_key_x25519(self, tmp_path_persistent, key_type):
        """Test signing with ed25519 and encrypting with x25519"""

        encoding = "pem"
        key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
        enckey = tmp_name(tmp_path_persistent, "x25519", GEN_KEY_EXT)
        pub_key = tmp_name(tmp_path_persistent, key_type, PUB_KEY_EXT + "." + encoding)

        self.runner.invoke(
            imgtool, ["keygen", "--key", str(enckey), "--type", "x25519"]
        )

        # Get public pair for key
        self.runner.invoke(
            imgtool,
            [
                "getpub",
                "--key",
                str(enckey),
                "--output",
                str(pub_key),
                "--encoding",
                encoding,
            ],
        )

        # Sign and encrypt
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                key,
                "--encrypt",
                pub_key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--clear",
                str(self.image),
                str(self.image_signed),
            ],
        )
        # Can be verified as --clean option used during signing
        assert_image_signed(result, self.image_signed)

        # Encryption key is not public should fail
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                key,
                "--encrypt",
                key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0

    @pytest.mark.parametrize("key_type", KEY_TYPES[:-1])
    def test_sign_enc_save_enctlv(self, tmp_path_persistent, key_type):
        """Test with save enctlv"""

        encoding = "pem"
        key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
        pub_key = tmp_name(tmp_path_persistent, key_type, PUB_KEY_EXT + "." + encoding)

        # Sign and encrypt
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                key,
                "--encrypt",
                pub_key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--save-enctlv",
                str(self.image),
                str(self.image_signed),
            ],
        )
        # Don't verify as verify command does not support encrypted images yet
        assert_image_signed(result, self.image_signed, verify=False)


class TestSignHex(TestSign):

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_sign_basic_hex(self, key_type, tmp_path_persistent):
        """Test basic sign and verify hex file"""

        loaded_key = keys_dir + key_type + ".key"
        self.image = images_dir + "/zero.hex"
        self.image_signed = tmp_name(tmp_path_persistent, "image", ".hex")

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                loaded_key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x2048",
                "--pad-header",
                "--hex-addr",
                "0",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed)

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_sign_hex_file_not_exists(self, key_type, tmp_path_persistent):
        """Test sign hex file with non-existent file"""

        loaded_key = keys_dir + key_type + ".key"
        self.image = "./images/invalid/path/file.hex"
        self.image_signed = tmp_name(tmp_path_persistent, "image", ".hex")

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                loaded_key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x2048",
                "--pad-header",
                "--hex-addr",
                "0",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert "Image file not found" in result.output

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_sign_hex_padded(self, key_type, tmp_path_persistent):
        """Test hex file with pad"""

        loaded_key = keys_dir + key_type + ".key"
        self.image = images_dir + "/one_offset.hex"
        # image above generated as below:
        # ih = IntelHex()
        # ih.puts(10, '1' * 1024)
        # ih.write_hex_file(str(self.image))

        self.image_signed = tmp_name(tmp_path_persistent, "image", ".hex")

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                loaded_key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x2048",
                "--pad-header",
                "--hex-addr",
                "1",
                "--pad",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed)

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_sign_hex_confirm(self, key_type, tmp_path_persistent):
        """Test hex file with confirm"""

        loaded_key = keys_dir + key_type + ".key"
        self.image = images_dir + "/zero.hex"
        self.image_signed = tmp_name(tmp_path_persistent, "image", ".hex")

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                loaded_key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x2048",
                "--pad-header",
                "--hex-addr",
                "0",
                "--confirm",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert_image_signed(result, self.image_signed)

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_sign_basic_hex_bad_image_size(self, key_type, tmp_path_persistent):
        """Test basic sign hex file with insufficient size should fail"""

        loaded_key = keys_dir + key_type + ".key"
        self.image = images_dir + "/zero.hex"
        self.image_signed = tmp_name(tmp_path_persistent, "image", ".hex")

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                loaded_key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x20",
                "--slot-size",
                "0x1024",
                "--pad-header",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert "exceeds requested size" in result.output
