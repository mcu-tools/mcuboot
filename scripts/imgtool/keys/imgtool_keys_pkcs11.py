"""
PKCS11 key management
"""
# SPDX-License-Identifier: Apache-2.0

import hashlib
import os
import pkcs11
import pkcs11.util.ec

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.serialization import (
    load_der_public_key,
    Encoding,
    PublicFormat
)
from cryptography.hazmat.primitives.asymmetric.ec import (
    ECDSA, SECP256R1, SECP384R1,
    EllipticCurvePublicKey
)
from urllib.parse import unquote, urlparse

from .general import KeyClass


def unquote_to_bytes(urlencoded_string):
    """Replace %xx escapes by their single-character equivalent,
    using the “iso-8859-1” encoding to decode all 8-bit values.
    """
    return bytes(
        unquote(urlencoded_string, encoding='iso-8859-1'),
        encoding='iso-8859-1'
    )

def get_pkcs11_uri_params(uri):
    """Return a dict of decoded URI key=val pairs
    """
    uri_tokens = urlparse(uri)
    assert uri_tokens.scheme == 'pkcs11'
    assert uri_tokens.query == ''
    assert uri_tokens.fragment == ''
    return {
        unquote_to_bytes(key): unquote_to_bytes(value)
        for key, value
        in [
            line.split('=')
            for line
            in uri_tokens.path.split(';')
        ]
    }

class PKCS11UsageError(Exception):
    pass


class PKCS11(KeyClass):
    """
    Wrapper around an ECDSA P384 key accessed via PKCS#11 URIs
    """
    def __init__(self, uri, env=None):
        if env is None:
            env = os.environ
        if 'PKCS11_PIN' not in env:
            raise RuntimeError("Environment variable PKCS11_PIN not set. Set it to the user PIN.")
        params = get_pkcs11_uri_params(uri)
        assert b'serial' in params
        assert b'id' in params or b'label' in params
        self.user_pin = env['PKCS11_PIN']

        # Fall back to OpenSC
        pkcs11_module_path = env.get('PKCS11_MODULE', 'opensc-pkcs11.so')

        try:
            lib = pkcs11.lib(pkcs11_module_path)
        except RuntimeError:
            raise RuntimeError(f"PKCS11 module {pkcs11_module_path} not loaded.")

        self.token = lib.get_token(token_serial=params[b'serial'])
        # try to open a session to see if the PIN is valid
        with self.token.open(user_pin=self.user_pin) as _:
            pass
        self.key_id = params.get(b'id', None)
        self.key_label = params.get(b'label', None)
        self.key_label = self.key_label.decode('utf-8') if self.key_label else None

    def shortname(self):
        return "ecdsa"

    def _unsupported(self, name):
        raise PKCS11UsageError(f"Operation {name} requires private key")

    def get_public_bytes(self):
        with self.token.open(user_pin=self.user_pin) as session:
            pub = session.get_key(
                id=self.key_id,
                label=self.key_label,
                key_type=pkcs11.KeyType.EC,
                object_class=pkcs11.ObjectClass.PUBLIC_KEY
            )
            key = pkcs11.util.ec.encode_ec_public_key(pub)
        return key

    def get_private_bytes(self, minimal):
        self._unsupported('get_private_bytes')

    def export_private(self, path, passwd=None):
        self._unsupported('export_private')

    def export_public(self, path):
        """Write the public key to the given file."""
        with self.token.open(user_pin=self.user_pin) as session:
            pub = session.get_key(
                id=self.key_id,
                label=self.key_label,
                key_type=pkcs11.KeyType.EC,
                object_class=pkcs11.ObjectClass.PUBLIC_KEY
            )
            # Encode to DER
            der_bytes = pkcs11.util.ec.encode_ec_public_key(pub)

            # Convert to PEM using cryptography
            public_key = load_der_public_key(der_bytes)
            pem = public_key.public_bytes(
                encoding=Encoding.PEM,
                format=PublicFormat.SubjectPublicKeyInfo
            )

        with open(path, 'wb') as f:
            f.write(pem)

    def sig_type(self):
        return "ECDSA384_SHA384"

    def sig_tlv(self):
        return "ECDSASIG"

    def sig_len(self):
        # Early versions of MCUboot (< v1.5.0) required ECDSA
        # signatures to be padded to a fixed length. Because the DER
        # encoding is done with signed integers, the size of the
        # signature will vary depending on whether the high bit is set
        # in each value. This padding was done in a
        # not-easily-reversible way (by just adding zeros).
        #
        # The signing code no longer requires this padding, and newer
        # versions of MCUboot don't require it. But, continue to
        # return the total length so that the padding can be done if
        # requested.
        return 103

    def raw_sign(self, payload):
        """Return the actual signature"""
        with self.token.open(user_pin=self.user_pin) as session:
            priv = session.get_key(
                id=self.key_id,
                label=self.key_label,
                key_type=pkcs11.KeyType.EC,
                object_class=pkcs11.ObjectClass.PRIVATE_KEY
            )
            sig = priv.sign(
                hashlib.sha384(payload).digest(),
                mechanism=pkcs11.Mechanism.ECDSA
            )
        return pkcs11.util.ec.encode_ecdsa_signature(sig)

    def sign(self, payload):
        """Return signature with legacy padding"""
        # To make fixed length, pad with one or two zeros.
        while True:
            sig = self.raw_sign(payload)
            if sig[-1] != 0x00:
                break

        sig += b'\000' * (self.sig_len() - len(sig))
        return sig

    def verify(self, signature, payload):
        """Verify the signature of the payload"""
        # strip possible paddings added during sign
        signature = signature[:signature[1] + 2]

        # Load public key from DER bytes
        public_key = load_der_public_key(self.get_public_bytes())

        if not isinstance(public_key, EllipticCurvePublicKey):
            raise TypeError(f"Unsupported key type: {type(public_key).__name__}")

        # Determine correct hash algorithm based on curve
        if isinstance(public_key.curve, SECP256R1):
            hash_alg = hashes.SHA256()
        elif isinstance(public_key.curve, SECP384R1):
            hash_alg = hashes.SHA384()
        else:
            raise ValueError(f"Unsupported curve: {public_key.curve.name}")

        try:
            # Attempt ECDSA verification
            public_key.verify(signature, payload, ECDSA(hash_alg))
            return True
        except InvalidSignature:
            return False
