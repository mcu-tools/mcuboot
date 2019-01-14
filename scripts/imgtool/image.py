# Copyright 2018 Nordic Semiconductor ASA
# Copyright 2017 Linaro Limited
# Copyright 2019 Arm Limited
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
Image signing and management.
"""

from . import version as versmod
from intelhex import IntelHex
import hashlib
import struct
import os.path
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import hashes

IMAGE_MAGIC =         0x96f3b83d
IMAGE_HEADER_SIZE =   32
BIN_EXT =             "bin"
INTEL_HEX_EXT =       "hex"
DEFAULT_MAX_SECTORS = 128

# Image header flags.
IMAGE_F = {
        'PIC':                   0x0000001,
        'NON_BOOTABLE':          0x0000010,
        'ENCRYPTED':             0x0000004,
}

TLV_VALUES = {
        'KEYHASH':      0x01,
        'SHA256':       0x10,
        'RSA2048':      0x20,
        'ECDSA224':     0x21,
        'ECDSA256':     0x22,
        'ENCRSA2048':   0x30,
        'ENCKW128':     0x31,
        'DEPENDENCY':   0x40
}

TLV_INFO_SIZE =  4
TLV_INFO_MAGIC = 0x6907

boot_magic = bytes([
    0x77, 0xc2, 0x95, 0xf3,
    0x60, 0xd2, 0xef, 0x7f,
    0x35, 0x52, 0x50, 0x0f,
    0x2c, 0xb6, 0x79, 0x80, ])

STRUCT_ENDIAN_DICT = {
        'little': '<',
        'big':    '>'
}

class TLV():
    def __init__(self, endian):
        self.buf = bytearray()
        self.endian = endian

    def add(self, kind, payload):
        """Add a TLV record.  Kind should be a string found in TLV_VALUES above."""
        e = STRUCT_ENDIAN_DICT[self.endian]
        buf = struct.pack(e + 'BBH', TLV_VALUES[kind], 0, len(payload))
        self.buf += buf
        self.buf += payload

    def get(self):
        e = STRUCT_ENDIAN_DICT[self.endian]
        header = struct.pack(e + 'HH', TLV_INFO_MAGIC, TLV_INFO_SIZE + len(self.buf))
        return header + bytes(self.buf)


class Image():

    def __init__(self, version=None, header_size=IMAGE_HEADER_SIZE,
                 dependencies={}, pad_header=False, pad=False, align=1,
                 slot_size=0, max_sectors=DEFAULT_MAX_SECTORS,
                 overwrite_only=False, endian="little"):
        self.version = version or versmod.decode_version("0")
        self.header_size = header_size
        self.dependencies = dependencies
        self.pad_header = pad_header
        self.pad = pad
        self.align = align
        self.slot_size = slot_size
        self.max_sectors = max_sectors
        self.overwrite_only = overwrite_only
        self.endian = endian
        self.base_addr = None
        self.payload = []

    def __repr__(self):
        return "<Image version={}, header_size={}, dependencies={}, \
                base_addr={}, align={}, slot_size={}, max_sectors={}, \
                overwrite_only={}, endian={} format={}, \
                payloadlen=0x{:x}>".format(
                    self.version,
                    self.header_size,
                    self.dependencies,
                    self.base_addr if self.base_addr is not None else "N/A",
                    self.align,
                    self.slot_size,
                    self.max_sectors,
                    self.overwrite_only,
                    self.endian,
                    self.__class__.__name__,
                    len(self.payload))

    def load(self, path):
        """Load an image from a given file"""
        ext = os.path.splitext(path)[1][1:].lower()
        if ext == INTEL_HEX_EXT:
            ih = IntelHex(path)
            self.payload = ih.tobinarray()
            self.base_addr = ih.minaddr()
        else:
            with open(path, 'rb') as f:
                self.payload = f.read()

        # Add the image header if needed.
        if self.pad_header and self.header_size > 0:
            if self.base_addr:
                # Adjust base_addr for new header
                self.base_addr -= self.header_size
            self.payload = (b'\000' * self.header_size) + self.payload

        self.check()

    def save(self, path):
        """Save an image from a given file"""
        if self.pad:
            self.pad_to(self.slot_size)

        ext = os.path.splitext(path)[1][1:].lower()
        if ext == INTEL_HEX_EXT:
            # input was in binary format, but HEX needs to know the base addr
            if self.base_addr is None:
                raise Exception("Input file does not provide a base address")
            h = IntelHex()
            h.frombytes(bytes=self.payload, offset=self.base_addr)
            h.tofile(path, 'hex')
        else:
            with open(path, 'wb') as f:
                f.write(self.payload)

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

    def create(self, key, enckey, dependencies={}):
        if dependencies == {}:
            dependencies_num = 0
            protected_tlv_size = 0
        else:
            # Size of a Dependency TLV = Header ('BBH') + Payload('IBBHI')
            # = 16 Bytes
            dependencies_num = len(dependencies["images"])
            protected_tlv_size = (dependencies_num * 16) + TLV_INFO_SIZE

        self.add_header(enckey, protected_tlv_size)

        tlv = TLV(self.endian)

        if protected_tlv_size != 0:
            for i in range(0, dependencies_num):
                e = STRUCT_ENDIAN_DICT[self.endian]
                payload = struct.pack(e + 'I'+'BBHI',
                    int(dependencies["images"][i]),
                    dependencies["versions"][i].major,
                    dependencies["versions"][i].minor,
                    dependencies["versions"][i].revision,
                    dependencies["versions"][i].build
                    )
                tlv.add('DEPENDENCY', payload)
            # Full TLV size needs to be calculated in advance, because the
            # header will be protected as well
            tlv_header_size     = 4
            payload_digest_size = 32
            keyhash_size        = 32
            cipherkey_size      = 32

            full_size = TLV_INFO_SIZE + len(tlv.buf) + tlv_header_size + \
                        payload_digest_size
            if key is not None:
                full_size += tlv_header_size + keyhash_size + \
                             tlv_header_size + key.sig_len()
            if enckey is not None:
                full_size += tlv_header_size + cipherkey_size
            tlv_header = struct.pack(e + 'HH', TLV_INFO_MAGIC, full_size)
            self.payload += tlv_header + bytes(tlv.buf)

        # Note that ecdsa wants to do the hashing itself, which means
        # we get to hash it twice.
        sha = hashlib.sha256()
        sha.update(self.payload)
        digest = sha.digest()

        tlv.add('SHA256', digest)

        if key is not None:
            pub = key.get_public_bytes()
            sha = hashlib.sha256()
            sha.update(pub)
            pubbytes = sha.digest()
            tlv.add('KEYHASH', pubbytes)

            sig = key.sign(bytes(self.payload))
            tlv.add(key.sig_tlv(), sig)

        if enckey is not None:
            plainkey = os.urandom(16)
            cipherkey = enckey._get_public().encrypt(
                plainkey, padding.OAEP(
                    mgf=padding.MGF1(algorithm=hashes.SHA256()),
                    algorithm=hashes.SHA256(),
                    label=None))
            tlv.add('ENCRSA2048', cipherkey)

            nonce = bytes([0] * 16)
            cipher = Cipher(algorithms.AES(plainkey), modes.CTR(nonce),
                            backend=default_backend())
            encryptor = cipher.encryptor()
            img = bytes(self.payload[self.header_size:])
            self.payload[self.header_size:] = encryptor.update(img) + \
                                              encryptor.finalize()
        if protected_tlv_size != 0:
            self.payload += tlv.get()[protected_tlv_size:]
        else:
            self.payload += tlv.get()

    def add_header(self, enckey, protected_tlv_size):
        """Install the image header."""
        flags = 0
        if enckey is not None:
            flags |= IMAGE_F['ENCRYPTED']

        e = STRUCT_ENDIAN_DICT[self.endian]
        fmt = (e +
            # type ImageHdr struct {
            'I' +    # Magic    uint32
            'I' +    # LoadAddr uint32
            'H' +    # HdrSz    uint16
            'H' +    # PTLVSz   uint16
            'I' +    # ImgSz    uint32
            'I' +    # Flags    uint32
            'BBHI' + # Vers     ImageVersion
            'I'      # Pad2     uint32
            ) # }
        assert struct.calcsize(fmt) == IMAGE_HEADER_SIZE
        header = struct.pack(fmt,
                IMAGE_MAGIC,
                0, # LoadAddr
                self.header_size,
                protected_tlv_size, # TLV Info header + Dependency TLVs
                len(self.payload) - self.header_size, # ImageSz
                flags, # Flags
                self.version.major,
                self.version.minor or 0,
                self.version.revision or 0,
                self.version.build or 0,
                0) # Pad2
        self.payload = bytearray(self.payload)
        self.payload[:len(header)] = header

    def _trailer_size(self, write_size, max_sectors, overwrite_only):
        # NOTE: should already be checked by the argument parser
        if overwrite_only:
            return 8 * 2 + 16
        else:
            if write_size not in set([1, 2, 4, 8]):
                raise Exception("Invalid alignment: {}".format(write_size))
            m = DEFAULT_MAX_SECTORS if max_sectors is None else max_sectors
            return m * 3 * write_size + 8 * 2 + 16

    def pad_to(self, size):
        """Pad the image to the given size, with the given flash alignment."""
        tsize = self._trailer_size(self.align, self.max_sectors,
                                   self.overwrite_only)
        padding = size - (len(self.payload) + tsize)
        pbytes  = b'\xff' * padding
        pbytes += b'\xff' * (tsize - len(boot_magic))
        pbytes += boot_magic
        self.payload += pbytes
