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

from pathlib import Path
from tempfile import mktemp

import pytest
from click.testing import CliRunner

from imgtool.main import imgtool
from tests.constants import tmp_name, KEY_TYPES, signed_images_dir, images_dir

DUMPINFO_SUCCESS_LITERAL = "dumpinfo has run successfully"

DUMPINFO_TRAILER = """
swap status: (len: unknown)
enc. keys:   Image not encrypted
swap size:   unknown
swap_info:   INVALID (0xff)
copy_done:   INVALID (0xff)
image_ok:    SET (0x1)
boot magic:  """

DUMPINFO_ENCRYPTED_TRAILER = """
swap status: (len: unknown)
enc. keys:   (len: 0x20, if BOOT_SWAP_SAVE_ENCTLV is unset)
swap size:   unknown
swap_info:   INVALID (0xff)
copy_done:   INVALID (0xff)
image_ok:    INVALID (0xff)
boot magic:  """


class TestDumpInfo:
    image = None
    image_signed = None
    runner = CliRunner()
    dumpfile = mktemp("dump.dat")

    @pytest.fixture(scope="session")
    def tmp_path_persistent(self, tmp_path_factory):
        return tmp_path_factory.mktemp("keys")

    @pytest.fixture(autouse=True)
    def setup(self, tmp_path_persistent, key_type="rsa-2048"):
        """Generate keys and images for testing"""

        self.key = tmp_name(tmp_path_persistent, key_type, ".key")
        self.runner.invoke(
            imgtool, ["keygen", "--key", str(self.key), "--type", key_type]
        )

        self.image = images_dir + "/zero.bin"
        self.image_signed = tmp_name(tmp_path_persistent, "image", ".signed")

    @pytest.mark.parametrize("key_type", ("rsa-2048",))
    def test_dumpinfo(self, tmp_path_persistent, key_type):
        self.image_signed = signed_images_dir + "/basic/" + key_type + ".bin"

        result = self.runner.invoke(
            imgtool, ["dumpinfo", str(self.image_signed)]
        )
        assert result.exit_code == 0
        assert DUMPINFO_SUCCESS_LITERAL in result.stdout

        result = self.runner.invoke(
            imgtool, ["dumpinfo", "invalid"]
        )
        assert result.exit_code != 0
        assert "Error: Image file not found" in result.stdout

    @pytest.mark.parametrize("key_type", ("rsa-2048",))
    def test_dumpinfo_silent(self, tmp_path_persistent, key_type):
        self.image_signed = signed_images_dir + "/basic/" + key_type + ".bin"

        result = self.runner.invoke(
            imgtool, ["dumpinfo", str(self.image_signed), "-s"]
        )
        assert result.exit_code == 0
        assert result.stdout == ""

        result = self.runner.invoke(
            imgtool, ["dumpinfo", "invalid", "-s"]
        )
        assert result.exit_code != 0
        assert "Error: Image file not found" in result.stdout

    @pytest.mark.parametrize("key_type", ("rsa-2048",))
    def test_dumpinfo_tlv(self, tmp_path_persistent, key_type):
        self.image_signed = signed_images_dir + "/customTLV/" + key_type + ".bin"

        result = self.runner.invoke(
            imgtool, ["dumpinfo", str(self.image_signed)]
        )
        assert result.exit_code == 0
        assert DUMPINFO_SUCCESS_LITERAL in result.stdout
        assert "type: UNKNOWN (0xa0)" in result.stdout

    @pytest.mark.parametrize("key_type", ("rsa-2048",))
    def test_dumpinfo_tlv_outfile(self, tmp_path_persistent, key_type):
        self.image_signed = signed_images_dir + "/customTLV/" + key_type + ".bin"

        result = self.runner.invoke(
            imgtool, ["dumpinfo", "-o", self.dumpfile, str(self.image_signed)]
        )
        assert result.exit_code == 0
        assert DUMPINFO_SUCCESS_LITERAL in result.stdout
        assert Path(self.dumpfile).exists()

    @pytest.mark.parametrize("key_type", ("rsa-2048",))
    @pytest.mark.parametrize("align", ('1', '2', '4', '8', '16', '32',))
    def test_dumpinfo_align_pad(self, tmp_path_persistent, key_type, align):
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                self.key,
                "--align",
                align,
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--pad",
                "--confirm",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code == 0

        result = self.runner.invoke(
            imgtool, ["dumpinfo", str(self.image_signed)]
        )
        assert result.exit_code == 0
        assert DUMPINFO_SUCCESS_LITERAL in result.stdout
        assert DUMPINFO_TRAILER in result.stdout

    @pytest.mark.parametrize("key_type", ("rsa-2048",))
    @pytest.mark.parametrize("align", ('1', '2', '4', '8', '16', '32',))
    def test_dumpinfo_align_pad_outfile(self, tmp_path_persistent, key_type, align):
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                self.key,
                "--align",
                align,
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--pad",
                "--confirm",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code == 0

        result = self.runner.invoke(
            imgtool, ["dumpinfo", "-o", self.dumpfile, str(self.image_signed)]
        )
        assert result.exit_code == 0
        assert DUMPINFO_SUCCESS_LITERAL in result.stdout
        assert DUMPINFO_TRAILER in result.stdout
        assert Path(self.dumpfile).exists()

    @pytest.mark.parametrize("key_type", ("rsa-2048",))
    @pytest.mark.parametrize("align", ('1', '2', '4', '8', '16', '32',))
    def test_dumpinfo_max_align_pad(self, tmp_path_persistent, key_type, align):
        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                self.key,
                "--align",
                align,
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x10000",
                "--pad-header",
                "--pad",
                "--confirm",
                "--max-align",
                "32",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code == 0

        result = self.runner.invoke(
            imgtool, ["dumpinfo", str(self.image_signed)]
        )
        assert result.exit_code == 0
        assert DUMPINFO_SUCCESS_LITERAL in result.stdout
        assert DUMPINFO_TRAILER in result.stdout

    @pytest.mark.parametrize("key_type", KEY_TYPES[:-1])
    def test_dumpinfo_encrypted_clear(self, tmp_path_persistent, key_type):
        encoding = "pem"
        enckey = "./keys/" + key_type + ".key"
        pub_key = tmp_name(tmp_path_persistent, key_type, ".pub." + encoding)
        self.image_signed = signed_images_dir + "/encryptedClear/" + key_type + ".enc"

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

        result = self.runner.invoke(
            imgtool, ["dumpinfo", str(self.image_signed)]
        )
        assert result.exit_code == 0
        assert DUMPINFO_SUCCESS_LITERAL in result.stdout

    @pytest.mark.parametrize("key_type", ("rsa-2048",))
    def test_dumpinfo_encrypted_padded_outfile(self, tmp_path_persistent, key_type):
        self.image_signed = signed_images_dir + "/pad_sig_enc_16.bin"
        result = self.runner.invoke(
            imgtool, ["dumpinfo", "-o", self.dumpfile, self.image_signed]
        )
        assert result.exit_code == 0
        assert DUMPINFO_SUCCESS_LITERAL in result.stdout
        assert DUMPINFO_ENCRYPTED_TRAILER in result.stdout
        assert Path(self.dumpfile).exists()

    @pytest.mark.parametrize("key_type", ("rsa-2048",))
    def test_dumpinfo_encrypted_padded_invalid(self, tmp_path_persistent, key_type):
        self.image_signed = signed_images_dir + "/pad_2.3K_sig_enc_16_inv.bin"
        result = self.runner.invoke(
            imgtool, ["dumpinfo", self.image_signed]
        )
        assert result.exit_code == 0
        assert DUMPINFO_SUCCESS_LITERAL in result.stdout
        assert "Warning: the trailer magic value is invalid!" in result.stdout

    @pytest.mark.parametrize("key_type", ("rsa-2048",))
    def test_dumpinfo_padded_multiflag(self, tmp_path_persistent, key_type):
        encoding = "pem"
        enckey = tmp_name(tmp_path_persistent, key_type, ".key")
        pub_key = tmp_name(tmp_path_persistent, key_type, ".pub." + encoding)

        self.runner.invoke(
            imgtool, ["keygen", "--key", str(enckey), "--type", key_type]
        )

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

        result = self.runner.invoke(
            imgtool,
            [
                "sign",
                "--key",
                self.key,
                "--encrypt",
                pub_key,
                "--align",
                "16",
                "--version",
                "1.0.0",
                "--header-size",
                "0x400",
                "--slot-size",
                "0x2300",
                "--pad-header",
                "--pad",
                "--rom-fixed",
                "32",
                str(self.image),
                str(self.image_signed),
            ],
        )
        assert result.exit_code == 0

        result = self.runner.invoke(
            imgtool, ["dumpinfo", str(self.image_signed)]
        )
        assert result.exit_code == 0
        assert "ENCRYPTED_AES128" in result.stdout
        assert "ROM_FIXED" in result.stdout
        assert DUMPINFO_SUCCESS_LITERAL in result.stdout
        assert DUMPINFO_ENCRYPTED_TRAILER in result.stdout

    @pytest.mark.parametrize("hex_addr", ("0", "16", "35"))
    def test_dumpinfo_hex(self, tmp_path_persistent, hex_addr):
        self.image_signed = signed_images_dir + "/hex/" + f"zero_hex-addr_{hex_addr}" + ".hex"
        result = self.runner.invoke(
            imgtool, ["dumpinfo", str(self.image_signed)]
        )
        assert result.exit_code == 0
        assert DUMPINFO_SUCCESS_LITERAL in result.stdout
