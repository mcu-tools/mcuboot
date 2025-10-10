"""
X25519 key management
"""

# SPDX-License-Identifier: Apache-2.0

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import x25519

from .general import KeyClass
from .privatebytes import PrivateBytesMixin


class X25519UsageError(Exception):
    pass


class X25519Public(KeyClass):
    def __init__(self, key):
        self.key = key

    def shortname(self):
        return "x25519"

    def _unsupported(self, name):
        raise X25519UsageError(f"Operation {name} requires private key")

    def _get_public(self):
        return self.key

    def get_public_bytes(self):
        # The key is embedded into MBUboot in "SubjectPublicKeyInfo" format
        return self._get_public().public_bytes(
                encoding=serialization.Encoding.DER,
                format=serialization.PublicFormat.SubjectPublicKeyInfo)

    def get_public_pem(self):
        return self._get_public().public_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PublicFormat.SubjectPublicKeyInfo)

    def get_private_bytes(self, minimal, format):
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
        return "X25519"

    def sig_tlv(self):
        return "X25519"

    def sig_len(self):
        return 32


class X25519(X25519Public, PrivateBytesMixin):
    """
    Wrapper around an X25519 private key.
    """

    def __init__(self, key):
        """key should be an instance of EllipticCurvePrivateKey"""
        self.key = key

    @staticmethod
    def generate():
        pk = x25519.X25519PrivateKey.generate()
        return X25519(pk)

    def _get_public(self):
        return self.key.public_key()

    _VALID_FORMATS = {
        'pkcs8': serialization.PrivateFormat.PKCS8
    }
    _DEFAULT_FORMAT = 'pkcs8'

    def get_private_bytes(self, minimal, format):
        _, priv = self._get_private_bytes(minimal, format,
                                          X25519UsageError)
        return priv

    def export_private(self, path, passwd=None):
        """
        Write the private key to the given file, protecting it with the
        optional password.
        """
        if passwd is None:
            enc = serialization.NoEncryption()
        else:
            enc = serialization.BestAvailableEncryption(passwd)
        pem = self.key.private_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PrivateFormat.PKCS8,
                encryption_algorithm=enc)
        with open(path, 'wb') as f:
            f.write(pem)

    def sign_digest(self, digest):
        """Return the actual signature"""
        return self.key.sign(data=digest)

    def verify_digest(self, signature, digest):
        """Verify that signature is valid for given digest"""
        k = self.key
        if isinstance(self.key, x25519.X25519PrivateKey):
            k = self.key.public_key()
        return k.verify(signature=signature, data=digest)
