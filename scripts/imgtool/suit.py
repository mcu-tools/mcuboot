# Copyright 2019 Linaro Limited
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
SUIT Image signing and management.
"""

from . import image
import cbor2
import hashlib
import os.path
import struct

IMAGE_MAGIC = 0x96f3b83e
IMAGE_HEADER_SIZE = 32
BIN_EXT = "bin"
INTEL_HEX_EXT = "hex"
DEFAULT_MAX_SECTORS = 128

MANIFEST_INFO_SIZE = 4
MANIFEST_INFO_MAGIC = 0x6917

STRUCT_ENDIAN_DICT = {
        'little': '<',
        'big:': '>'
}

class Suit():

    def __init__(self, endian):
        self.payload = None
        self.sequence = 0
        self.endian = endian

    def add_payload(self, payload):
        self.payload = payload

    def set_sequence(self, sequence):
        self.sequence = sequence

    def generate(self, key):
        manifest = self._gen_manifest(self.payload)
        sig = self._gen_sig(manifest, key)

        body = cbor2.dumps({1: sig, 2: manifest})

        return (struct.pack(STRUCT_ENDIAN_DICT[self.endian] +
                "HH", MANIFEST_INFO_MAGIC, MANIFEST_INFO_SIZE + len(body)) +
                body)

    def _get_digest(self, payload, prot):
        """Make a COSE Digest of the payload, with the given
        CBOR-encoded protected data.  This is defined by SUIT."""
        block = cbor2.dumps(["Digest", prot, payload])
        sha = hashlib.sha256()
        sha.update(block)
        digest = sha.digest()

        return [prot, {}, None, digest]

    def _gen_manifest(self, payload):
        """Generate the actual SUIT manifest itself."""
        prot = cbor2.dumps({1: 41})
        digest = self._get_digest(payload, prot)

        return cbor2.dumps({
            1: 1,
            2: self.sequence,
            3: [ { 1: [b"0"], 2: len(payload), 3: digest } ] })

    def _gen_sig(self, body, key):
        """Generate a COSE signature wrapper for the given payload,
        signed with the given key.  This doesn't return CBOR, but a
        Python data structure intended to be included within CBOR."""
        body_prot = cbor2.dumps({3: 0})
        sig_prot = cbor2.dumps({1: -37}) # TODO: Hardcoded, -37 is RS256.

        # The block we actually sign.
        sig_block = cbor2.dumps(["Signature", body_prot, sig_prot, b"", body])
        
        sig = key.sign(bytes(sig_block))

        return cbor2.CBORTag(98, [body_prot, {}, None, [
                    [sig_prot, {4: b"key-id-here"}, sig]]])

class Image(image.Image):

    def __init__(self, **kwargs):
        image.Image.__init__(self, **kwargs)

    def check(self):
        """Perform some sanity checking of the image."""
        # If there is a header requested, make sure that the image
        # starts with all zeros.
        if self.header_size > 0:
            if any(v != 0 for v in self.payload[0:self.header_size]):
                raise Exception("Padding requested, but image does not start with zeros")
        if self.slot_size > 0:
            tsize = self._trailer_size(self.align, self.max_sectors,
                    self.overwrite_only)
            padding = self.slot_size - (len(self.payload) + tsize)
            if padding < 0:
                msg = "Image size (0x{:x}) + trailer (0x{:x}) exceeds requested size 0x{:x}".format(
                        len(self.payload), tsize, self.slot_size)
                raise Exception(msg)

    def create(self, key, enckey):
        if enckey is not None:
            raise Exception("SUIT support does not yet support encryption")

        self.add_header(None)

        s = Suit(self.endian)
        s.add_payload(self.payload)
        # TODO: Where does this sequence number come from?
        s.set_sequence(self.version.build)

        self.payload += s.generate(key)

    def _image_magic(self):
        return IMAGE_MAGIC
