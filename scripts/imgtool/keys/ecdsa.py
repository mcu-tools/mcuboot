"""
ECDSA key management
"""

# SPDX-License-Identifier: Apache-2.0
import os.path
import hashlib

from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.hashes import SHA256, SHA384

from .general import KeyClass
from .privatebytes import PrivateBytesMixin


class ECDSAUsageError(Exception):
    pass


class ECDSAPublicKey(KeyClass):
    """
    Wrapper around an ECDSA public key.
    """
    def __init__(self, key):
        self.key = key

    def _unsupported(self, name):
        raise ECDSAUsageError("Operation {} requires private key".format(name))

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


class ECDSAPrivateKey(PrivateBytesMixin):
    """
    Wrapper around an ECDSA private key.
    """
    def __init__(self, key):
        self.key = key

    def _get_public(self):
        return self.key.public_key()

    def _build_minimal_ecdsa_privkey(self, der, format):
        '''
        Builds a new DER that only includes the EC private key, removing the
        public key that is added as an "optional" BITSTRING.
        '''

        if format == serialization.PrivateFormat.OpenSSH:
            print(os.path.basename(__file__) +
                  ': Warning: --minimal is supported only for PKCS8 '
                  'or TraditionalOpenSSL formats')
            return bytearray(der)

        EXCEPTION_TEXT = "Error parsing ecdsa key. Please submit an issue!"
        if format == serialization.PrivateFormat.PKCS8:
            offset_PUB = 68  # where the context specific TLV starts (tag 0xA1)
            if der[offset_PUB] != 0xa1:
                raise ECDSAUsageError(EXCEPTION_TEXT)
            len_PUB = der[offset_PUB + 1] + 2  # + 2 for 0xA1 0x44 bytes
            b = bytearray(der[:offset_PUB])  # remove the TLV with the PUB key
            offset_SEQ = 29
            if b[offset_SEQ] != 0x30:
                raise ECDSAUsageError(EXCEPTION_TEXT)
            b[offset_SEQ + 1] -= len_PUB
            offset_OCT_STR = 27
            if b[offset_OCT_STR] != 0x04:
                raise ECDSAUsageError(EXCEPTION_TEXT)
            b[offset_OCT_STR + 1] -= len_PUB
            if b[0] != 0x30 or b[1] != 0x81:
                raise ECDSAUsageError(EXCEPTION_TEXT)
            # as b[1] has bit7 set, the length is on b[2]
            b[2] -= len_PUB
            if b[2] < 0x80:
                del(b[1])

        elif format == serialization.PrivateFormat.TraditionalOpenSSL:
            offset_PUB = 51
            if der[offset_PUB] != 0xA1:
                raise ECDSAUsageError(EXCEPTION_TEXT)
            len_PUB = der[offset_PUB + 1] + 2
            b = bytearray(der[0:offset_PUB])
            b[1] -= len_PUB

        return b

    _VALID_FORMATS = {
        'pkcs8': serialization.PrivateFormat.PKCS8,
        'openssl': serialization.PrivateFormat.TraditionalOpenSSL
    }
    _DEFAULT_FORMAT = 'pkcs8'

    def get_private_bytes(self, minimal, format):
        format, priv = self._get_private_bytes(minimal,
                                               format, ECDSAUsageError)
        if minimal:
            priv = self._build_minimal_ecdsa_privkey(
                priv, self._VALID_FORMATS[format])
        return priv

    def export_private(self, path, passwd=None):
        """Write the private key to the given file, protecting it with '
          'the optional password."""
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


class ECDSA256P1Public(ECDSAPublicKey):
    """
    Wrapper around an ECDSA (p256) public key.
    """
    def __init__(self, key):
        super().__init__(key)
        self.key = key

    def shortname(self):
        return "ecdsa"

    def sig_type(self):
        return "ECDSA256_SHA256"

    def sig_tlv(self):
        return "ECDSASIG"

    def sig_len(self):
        # Early versions of MCUboot (< v1.5.0) required ECDSA
        # signatures to be padded to 72 bytes.  Because the DER
        # encoding is done with signed integers, the size of the
        # signature will vary depending on whether the high bit is set
        # in each value.  This padding was done in a
        # not-easily-reversible way (by just adding zeros).
        #
        # The signing code no longer requires this padding, and newer
        # versions of MCUboot don't require it.  But, continue to
        # return the total length so that the padding can be done if
        # requested.
        return 72

    def verify(self, signature, payload):
        # strip possible paddings added during sign
        signature = signature[:signature[1] + 2]
        k = self.key
        if isinstance(self.key, ec.EllipticCurvePrivateKey):
            k = self.key.public_key()
        return k.verify(signature=signature, data=payload,
                        signature_algorithm=ec.ECDSA(SHA256()))


class ECDSA256P1(ECDSAPrivateKey, ECDSA256P1Public):
    """
    Wrapper around an ECDSA (p256) private key.
    """
    def __init__(self, key):
        super().__init__(key)
        self.key = key
        self.pad_sig = False

    @staticmethod
    def generate():
        pk = ec.generate_private_key(
                ec.SECP256R1(),
                backend=default_backend())
        return ECDSA256P1(pk)

    def raw_sign(self, payload):
        """Return the actual signature"""
        return self.key.sign(
                data=payload,
                signature_algorithm=ec.ECDSA(SHA256()))

    def sign(self, payload):
        sig = self.raw_sign(payload)
        if self.pad_sig:
            # To make fixed length, pad with one or two zeros.
            sig += b'\000' * (self.sig_len() - len(sig))
            return sig
        else:
            return sig


class ECDSA384P1Public(ECDSAPublicKey):
    """
    Wrapper around an ECDSA (p384) public key.
    """
    def __init__(self, key):
        super().__init__(key)
        self.key = key

    def shortname(self):
        return "ecdsap384"

    def sig_type(self):
        return "ECDSA384_SHA384"

    def sig_tlv(self):
        return "ECDSASIG"

    def sig_len(self):
        # Early versions of MCUboot (< v1.5.0) required ECDSA
        # signatures to be padded to a fixed length.  Because the DER
        # encoding is done with signed integers, the size of the
        # signature will vary depending on whether the high bit is set
        # in each value.  This padding was done in a
        # not-easily-reversible way (by just adding zeros).
        #
        # The signing code no longer requires this padding, and newer
        # versions of MCUboot don't require it.  But, continue to
        # return the total length so that the padding can be done if
        # requested.
        return 103

    def verify(self, signature, payload):
        # strip possible paddings added during sign
        signature = signature[:signature[1] + 2]
        k = self.key
        if isinstance(self.key, ec.EllipticCurvePrivateKey):
            k = self.key.public_key()
        return k.verify(signature=signature, data=payload,
                        signature_algorithm=ec.ECDSA(SHA384()))


class ECDSA384P1(ECDSAPrivateKey, ECDSA384P1Public):
    """
    Wrapper around an ECDSA (p384) private key.
    """

    def __init__(self, key):
        """key should be an instance of EllipticCurvePrivateKey"""
        super().__init__(key)
        self.key = key
        self.pad_sig = False

    @staticmethod
    def generate():
        pk = ec.generate_private_key(
                ec.SECP384R1(),
                backend=default_backend())
        return ECDSA384P1(pk)

    def raw_sign(self, payload):
        """Return the actual signature"""
        return self.key.sign(
                data=payload,
                signature_algorithm=ec.ECDSA(SHA384()))

    def sign(self, payload):
        sig = self.raw_sign(payload)
        if self.pad_sig:
            # To make fixed length, pad with one or two zeros.
            sig += b'\000' * (self.sig_len() - len(sig))
            return sig
        else:
            return sig
