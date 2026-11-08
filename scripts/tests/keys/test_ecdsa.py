"""
Tests for ECDSA keys
"""

# SPDX-License-Identifier: Apache-2.0

import io
import os.path
import sys
import tempfile
import unittest

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.hashes import SHA256, SHA384

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '../..')))

from imgtool.keys import ECDSA256P1, ECDSA384P1, ECDSAUsageError, load


class EcKeyGeneration(unittest.TestCase):

    def setUp(self):
        self.test_dir = tempfile.TemporaryDirectory()

    def tname(self, base):
        return os.path.join(self.test_dir.name, base)

    def tearDown(self):
        self.test_dir.cleanup()

    def test_keygen(self):
        name1 = self.tname("keygen.pem")
        k = ECDSA256P1.generate()
        k.export_private(name1, b'secret')

        self.assertIsNone(load(name1))

        k2 = load(name1, b'secret')

        pubname = self.tname('keygen-pub.pem')
        k2.export_public(pubname)
        pk2 = load(pubname)

        # We should be able to export the public key from the loaded
        # public key, but not the private key.
        pk2.export_public(self.tname('keygen-pub2.pem'))
        self.assertRaises(ECDSAUsageError,
                pk2.export_private, self.tname('keygen-priv2.pem'))

    def test_emit(self):
        """Basic sanity check on the code emitters."""
        k = ECDSA256P1.generate()

        pubpem = io.StringIO()
        k.emit_public_pem(pubpem)
        self.assertIn("BEGIN PUBLIC KEY", pubpem.getvalue())
        self.assertIn("END PUBLIC KEY", pubpem.getvalue())

        ccode = io.StringIO()
        k.emit_c_public(ccode)
        self.assertIn("ecdsa_pub_key", ccode.getvalue())
        self.assertIn("ecdsa_pub_key_len", ccode.getvalue())

        hashccode = io.StringIO()
        k.emit_c_public_hash(hashccode)
        self.assertIn("ecdsa_pub_key_hash", hashccode.getvalue())
        self.assertIn("ecdsa_pub_key_hash_len", hashccode.getvalue())

        rustcode = io.StringIO()
        k.emit_rust_public(rustcode)
        self.assertIn("ECDSA_PUB_KEY", rustcode.getvalue())

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
        k = ECDSA256P1.generate()
        k.export_public(pubname)

        k2 = load(pubname)

        ccode = io.StringIO()
        k2.emit_c_public(ccode)
        self.assertIn("ecdsa_pub_key", ccode.getvalue())
        self.assertIn("ecdsa_pub_key_len", ccode.getvalue())

        rustcode = io.StringIO()
        k2.emit_rust_public(rustcode)
        self.assertIn("ECDSA_PUB_KEY", rustcode.getvalue())

    def _test_sig(self, key_cls, hash_alg, pem_file):
        """Test ECDSA unpadded signature generation, verification & failure."""
        buf = b'This is the message'
        k = key_cls.generate()
        sig = k.sign(buf)

        # 1. Verify signature is strictly unpadded (exact ASN.1 DER length).
        self.assertEqual(len(sig), sig[1] + 2)

        # 2. Verify signature via cryptography library as an oracle.
        k.key.public_key().verify(
                signature=sig,
                data=buf,
                signature_algorithm=ec.ECDSA(hash_alg()))

        # 3. Test verify() method using signature tested in #2 & private key.
        k.verify(signature=sig, payload=buf)

        # 4. Modify the message & test verify() fails with private key.
        self.assertRaises(
                InvalidSignature,
                k.verify,
                signature=sig,
                payload=b'This is thE message')

        # 5. Modify the signature & tests verify() fails with private key.
        corrupted_sig = sig[:-6] + b'\xFF\xFF\xFF\xFF\xFF\xFF'
        self.assertRaises(
                InvalidSignature,
                k.verify,
                signature=corrupted_sig,
                payload=buf)

        # 6. Test verify() using signature tested in #2 & generated public key.
        public_key_name = self.tname(pem_file)
        k.export_public(public_key_name)
        pub_key = load(public_key_name)
        pub_key.verify(signature=sig, payload=buf)

        # 7. Modify the message & test verify() fails with public key.
        self.assertRaises(
                InvalidSignature,
                pub_key.verify,
                signature=sig,
                payload=b'This is thE message')

        # 8. Modify the signature & tests verify() fails with public key
        corrupted_sig = sig[:-6] + b'\xFF\xFF\xFF\xFF\xFF\xFF'
        self.assertRaises(
                InvalidSignature,
                pub_key.verify,
                signature=corrupted_sig,
                payload=buf)

        # 9. Test verify() fails with a completely different public key
        wrong_key = key_cls.generate()
        self.assertRaises(
                InvalidSignature,
                wrong_key.verify,
                signature=sig,
                payload=buf)

    def test_sig256(self):
        """Test ECDSA256 unpadded signature and verif method."""
        self._test_sig(ECDSA256P1, SHA256, 'public256.pem')

    def test_sig384(self):
        """Test ECDSA384 unpadded signature and verif method."""
        self._test_sig(ECDSA384P1, SHA384, 'public384.pem')

    def _test_pad_sig(self, key_cls, hash_alg, pem_file):
        """Test ECDSA padded signature generation, verification & failure."""
        buf = b'This is the message'
        k = key_cls.generate()
        k.pad_sig = True

        public_key_name = self.tname(pem_file)
        k.export_public(public_key_name)
        pub_key = load(public_key_name)

        target_lengths = {k.sig_len() - 2, k.sig_len() - 1, k.sig_len()}

        for _ in range(300):
            sig_with_padding = k.sign(buf)
            raw_len = sig_with_padding[1] + 2

            # 1. Verify padded signature total length.
            self.assertEqual(len(sig_with_padding), k.sig_len())

            if raw_len not in target_lengths:
                continue
            target_lengths.remove(raw_len)

            # 2. Verify padding is exactly : padding_length * '\x00'.
            self.assertEqual(
                sig_with_padding[raw_len:],
                b'\x00' * (k.sig_len() - raw_len))

            # 3. Verify strict cryptography oracle rejects padded signature.
            if raw_len < k.sig_len():
                self.assertRaises(
                        InvalidSignature,
                        k.key.public_key().verify,
                        signature=sig_with_padding,
                        data=buf,
                        signature_algorithm=ec.ECDSA(hash_alg()))

            # 4. Verify cryptography oracle accept signature without padding.
            k.key.public_key().verify(
                    signature=sig_with_padding[:raw_len],
                    data=buf,
                    signature_algorithm=ec.ECDSA(hash_alg()))

            # 5. Test verify() method using padded signature & private key.
            k.verify(signature=sig_with_padding, payload=buf)

            # 6. Test verify() using padded signature & generated public key.
            pub_key.verify(signature=sig_with_padding, payload=buf)

            # 7. Modify the message & test verify() fails with private key.
            self.assertRaises(
                    InvalidSignature,
                    k.verify,
                    signature=sig_with_padding,
                    payload=b'This is thE message')

            # 8. Modify signature payload & verify() fails with private key.
            corrupted_body = (
                sig_with_padding[:raw_len - 6] +
                b'\xFF\xFF\xFF\xFF\xFF\xFF' + sig_with_padding[raw_len:])
            self.assertRaises(
                    InvalidSignature,
                    k.verify,
                    signature=corrupted_body,
                    payload=buf)

            # 9. Modify the message & test verify() fails with public key.
            self.assertRaises(
                    InvalidSignature,
                    pub_key.verify,
                    signature=sig_with_padding,
                    payload=b'This is thE message')

            # 10. Modify signature payload & verify() fails with public key.
            self.assertRaises(
                    InvalidSignature,
                    pub_key.verify,
                    signature=corrupted_body,
                    payload=buf)

            if not target_lengths:
                break

        # 11. Ensure all possible padding lengths have been tested.
        self.assertFalse(target_lengths)

    def test_pad_sig256(self):
        """Test ECDSA256 padded signature and verif method."""
        self._test_pad_sig(ECDSA256P1, SHA256, 'public256_pad.pem')

    def test_pad_sig384(self):
        """Test ECDSA384 padded signature and verif method."""
        self._test_pad_sig(ECDSA384P1, SHA384, 'public384_pad.pem')


if __name__ == '__main__':
    unittest.main()
