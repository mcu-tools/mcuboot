# Copyright 2017 Linaro Limited
# Copyright 2023 Arm Limited
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

"""
Cryptographic key management for imgtool.
"""

import base64

from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ec import (
    EllipticCurvePrivateKey,
    EllipticCurvePublicKey,
)
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey
from cryptography.hazmat.primitives.asymmetric.rsa import RSAPrivateKey, RSAPublicKey
from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey, X25519PublicKey

from .ecdsa import ECDSA256P1, ECDSA384P1, ECDSA256P1Public, ECDSA384P1Public, ECDSAUsageError
from .ed25519 import Ed25519, Ed25519Public, Ed25519UsageError
from .rsa import RSA, RSA_KEY_SIZES, RSAPublic, RSAUsageError
from .x25519 import X25519, X25519Public, X25519UsageError

__all__ = [
    "ECDSA256P1",
    "ECDSA384P1",
    "ECDSA256P1Public",
    "ECDSA384P1Public",
    "ECDSAUsageError",
    "Ed25519",
    "Ed25519Public",
    "Ed25519UsageError",
    "RSA",
    "RSA_KEY_SIZES",
    "RSAPublic",
    "RSAUsageError",
    "X25519",
    "X25519Public",
    "X25519UsageError",
    "AESKWKey",
]


class PasswordRequired(Exception):
    """Raised to indicate that the key is password protected, but a
    password was not specified."""


class AESKWKey:
    def __init__(self, kek):
        self.kek = kek


def load(path, passwd=None, allow_aes=False):
    """Try loading a key from the given path.
      Returns None if the password wasn't specified."""
    with open(path, 'rb') as f:
        raw_pem = f.read()
    load_error = None
    try:
        pk = serialization.load_pem_private_key(
                raw_pem,
                password=passwd,
                backend=default_backend())
    # Unfortunately, the crypto library raises unhelpful exceptions,
    # so we have to look at the text.
    except Exception as e:
        load_error = e
        pk = None

    if isinstance(load_error, TypeError):
        msg = str(load_error)
        if "private key is encrypted" in msg and passwd is None:
            return None

    # This seems to happen if the key is a public key, let's try
    # loading it as a public key.
    if isinstance(load_error, ValueError):
        try:
            pk = serialization.load_pem_public_key(
                    raw_pem,
                    backend=default_backend())
        except Exception as e:
            load_error = e
            pk = None
        else:
            load_error = None

    # Try base64 on malformed PEM
    kek = None
    if isinstance(load_error, ValueError) and allow_aes:
        try:
            kek = base64.b64decode(raw_pem.strip(), validate=True)
        except Exception:
            raise load_error

    if kek is not None:
        if len(kek) not in (16, 32):
            raise Exception(
                f"Invalid AES key length: {len(kek)} bytes. Expected 16 or 32 bytes after base64 decode."
            )
        return AESKWKey(kek)

    if load_error is not None:
        raise load_error

    if isinstance(pk, RSAPrivateKey):
        if pk.key_size not in RSA_KEY_SIZES:
            raise Exception("Unsupported RSA key size: " + pk.key_size)
        return RSA(pk)
    elif isinstance(pk, RSAPublicKey):
        if pk.key_size not in RSA_KEY_SIZES:
            raise Exception("Unsupported RSA key size: " + pk.key_size)
        return RSAPublic(pk)
    elif isinstance(pk, EllipticCurvePrivateKey):
        if pk.curve.name not in ('secp256r1', 'secp384r1'):
            raise Exception("Unsupported EC curve: " + pk.curve.name)
        if pk.key_size not in (256, 384):
            raise Exception("Unsupported EC size: " + pk.key_size)
        if pk.curve.name == 'secp256r1':
            return ECDSA256P1(pk)
        elif pk.curve.name == 'secp384r1':
            return ECDSA384P1(pk)
    elif isinstance(pk, EllipticCurvePublicKey):
        if pk.curve.name not in ('secp256r1', 'secp384r1'):
            raise Exception("Unsupported EC curve: " + pk.curve.name)
        if pk.key_size not in (256, 384):
            raise Exception("Unsupported EC size: " + pk.key_size)
        if pk.curve.name == 'secp256r1':
            return ECDSA256P1Public(pk)
        elif pk.curve.name == 'secp384r1':
            return ECDSA384P1Public(pk)
    elif isinstance(pk, Ed25519PrivateKey):
        return Ed25519(pk)
    elif isinstance(pk, Ed25519PublicKey):
        return Ed25519Public(pk)
    elif isinstance(pk, X25519PrivateKey):
        return X25519(pk)
    elif isinstance(pk, X25519PublicKey):
        return X25519Public(pk)
    else:
        raise Exception("Unknown key type: " + str(type(pk)))
