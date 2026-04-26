# Copyright 2026 Linaro Limited
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
LMS (RFC 8554) key management.

Wraps `pyhsslms` so imgtool can generate keys, sign image digests, and
verify signatures. The bootloader side (Mbed TLS 4.x) currently
verifies only LMS_SHA256_M32_H10 + LMOTS_SHA256_N32_W8 — that is the
sole parameter set MCUboot can verify today. The wrapper still
carries the parameter set through so additional sets can be surfaced
from the CLI by extending LMS_PARAM_SETS once the verifier supports
them; smaller-tree variants (e.g. lms-sha256-h5-w8) are useful for
imgtool round-trip tests but cannot be used to sign images intended
for an MCUboot device until the bootloader-side support lands.

Stateful-key hazard: LMS is built on a one-time signature scheme, so
the leaf index `q` must never be reused. The private key file holds
`q`; each successful sign() advances it and persists the new value.
Treat the .lms file as a single-writer object — never restore from
backup, never copy, never sign in parallel processes.
"""

import base64
import os

import pyhsslms
from cryptography.exceptions import InvalidSignature

from .general import KeyClass

PRIV_MARKER_BEGIN = b'-----BEGIN MCUBOOT LMS PRIVATE KEY-----'
PRIV_MARKER_END = b'-----END MCUBOOT LMS PRIVATE KEY-----'
PUB_MARKER_BEGIN = b'-----BEGIN MCUBOOT LMS PUBLIC KEY-----'
PUB_MARKER_END = b'-----END MCUBOOT LMS PUBLIC KEY-----'


# Adding a new entry here is the only change needed to surface another
# LMS variant on the CLI; the wrapper itself is parameter-set-agnostic.
# Only h10-w8 is verifiable by MCUboot today (Mbed TLS 4.x ships only
# that one set); other entries are usable for imgtool round-trip tests
# but cannot be paired with a current bootloader build.
LMS_PARAM_SETS = {
    'lms-sha256-h10-w8': (pyhsslms.lms_sha256_m32_h10,
                          pyhsslms.lmots_sha256_n32_w8),
    'lms-sha256-h5-w8':  (pyhsslms.lms_sha256_m32_h5,
                          pyhsslms.lmots_sha256_n32_w8),
}


class LMSUsageError(Exception):
    pass


def _wrap_pem(begin, end, body):
    return begin + b'\n' + base64.encodebytes(body).rstrip() + b'\n' + end + b'\n'


def _unwrap_pem(begin, end, data):
    lines = data.splitlines()
    try:
        i = lines.index(begin)
        j = lines.index(end, i + 1)
    except ValueError as e:
        raise LMSUsageError("not an MCUboot LMS key file") from e
    return base64.decodebytes(b'\n'.join(lines[i + 1:j]))


def is_lms_key_data(data):
    s = data.lstrip()
    return s.startswith(PRIV_MARKER_BEGIN) or s.startswith(PUB_MARKER_BEGIN)


def load(data, path=None):
    s = data.lstrip()
    if s.startswith(PRIV_MARKER_BEGIN):
        raw = _unwrap_pem(PRIV_MARKER_BEGIN, PRIV_MARKER_END, data)
        return LMS(pyhsslms.LmsPrivateKey.deserialize(raw), path=path)
    if s.startswith(PUB_MARKER_BEGIN):
        raw = _unwrap_pem(PUB_MARKER_BEGIN, PUB_MARKER_END, data)
        return LMSPublic(pyhsslms.LmsPublicKey.deserialize(raw))
    raise LMSUsageError("not an MCUboot LMS key file")


def _sig_len(lms_type, lmots_type):
    # RFC 8554 §5.4: u32(q) + lmots_sig + u32(lms_type) + path
    # lmots_sig = u32(lmots_type) + C(n) + y[p][n]
    _, n, p, _, _ = pyhsslms.lmots_params[lmots_type]
    _, m, h = pyhsslms.lms_params[lms_type]
    return 4 + 4 + n + p * n + 4 + h * m


class LMSPublic(KeyClass):
    def __init__(self, key):
        self.key = key

    def shortname(self):
        return "lms"

    def _unsupported(self, name):
        raise LMSUsageError(f"Operation {name} requires private key")

    def get_public_bytes(self):
        # RFC 8554 §5.3: u32(lms_type) || u32(lmots_type) || I || T[1]
        return self._public_key().serialize()

    def _public_key(self):
        return self.key

    def get_public_pem(self):
        return _wrap_pem(PUB_MARKER_BEGIN, PUB_MARKER_END,
                         self._public_key().serialize())

    def get_private_bytes(self, minimal, format):
        self._unsupported('get_private_bytes')

    def export_private(self, path, passwd=None):
        self._unsupported('export_private')

    def export_public(self, path):
        with open(path, 'wb') as f:
            f.write(self.get_public_pem())

    def sig_type(self):
        return "LMS"

    def sig_tlv(self):
        return "LMS"

    def sig_len(self):
        return _sig_len(self._public_key().lms_type,
                        self._public_key().lmots_type)

    def verify_digest(self, signature, digest):
        if not self._public_key().verify(digest, signature):
            raise InvalidSignature("LMS signature verification failed")


class LMS(LMSPublic):
    """Stateful LMS private key. See module docstring for the q-reuse hazard."""

    def __init__(self, key, path=None):
        super().__init__(key)
        self._path = path

    @staticmethod
    def generate(param_name='lms-sha256-h10-w8'):
        if param_name not in LMS_PARAM_SETS:
            raise LMSUsageError(f"unknown LMS parameter set: {param_name}")
        lms_type, lmots_type = LMS_PARAM_SETS[param_name]
        return LMS(pyhsslms.LmsPrivateKey(lms_type=lms_type,
                                         lmots_type=lmots_type))

    def _public_key(self):
        return self.key.publicKey()

    def export_private(self, path, passwd=None):
        if passwd is not None:
            raise LMSUsageError(
                "password protection of LMS keys is not supported")
        self._path = path
        self._save_state()

    def _save_state(self):
        if self._path is None:
            raise LMSUsageError("LMS private key has no associated file path")
        pem = _wrap_pem(PRIV_MARKER_BEGIN, PRIV_MARKER_END, self.key.serialize())
        tmp = self._path + '.tmp'
        with open(tmp, 'wb') as f:
            f.write(pem)
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, self._path)

    def sign_digest(self, digest):
        if self.key.is_exhausted():
            raise LMSUsageError(
                "LMS private key is exhausted (all 2^h indices used)")
        sig = self.key.sign(digest)
        if self._path is not None:
            self._save_state()
        return sig

    def remaining(self):
        return self.key.remaining()
