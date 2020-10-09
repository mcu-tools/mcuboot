"""
Tests for PKCS11 keys
"""

import io
import os.path
import sys
import tempfile
import unittest
import oscrypto
import oscrypto.errors
import pkcs11
import pkcs11.exceptions

sys.path.insert(0, os.path.abspath(
    os.path.join(os.path.dirname(__file__), '../..')))

from imgtool.keys import load, PKCS11


class EcKeyGeneration(unittest.TestCase):

    def setUp(self):
        self.test_dir = tempfile.TemporaryDirectory()

    def tname(self, base):
        return os.path.join(self.test_dir.name, base)

    def tearDown(self):
        self.test_dir.cleanup()

    def test_emit(self):
        """Basic sanity check on the code emitters."""
        k = PKCS11()

        ccode = io.StringIO()
        k.emit_c_public(ccode)
        self.assertIn("ecdsa_pub_key", ccode.getvalue())
        self.assertIn("ecdsa_pub_key_len", ccode.getvalue())

        rustcode = io.StringIO()
        k.emit_rust_public(rustcode)
        self.assertIn("ECDSA_PUB_KEY", rustcode.getvalue())

    def test_emit_pub(self):
        """Basic sanity check on the code emitters."""
        pubname = self.tname("public.pem")
        k = PKCS11()
        k.export_public(pubname)

        k2 = load(pubname)

        ccode = io.StringIO()
        k2.emit_c_public(ccode)
        self.assertIn("ecdsa_pub_key", ccode.getvalue())
        self.assertIn("ecdsa_pub_key_len", ccode.getvalue())

        rustcode = io.StringIO()
        k2.emit_rust_public(rustcode)
        self.assertIn("ECDSA_PUB_KEY", rustcode.getvalue())

    def test_sig(self):
        k = PKCS11()
        buf = b'This is the message'
        sig = k.raw_sign(buf)

        k.verify(
            signature=sig,
            payload=buf)

        # Modify the message to make sure the signature fails.
        self.assertRaises(oscrypto.errors.SignatureError,
                          k.verify,
                          signature=sig,
                          payload=b'This is thE message')

    def clone_env(self):
        return {
            'PKCS11_MODULE': os.environ['PKCS11_MODULE'],
            'PKCS11_TOKEN_LABEL': os.environ['PKCS11_TOKEN_LABEL'],
            'PKCS11_KEY_ID': os.environ['PKCS11_KEY_ID'],
            'PKCS11_PIN': os.environ['PKCS11_PIN'],
        }

    def test_env(self):
        env = self.clone_env()
        env['PKCS11_MODULE'] = '/invalid/path'
        self.assertRaises(
            pkcs11.exceptions.AlreadyInitialized, PKCS11, env=env)

        env = self.clone_env()
        env['PKCS11_TOKEN_LABEL'] = 'invalid_label'
        self.assertRaises(pkcs11.exceptions.NoSuchToken, PKCS11, env=env)

        env = self.clone_env()
        env['PKCS11_PIN'] = '123456'
        self.assertRaises(pkcs11.exceptions.PinIncorrect, PKCS11, env=env)

        env = self.clone_env()
        env['PKCS11_KEY_ID'] = '00'
        k = PKCS11(env)
        self.assertRaises(pkcs11.exceptions.NoSuchKey,
                          k.raw_sign, payload=b'test')


if __name__ == '__main__':
    unittest.main()
