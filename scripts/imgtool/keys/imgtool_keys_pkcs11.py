"""
PKCS11 key management
"""

import hashlib
import os
import oscrypto.asymmetric
import pkcs11
import pkcs11.util.ec

from .general import KeyClass


class PKCS11UsageError(Exception):
    pass


class PKCS11(KeyClass):
    def __init__(self, env=os.environ):
        lib = pkcs11.lib(env['PKCS11_MODULE'])
        self.token = lib.get_token(token_label=env['PKCS11_TOKEN_LABEL'])
        self.key_id = bytes.fromhex(env['PKCS11_KEY_ID'])
        self.session = self.token.open(user_pin=env['PKCS11_PIN'])

    def __del__(self):
        if hasattr(self, 'session'):
            self.session.close()

    def shortname(self):
        return "ecdsa"

    def _unsupported(self, name):
        raise PKCS11UsageError("Operation {} requires private key".format(name))

    def get_public_bytes(self):
        pub = self.session.get_key(
            id=self.key_id, key_type=pkcs11.KeyType.EC, object_class=pkcs11.ObjectClass.PUBLIC_KEY)
        key = oscrypto.asymmetric.load_public_key(
            pkcs11.util.ec.encode_ec_public_key(pub))
        key = pkcs11.util.ec.encode_ec_public_key(pub)
        return key

    def get_private_bytes(self, minimal):
        self._unsupported('get_private_bytes')

    def export_private(self, path, passwd=None):
        self._unsupported('export_private')

    def export_public(self, path):
        """Write the public key to the given file."""
        pub = self.session.get_key(
            id=self.key_id, key_type=pkcs11.KeyType.EC, object_class=pkcs11.ObjectClass.PUBLIC_KEY)
        key = oscrypto.asymmetric.load_public_key(
            pkcs11.util.ec.encode_ec_public_key(pub))
        pem = oscrypto.asymmetric.dump_public_key(key, encoding='pem')

        with open(path, 'wb') as f:
            f.write(pem)

    def sig_type(self):
        return "ECDSA256_SHA256"

    def sig_tlv(self):
        return "ECDSA256"

    def sig_len(self):
        # The DER encoding depends on the high bit, and can be
        # anywhere from 70 to 72 bytes.  Because we have to fill in
        # the length field before computing the signature, however,
        # we'll give the largest, and the sig checking code will allow
        # for it to be up to two bytes larger than the actual
        # signature.
        return 72

    def raw_sign(self, payload):
        """Return the actual signature"""
        priv = self.session.get_key(
            id=self.key_id, key_type=pkcs11.KeyType.EC, object_class=pkcs11.ObjectClass.PRIVATE_KEY)
        sig = priv.sign(hashlib.sha256(payload).digest(),
                        mechanism=pkcs11.Mechanism.ECDSA)
        return pkcs11.util.ec.encode_ecdsa_signature(sig)

    def sign(self, payload):
        # To make fixed length, pad with one or two zeros.
        while True:
            sig = self.raw_sign(payload)
            if sig[-1] != 0x00:
                break

        sig += b'\000' * (self.sig_len() - len(sig))
        return sig

    def verify(self, signature, payload):
        # strip possible paddings added during sign
        signature = signature[:signature[1] + 2]

        k = oscrypto.asymmetric.load_public_key(self.get_public_bytes())
        return oscrypto.asymmetric.ecdsa_verify(k, signature, payload, 'sha256')
