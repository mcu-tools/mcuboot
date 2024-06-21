"""
Tests for ECDSA keys
"""

# SPDX-License-Identifier: Apache-2.0

import hashlib
import io
import os.path
import sys
import tempfile
import unittest

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives.asymmetric import ed25519

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '../..')))

from imgtool.keys import load, Ed25519, Ed25519UsageError


class Ed25519KeyGeneration(unittest.TestCase):

    def setUp(self):
        self.test_dir = tempfile.TemporaryDirectory()

    def tname(self, base):
        return os.path.join(self.test_dir.name, base)

    def tearDown(self):
        self.test_dir.cleanup()

    def test_keygen(self):
        name1 = self.tname("keygen.pem")
        k = Ed25519.generate()
        k.export_private(name1, b'secret')

        self.assertIsNone(load(name1))

        k2 = load(name1, b'secret')

        pubname = self.tname('keygen-pub.pem')
        k2.export_public(pubname)
        pk2 = load(pubname)

        # We should be able to export the public key from the loaded
        # public key, but not the private key.
        pk2.export_public(self.tname('keygen-pub2.pem'))
        self.assertRaises(Ed25519UsageError,
                          pk2.export_private, self.tname('keygen-priv2.pem'))

    def test_emit(self):
        """Basic sanity check on the code emitters."""
        k = Ed25519.generate()

        pubpem = io.StringIO()
        k.emit_public_pem(pubpem)
        self.assertIn("BEGIN PUBLIC KEY", pubpem.getvalue())
        self.assertIn("END PUBLIC KEY", pubpem.getvalue())

        ccode = io.StringIO()
        k.emit_c_public(ccode)
        self.assertIn("ed25519_pub_key", ccode.getvalue())
        self.assertIn("ed25519_pub_key_len", ccode.getvalue())

        hashccode = io.StringIO()
        k.emit_c_public_hash(hashccode)
        self.assertIn("ed25519_pub_key_hash", hashccode.getvalue())
        self.assertIn("ed25519_pub_key_hash_len", hashccode.getvalue())

        rustcode = io.StringIO()
        k.emit_rust_public(rustcode)
        self.assertIn("ED25519_PUB_KEY", rustcode.getvalue())

        # raw data - bytes
        pubraw = io.BytesIO()
        k.emit_raw_public(pubraw)
        self.assertTrue(len(pubraw.getvalue()) > 0)

        hashraw = io.BytesIO()
        k.emit_raw_public_hash(hashraw)
        self.assertTrue(len(hashraw.getvalue()) > 0)

    def test_emit_pub(self):
        """Basic sanity check on the code emitters, from public key."""
        pubname = self.tname("public.pem")
        k = Ed25519.generate()
        k.export_public(pubname)

        k2 = load(pubname)

        ccode = io.StringIO()
        k2.emit_c_public(ccode)
        self.assertIn("ed25519_pub_key", ccode.getvalue())
        self.assertIn("ed25519_pub_key_len", ccode.getvalue())

        rustcode = io.StringIO()
        k2.emit_rust_public(rustcode)
        self.assertIn("ED25519_PUB_KEY", rustcode.getvalue())

    def test_sig(self):
        k = Ed25519.generate()
        buf = b'This is the message'
        sha = hashlib.sha256()
        sha.update(buf)
        digest = sha.digest()
        sig = k.sign_digest(digest)

        # The code doesn't have any verification, so verify this
        # manually.
        k.key.public_key().verify(signature=sig, data=digest)

        # Modify the message to make sure the signature fails.
        sha = hashlib.sha256()
        sha.update(b'This is thE message')
        new_digest = sha.digest()
        self.assertRaises(InvalidSignature,
                          k.key.public_key().verify,
                          signature=sig,
                          data=new_digest)


if __name__ == '__main__':
    unittest.main()
