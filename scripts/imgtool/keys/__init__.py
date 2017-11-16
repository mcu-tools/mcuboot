# Copyright 2017 Linaro Limited
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

"""
Cryptographic key management for imgtool.
"""

from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.rsa import RSAPrivateKey, RSAPublicKey

from .rsa import RSA2048, RSA2048Public, RSAUsageError

class ECDSA256P1():
    def __init__(self, key):
        """Construct an ECDSA P-256 private key"""
        self.key = key

    @staticmethod
    def generate():
        return ECDSA256P1(SigningKey.generate(curve=NIST256p))

    def export_private(self, path):
        with open(path, 'wb') as f:
            f.write(self.key.to_pem())

    def get_public_bytes(self):
        vk = self.key.get_verifying_key()
        return bytes(vk.to_der())

    def emit_c(self):
        vk = self.key.get_verifying_key()
        print(AUTOGEN_MESSAGE)
        print("const unsigned char ecdsa_pub_key[] = {", end='')
        encoded = bytes(vk.to_der())
        for count, b in enumerate(encoded):
            if count % 8 == 0:
                print("\n\t", end='')
            else:
                print(" ", end='')
            print("0x{:02x},".format(b), end='')
        print("\n};")
        print("const unsigned int ecdsa_pub_key_len = {};".format(len(encoded)))

    def emit_rust(self):
        vk = self.key.get_verifying_key()
        print(AUTOGEN_MESSAGE)
        print("static ECDSA_PUB_KEY: &'static [u8] = &[", end='')
        encoded = bytes(vk.to_der())
        for count, b in enumerate(encoded):
            if count % 8 == 0:
                print("\n    ", end='')
            else:
                print(" ", end='')
            print("0x{:02x},".format(b), end='')
        print("\n];")

    def sign(self, payload):
        # To make this fixed length, possibly pad with zeros.
        sig = self.key.sign(payload, hashfunc=hashlib.sha256, sigencode=util.sigencode_der)
        sig += b'\000' * (self.sig_len() - len(sig))
        return sig

    def sig_len(self):
        # The DER encoding depends on the high bit, and can be
        # anywhere from 70 to 72 bytes.  Because we have to fill in
        # the length field before computing the signature, however,
        # we'll give the largest, and the sig checking code will allow
        # for it to be up to two bytes larger than the actual
        # signature.
        return 72

    def sig_type(self):
        """Return the type of this signature (as a string)"""
        return "ECDSA256_SHA256"

    def sig_tlv(self):
        return "ECDSA256"

class PasswordRequired(Exception):
    """Raised to indicate that the key is password protected, but a
    password was not specified."""
    pass

def load(path, passwd=None):
    """Try loading a key from the given path.  Returns None if the password wasn't specified."""
    with open(path, 'rb') as f:
        raw_pem = f.read()
    try:
        pk = serialization.load_pem_private_key(
                raw_pem,
                password=passwd,
                backend=default_backend())
    # This is a bit nonsensical of an exception, but it is what
    # cryptography seems to currently raise if the password is needed.
    except TypeError:
        return None
    except ValueError:
        # This seems to happen if the key is a public key, let's try
        # loading it as a public key.
        pk = serialization.load_pem_public_key(
                raw_pem,
                backend=default_backend())

    if isinstance(pk, RSAPrivateKey):
        if pk.key_size != 2048:
            raise Exception("Unsupported RSA key size: " + pk.key_size)
        return RSA2048(pk)
    elif isinstance(pk, RSAPublicKey):
        if pk.key_size != 2048:
            raise Exception("Unsupported RSA key size: " + pk.key_size)
        return RSA2048Public(pk)
    else:
        raise Exception("Unknown key type: " + str(type(pk)))
