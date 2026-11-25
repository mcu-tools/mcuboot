# SPDX-License-Identifier: Apache-2.0

from pathlib import Path

import pytest
from click.testing import CliRunner
from imgtool.main import imgtool

ROOT = Path(__file__).parents[2]


def sign(tmpdir, key, enc_args):
    in_file = tmpdir / 'app.bin'
    in_file.write_binary(b'\xa5' * 1024)
    return CliRunner().invoke(imgtool, [
        'sign', str(in_file), str(tmpdir / 'app_signed.bin'),
        '--header-size=0x400', '--slot-size=0x10000', '--version=1.0.0',
        '--pad-header', f'--key={ROOT / key}', *enc_args,
    ])


@pytest.mark.parametrize('enc_args', [
    ['--encrypt', str(ROOT / 'enc-ec256-pub.pem')],
    ['--encrypt-xip', str(ROOT / 'enc-ec256-pub.pem'),
     '--xip-base-address=0x60000000'],
])
def test_p384_signature_with_ec256_envelope(tmpdir, enc_args):
    result = sign(tmpdir, 'root-ec-p384.pem', enc_args)
    assert result.exit_code == 0, result.output


@pytest.mark.parametrize('key', ['root-ec-p256.pem', 'root-ec-p384.pem'])
def test_ec_signature_with_rsa_envelope_is_rejected(tmpdir, key):
    result = sign(tmpdir, key, ['--encrypt', str(ROOT / 'enc-rsa2048-pub.pem')])
    assert result.exit_code != 0
    assert 'same type of key' in result.output
