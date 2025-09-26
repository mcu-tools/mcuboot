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

import pytest
from click.testing import CliRunner

from imgtool.main import imgtool
from tests.constants import KEY_TYPES, GEN_KEY_EXT, tmp_name, signed_images_dir, keys_dir

KEY_TYPE_MISMATCH_TLV = "Key type does not match TLV record"
NO_SIG_FOR_KEY = "No signature found for the given key"

try:
    KEY_TYPES.remove("x25519")  # x25519 is not used for signing, so directory does not contain any such image
except ValueError:
    pass


def assert_valid(result):
    assert result.exit_code == 0
    assert "Image was correctly validated" in result.stdout


class TestVerify:
    image = None
    image_signed = None
    runner = CliRunner()

    @pytest.fixture(scope="session")
    def tmp_path_persistent(self, tmp_path_factory):
        return tmp_path_factory.mktemp("keys")


class TestVerifyBasic(TestVerify):
    key = None
    gen_key = None
    test_signed_images_dir = signed_images_dir + "/basic/"

    @pytest.fixture(autouse=True)
    def setup(self, request, tmp_path_persistent, key_type):
        self.key = keys_dir + key_type + ".key"
        self.gen_key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
        self.runner.invoke(
            imgtool, ["keygen", "--key", str(self.gen_key), "--type", key_type]
        )

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_verify_basic(self, key_type, tmp_path_persistent):
        """Test verify basic image"""

        self.image_signed = self.test_signed_images_dir + key_type + ".bin"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                "--key",
                str(self.key),
                str(self.image_signed),
            ],
        )
        assert_valid(result)

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_verify_wrong_key(self, key_type, tmp_path_persistent):
        """Test verify basic image with wrong key"""

        self.image_signed = self.test_signed_images_dir + key_type + ".bin"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                "--key",
                str(self.gen_key),
                self.image_signed,
            ],
        )
        assert result.exit_code != 0
        assert NO_SIG_FOR_KEY in result.stdout

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_verify_key_not_exists(self, key_type, tmp_path_persistent):
        """Test verify basic image with non-existent key"""

        inv_key = "./invalidPath"
        self.image_signed = self.test_signed_images_dir + key_type + ".bin"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                "--key",
                str(inv_key),
                self.image_signed,
            ],
        )
        assert result.exit_code != 0
        assert "Key file not found" in result.stdout

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_verify_image_not_exists(self, key_type, tmp_path_persistent):
        """Test verify basic image with non-existent image"""

        self.image_signed = "./invalid"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                "--key",
                self.key,
                self.image_signed,
            ],
        )
        assert result.exit_code != 0
        assert "Image file not found" in result.stdout

    @pytest.mark.parametrize("key_type", ("rsa-3072",))
    def test_verify_key_inv_magic(self, key_type, tmp_path_persistent):
        """Test verify basic image with invalid magic"""

        self.image_signed = self.test_signed_images_dir + key_type + "_invMagic.bin"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                "--key",
                str(self.key),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert "Invalid image magic" in result.stdout

    @pytest.mark.parametrize("key_type", ("rsa-3072",))
    def test_verify_key_inv_tlv_magic(self, key_type, tmp_path_persistent):
        """Test verify basic image with invalid tlv magic"""

        self.image_signed = self.test_signed_images_dir + key_type + "_invTLVMagic.bin"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                "--key",
                str(self.key),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert "Invalid TLV info magic" in result.stdout

    @pytest.mark.parametrize("key_type", ("rsa-3072",))
    def test_verify_key_inv_hash(self, key_type, tmp_path_persistent):
        """ Test verify basic image with invalid hash"""

        self.image_signed = self.test_signed_images_dir + key_type + "_invHash.bin"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                "--key",
                str(self.key),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert "Image has an invalid hash" in result.stdout


class TestVerifyEncryptedClear(TestVerify):
    key = None
    gen_key = None
    test_signed_images_dir = signed_images_dir + "/encryptedClear/"

    @pytest.fixture(autouse=True)
    def setup(self, request, tmp_path_persistent, key_type):
        self.key = keys_dir + key_type + ".key"
        self.gen_key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
        self.runner.invoke(
            imgtool, ["keygen", "--key", str(self.gen_key), "--type", key_type]
        )

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_verify_encrypted_clear(self, key_type, tmp_path_persistent):
        """Test verify encrypted imaged with clear option"""

        self.image_signed = self.test_signed_images_dir + key_type + ".enc"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                "--key",
                str(self.key),
                str(self.image_signed),
            ],
        )
        assert_valid(result)

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_verify_encrypted_clear_wrong_key(self, key_type, tmp_path_persistent):
        """Test verify with wrong key should fail"""

        self.image_signed = self.test_signed_images_dir + key_type + ".enc"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                "--key",
                str(self.gen_key),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert NO_SIG_FOR_KEY in result.stdout


class TestVerifyCustomTLV(TestVerify):
    key = None
    gen_key = None
    test_signed_images_dir = signed_images_dir + "/customTLV/"

    @pytest.fixture(autouse=True)
    def setup(self, request, tmp_path_persistent, key_type):
        self.key = keys_dir + key_type + ".key"
        self.gen_key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
        self.runner.invoke(
            imgtool, ["keygen", "--key", str(self.gen_key), "--type", key_type]
        )

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_verify_custom_tlv(self, key_type, tmp_path_persistent):
        """Test verify image with customTLV"""

        self.image_signed = self.test_signed_images_dir + key_type + ".bin"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                "--key",
                str(self.key),
                str(self.image_signed),
            ],
        )
        assert_valid(result)

    @pytest.mark.parametrize("key_type", KEY_TYPES)
    def test_verify_custom_tlv_no_key(self, key_type, tmp_path_persistent):
        """Test verify image with customTLV"""

        self.image_signed = self.test_signed_images_dir + key_type + ".bin"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                str(self.image_signed),
            ],
        )
        assert_valid(result)


class TestVerifyNoKey(TestVerify):
    """Tests for images that are signed without --key option """
    key = None
    test_signed_images_dir = signed_images_dir + "/noKey/"

    def test_verify_no_key(self):
        """Test verify image signed without key """

        self.image_signed = self.test_signed_images_dir + "noKey256" + ".bin"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                str(self.image_signed),
            ],
        )
        assert_valid(result)

    def test_verify_no_key_image_with_key(self):
        """Test verify image signed without key, attempt to verify with a key should fail on signature check"""

        self.key = keys_dir + "rsa-2048" + ".key"
        self.image_signed = self.test_signed_images_dir + "noKey256" + ".bin"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                "--key",
                str(self.key),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert NO_SIG_FOR_KEY in result.stdout

    def test_verify_no_key_image_with_wrong_key(self):
        """Test verify image signed without key, attempt to verify with wrong key should fail on hash check"""

        self.key = keys_dir + "ecdsa-p384" + ".key"
        self.image_signed = self.test_signed_images_dir + "noKey256" + ".bin"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                "--key",
                str(self.key),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert KEY_TYPE_MISMATCH_TLV in result.stdout


class TestVerifyPubKey(TestVerify):
    """Tests for images that are signed without --key option and with --fix-sig-pubkey"""
    key = None
    gen_key = None
    test_signed_images_dir = signed_images_dir + "/pubKeyOnly/"

    @pytest.fixture(autouse=True)
    def setup(self, request, tmp_path_persistent, key_type):
        self.key = keys_dir + key_type + ".key"
        self.gen_key = tmp_name(tmp_path_persistent, key_type, GEN_KEY_EXT)
        self.runner.invoke(
            imgtool, ["keygen", "--key", str(self.gen_key), "--type", key_type]
        )

    @pytest.mark.parametrize("key_type", KEY_TYPES[:-1])
    def test_verify_no_key(self, key_type):
        """Test verify image signed with --fix-sig, verify without key """

        self.image_signed = self.test_signed_images_dir + key_type + ".bin"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                str(self.image_signed),
            ],
        )
        assert_valid(result)

    @pytest.mark.parametrize("key_type", ("ecdsa-p384",))
    def test_verify_384_key(self, key_type):
        """Test verify image without key """

        self.image_signed = self.test_signed_images_dir + key_type + ".bin"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                str(self.image_signed),
            ],
        )
        assert_valid(result)

    @pytest.mark.parametrize("key_type", KEY_TYPES[:-2])
    def test_verify_key_not_matching(self, key_type, tmp_path_persistent):
        """Test verify image with mismatching key """

        self.key = keys_dir + "ecdsa-p384" + ".key"
        self.image_signed = self.test_signed_images_dir + key_type + ".bin"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                "--key",
                str(self.key),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert KEY_TYPE_MISMATCH_TLV in result.stdout

    @pytest.mark.parametrize("key_type", ("ecdsa-p256",))
    def test_verify_key_not_matching_384(self, key_type, tmp_path_persistent):
        """Test verify image with mismatching key """

        self.image_signed = self.test_signed_images_dir + "ecdsa-p384" + ".bin"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                "--key",
                str(self.key),
                str(self.image_signed),
            ],
        )
        assert result.exit_code != 0
        assert KEY_TYPE_MISMATCH_TLV in result.stdout


class TestVerifyHex(TestVerify):
    key = None
    test_signed_images_dir = signed_images_dir + "/hex/"

    @pytest.fixture(autouse=True)
    def setup(self, request, tmp_path_persistent, key_type="rsa-2048"):
        self.key = keys_dir + key_type + ".key"

    @pytest.mark.parametrize("hex_addr", ("0", "16", "35"))
    def test_verify_basic(self, hex_addr, tmp_path_persistent):
        """Test verify basic image"""

        self.image_signed = self.test_signed_images_dir + f"zero_hex-addr_{hex_addr}" + ".hex"

        result = self.runner.invoke(
            imgtool,
            [
                "verify",
                "--key",
                str(self.key),
                str(self.image_signed),
            ],
        )
        assert_valid(result)
