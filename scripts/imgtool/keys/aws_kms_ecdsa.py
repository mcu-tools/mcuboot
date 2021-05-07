"""
RSA Key management
"""

# SPDX-License-Identifier: Apache-2.0

from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import serialization

from .ecdsa import ECDSA256P1Public

from .general import KeyClass


class AwsKmsECDSA256UsageError(Exception):
    pass


class AwsKmsECDSA256(KeyClass):
    def __init__(self, client, key_info, public_key_raw):
        self.client = client
        self.key_info = key_info

        self.key = serialization.load_der_public_key(
            public_key_raw['PublicKey'],
            backend=default_backend())
        self.public_key = ECDSA256P1Public(self.key)

        self.pad_sig = False

    def shortname(self):
        return self.public_key.shortname()

    def _unsupported(self, name):
        raise AwsKmsECDSA256UsageError("Operation {} requires private key".format(name))

    def _get_public(self):
        return self.key

    def get_public_bytes(self):
        # The key is embedded into MBUboot in "SubjectPublicKeyInfo" format
        return self._get_public().public_bytes(
                encoding=serialization.Encoding.DER,
                format=serialization.PublicFormat.SubjectPublicKeyInfo)

    def get_private_bytes(self, minimal):
        self._unsupported('get_private_bytes')

    def export_private(self, path, passwd=None):
        self._unsupported('export_private')

    def export_public(self, path):
        """Write the public key to the given file."""
        pem = self._get_public().public_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PublicFormat.SubjectPublicKeyInfo)
        with open(path, 'wb') as f:
            f.write(pem)

    def sig_type(self):
        return self.public_key.sig_type()

    def sig_tlv(self):
        return self.public_key.sig_tlv()

    def sig_len(self):
        return self.public_key.sig_len()

    def sign_digest(self, digest):
        """Return the actual signature"""
        sig = self.client.sign(KeyId=self.key_info['KeyMetadata']['Arn'], Message=digest, MessageType='DIGEST', SigningAlgorithm='ECDSA_SHA_256')['Signature']
        if self.pad_sig:
            # To make fixed length, pad with one or two zeros.
            sig += b'\000' * (self.sig_len() - len(sig))
            return sig
        else:
            return sig

    def verify(self, signature, payload):
        """Verify that signature is valid for given digest"""
        return self.public_key.verify(signature=signature, payload=payload)
