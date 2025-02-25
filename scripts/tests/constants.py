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

from imgtool import main as imgtool_main

# all supported key types for 'keygen'
KEY_TYPES = [*imgtool_main.keygens]
KEY_ENCODINGS = [*imgtool_main.valid_encodings]
KEY_LANGS = [*imgtool_main.valid_langs]
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
PUB_KEY_EXT = ".pub"
PUB_KEY_HASH_EXT = ".pubhash"

images_dir = "./tests/assets/images"
signed_images_dir = images_dir + "/signed"
keys_dir = "./tests/assets/keys/"

def tmp_name(tmp_path, key_type, suffix=""):
    return tmp_path / (key_type + suffix)
