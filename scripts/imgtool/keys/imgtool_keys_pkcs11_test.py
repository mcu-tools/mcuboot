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

from datetime import datetime
from imgtool.keys import load, PKCS11
from imgtool.keys.imgtool_keys_pkcs11 import \
    find_library, get_pkcs11_uri_params, unquote_to_bytes
from pkcs11.util import ec
from urllib.parse import quote

class GetPKCS11URLParams(unittest.TestCase):
    def setUp(self):
        self.maxDiff = None ## todo: remove

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
        self.library_path = find_library('opensc-pkcs11.so')  # tests require OpenSC
        lib = pkcs11.lib(self.library_path)
        self.token = lib.get_token()
        self.serial = self.token.serial.decode("iso-8859-1")
        self.user_pin = os.environ['PKCS11_PIN']

        with self.token.open(user_pin=self.user_pin, rw=True) as session:
            ecparams = session.create_domain_parameters(
                pkcs11.KeyType.EC,
                {
                    pkcs11.Attribute.EC_PARAMS: ec.encode_named_curve_parameters('secp256r1'),
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
        """Basic sanity check on the code emitters."""
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
        """Basic sanity check on the code emitters."""
        pubname = self.tname("public.pem")
        k = PKCS11(uri=self.pkcs11_uri)
        k.export_public(pubname)

        k2 = load(pubname)

        ccode = io.StringIO()
        k2.emit_c_public(ccode)
        self.assertIn(
            "ecdsa_pub_key",
            ccode.getvalue(),
        )
        self.assertIn(
            "ecdsa_pub_key_len",
            ccode.getvalue(),
        )

        rustcode = io.StringIO()
        k2.emit_rust_public(rustcode)
        self.assertIn(
            "ECDSA_PUB_KEY",
            rustcode.getvalue(),
        )

    def test_sig(self):
        k = PKCS11(uri=self.pkcs11_uri)
        buf = b'This is the message'
        sig = k.raw_sign(buf)

        k.verify(
            signature=sig,
            payload=buf)

        # Modify the message to make sure the signature fails.
        self.assertRaises(
            oscrypto.errors.SignatureError,
            k.verify,
            signature=sig,
            payload=b'This is thE message'
        )

    def clone_env(self):
        return {
            'PKCS11_PIN': os.environ['PKCS11_PIN'],
        }

    def test_env(self):
        env = self.clone_env()
        env['PKCS11_MODULE'] = '/invalid/path'
        with self.assertRaises(RuntimeError) as context_manager:
            PKCS11(
                uri=self.pkcs11_uri,
                env=env,
            )
        self.assertEqual(
            str(context_manager.exception),
            '$PKCS11_MODULE is not a file: /invalid/path'
        )

        env['PKCS11_MODULE'] = self.library_path
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
        env['PKCS11_PIN'] = '123456'
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

        total = 1750
        for n in range(total):
            sys.stdout.write(f'\rtest signature {n} / {total} ...')
            sys.stdout.flush()

            sig = k.raw_sign(buf)

            k.verify(
                signature=sig,
                payload=buf,
            )


if __name__ == '__main__':
    unittest.main()
