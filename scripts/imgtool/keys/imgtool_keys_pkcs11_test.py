"""
Tests for PKCS11 keys
"""
# SPDX-License-Identifier: Apache-2.0

import io
import os.path
import pkcs11
import pkcs11.exceptions
import sys
import tempfile
import unittest

from datetime import datetime
from pkcs11.util import ec
from urllib.parse import quote

sys.path.insert(0, os.path.abspath(
    os.path.join(os.path.dirname(__file__), '../..')))

from imgtool.keys import load, PKCS11
from imgtool.keys.imgtool_keys_pkcs11 import \
    get_pkcs11_uri_params, unquote_to_bytes

class GetPKCS11URLParams(unittest.TestCase):
    def test_unquote_verbatim(self):
        for i in range(0, 256):
            with self.subTest(i=i):
                urlencoded_string = '%%%2.2x' % i
                actual_bytes = unquote_to_bytes(urlencoded_string)
                expected_bytes = bytes([i])
                self.assertEqual(
                    actual_bytes,
                    expected_bytes
                )

    def test_get_pkcs11_uri_params(self):
        url = 'pkcs11:model=PKCS%2315%20emulated;manufacturer=www.CardContact.de;serial=DENK0103525;token=SmartCard-HSM%20%28UserPIN%29;id=%9E%81%81%27%0C%DE%85%32%75%86%61%E9%87%9A%69%E8%5E%9B%4F%24;object=2020-10-14-mcuboot;type=private'
        actual_params = get_pkcs11_uri_params(url)
        expected_params = {
            b'model': b'PKCS#15 emulated',
            b'manufacturer': b'www.CardContact.de',
            b'serial': b'DENK0103525',
            b'token': b'SmartCard-HSM (UserPIN)',
            b'id': b'\x9E\x81\x81\x27\x0C\xDE\x85\x32\x75\x86\x61\xE9\x87\x9A\x69\xE8\x5E\x9B\x4F\x24',
            b'object': b'2020-10-14-mcuboot',
            b'type': b'private'
        }
        self.assertEqual(
            actual_params,
            expected_params
        )


class EcKeyGeneration(unittest.TestCase):
    def setUp(self):
        self.test_dir = tempfile.TemporaryDirectory()
        self.lib_path = os.environ.get('PKCS11_MODULE', 'opensc-pkcs11.so')
        lib = pkcs11.lib(self.lib_path)
        # There may be multiple tokens. Pick the first.
        slot = lib.get_slots(token_present=True)[0]
        self.token = slot.get_token()
        self.serial = self.token.serial.decode("iso-8859-1")
        self.user_pin = os.environ['PKCS11_PIN']

        with self.token.open(user_pin=self.user_pin, rw=True) as session:
            ecparams = session.create_domain_parameters(
                pkcs11.KeyType.EC,
                {
                    pkcs11.Attribute.EC_PARAMS: ec.encode_named_curve_parameters('secp384r1'),
                },
                local=True
            )
            timestamp = datetime.now().isoformat()
            pubkey, privkey = ecparams.generate_keypair(
                store=True,
                label=f"imgtool.py test key {timestamp}"
            )
            self.key_id = privkey.id
        self.pkcs11_uri = f"pkcs11:serial={self.serial};id={quote(self.key_id)}"

    def tname(self, base):
        return os.path.join(
            self.test_dir.name,
            base
        )

    def tearDown(self):
        self.test_dir.cleanup()
        with self.token.open(user_pin=self.user_pin, rw=True) as session:
            privkey = session.get_key(
                id=self.key_id,
                key_type=pkcs11.KeyType.EC,
                object_class=pkcs11.ObjectClass.PRIVATE_KEY
            )
            privkey.destroy()
            pubkey = session.get_key(
                id=self.key_id,
                key_type=pkcs11.KeyType.EC,
                object_class=pkcs11.ObjectClass.PUBLIC_KEY
            )
            pubkey.destroy()

    def test_emit(self):
        """Basic smoke test on the code emitters."""
        k = PKCS11(uri=self.pkcs11_uri)

        ccode = io.StringIO()
        k.emit_c_public(ccode)
        self.assertIn(
            "ecdsa_pub_key",
            ccode.getvalue(),
        )
        self.assertIn(
            "ecdsa_pub_key_len",
            ccode.getvalue(),
        )

        rustcode = io.StringIO()
        k.emit_rust_public(rustcode)
        self.assertIn(
            "ECDSA_PUB_KEY",
            rustcode.getvalue(),
        )

    def test_emit_pub(self):
        """Basic smoke test on the code emitters."""
        pubname = self.tname("public.pem")
        k = PKCS11(uri=self.pkcs11_uri)
        k.export_public(pubname)

        k2 = load(pubname)

        ccode = io.StringIO()
        k2.emit_c_public(ccode)
        self.assertIn(
            "ecdsap384_pub_key",
            ccode.getvalue(),
        )
        self.assertIn(
            "ecdsap384_pub_key_len",
            ccode.getvalue(),
        )

        rustcode = io.StringIO()
        k2.emit_rust_public(rustcode)
        self.assertIn(
            "ECDSAP384_PUB_KEY",
            rustcode.getvalue(),
        )

    def test_sig(self):
        k = PKCS11(uri=self.pkcs11_uri)
        buf = b'This is the message'
        sig = k.raw_sign(buf)

        self.assertTrue(k.verify(
            signature=sig,
            payload=buf))

        self.assertFalse(k.verify(
            signature=sig,
            payload=b'This is not the message'))

    def clone_env(self):
        # PKCS module can only support one loaded library.
        # Ensure tests use the same one used by setUp.
        return {
            'PKCS11_PIN': os.environ['PKCS11_PIN'],
            'PKCS11_MODULE': os.environ.get('PKCS11_MODULE','opensc-pkcs11.so')
        }

    def test_env(self):
        env = self.clone_env()
        with self.assertRaises(
                pkcs11.exceptions.NoSuchToken
        ):
            PKCS11(uri='pkcs11:serial=bogus;id=00', env=env)

        env = self.clone_env()
        del env['PKCS11_PIN']
        with self.assertRaises(RuntimeError) as context_manager:
            PKCS11(
                uri=self.pkcs11_uri,
                env=env,
            )
        self.assertEqual(
            str(context_manager.exception),
            'Environment variable PKCS11_PIN not set. Set it to the user PIN.'
        )

        env = self.clone_env()
        env['PKCS11_PIN'] = 'notavalidpin'
        with self.assertRaises(pkcs11.exceptions.PinIncorrect):
            PKCS11(
                uri=self.pkcs11_uri,
                env=env,
            )

        env = self.clone_env()
        with self.assertRaises(pkcs11.exceptions.NoSuchKey):
            pkcs11_uri = f"pkcs11:serial={self.serial};id=00"
            k = PKCS11(
                uri=pkcs11_uri,
                env=env,
            )
            k.raw_sign(payload=b'test')

    def test_sig_statistical(self):
        k = PKCS11(uri=self.pkcs11_uri)
        buf = b'This is the message'

        total = 173
        for n in range(total):
            sys.stdout.write(f'\rtest signature {n} / {total} ...')
            sys.stdout.flush()

            sig = k.raw_sign(buf)

            self.assertTrue(k.verify(
                signature=sig,
                payload=buf,
            ))


if __name__ == '__main__':
    unittest.main()
