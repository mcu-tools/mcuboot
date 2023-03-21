"""
PKCS11 key management
"""

import hashlib
import os
import oscrypto.asymmetric
import pkcs11
import pkcs11.util.ec

from pathlib import Path
from urllib.parse import unquote, urlparse

from .general import KeyClass


def find_library(filename):
    search_directory = '/usr'
    for root, directorios, filenames in os.walk(search_directory):
        if filename in filenames:
            return os.path.join(
                root,
                filename,
            )

def unquote_to_bytes(urlencoded_string):
    """Replace %xx escapes by their single-character equivalent,
using the “iso-8859-1” encoding to decode all 8-bit values.
    """
    return bytes(
        unquote(urlencoded_string, encoding='iso-8859-1'),
        encoding='iso-8859-1'
    )

def get_pkcs11_uri_params(uri):
    uri_tokens = urlparse(uri)
    assert(uri_tokens.scheme == 'pkcs11')
    assert(uri_tokens.query == '')
    assert(uri_tokens.fragment == '')
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
    def __init__(self, uri, env=os.environ):
        params = get_pkcs11_uri_params(uri)

        if b'pin-value' in params.keys():
            self.user_pin = str(params[b'pin-value'], "utf-8")
        elif 'PKCS11_PIN' in env.keys():
            self.user_pin = env['PKCS11_PIN']
        else:
            raise RuntimeError("Environment variable PKCS11_PIN not set. Set it to the user PIN.")

        assert(b'serial' in params.keys())

        # Try to find PKCS #11 module path like this:
        # Use the environment variable $PKCS11_MODULE,
        # then fall back to OpenSC (for NitroKey HSM).
        pkcs11_module_path = ''
        try:
            pkcs11_module_path = env['PKCS11_MODULE']
            if not Path(pkcs11_module_path).is_file():
                raise RuntimeError('$PKCS11_MODULE is not a file: %s' % pkcs11_module_path)
        except KeyError:
            pkcs11_module_path = find_library('opensc-pkcs11.so')
            if pkcs11_module_path is None:
                raise RuntimeError('$PKCS11_MODULE not set and OpenSC not found.')

        lib = ''
        try:
            lib = pkcs11.lib(pkcs11_module_path)
        except RuntimeError:
            pass  # happens if lib does not exist or is corrupt
        if '' == lib:
            raise RuntimeError('PKCS #11 module not loaded.')

        self.token = lib.get_token(token_serial=params[b'serial'])
        # try to open a session to see if the PIN is valid
        with self.token.open(user_pin=self.user_pin) as session:
            pass
        self.key_id = params[b'id']

    def shortname(self):
        return "ecdsa"

    def _unsupported(self, name):
        raise PKCS11UsageError("Operation {} requires private key".format(name))

    def get_public_bytes(self):
        with self.token.open(user_pin=self.user_pin) as session:
            pub = session.get_key(
                id=self.key_id,
                key_type=pkcs11.KeyType.EC,
                object_class=pkcs11.ObjectClass.PUBLIC_KEY
            )
            key = oscrypto.asymmetric.load_public_key(
                pkcs11.util.ec.encode_ec_public_key(pub)
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
                key_type=pkcs11.KeyType.EC,
                object_class=pkcs11.ObjectClass.PUBLIC_KEY
            )
            key = oscrypto.asymmetric.load_public_key(
                pkcs11.util.ec.encode_ec_public_key(pub)
            )
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
        with self.token.open(user_pin=self.user_pin) as session:
            priv = session.get_key(
                id=self.key_id,
                key_type=pkcs11.KeyType.EC,
                object_class=pkcs11.ObjectClass.PRIVATE_KEY
            )
            sig = priv.sign(
                hashlib.sha256(payload).digest(),
                mechanism=pkcs11.Mechanism.ECDSA
            )
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
