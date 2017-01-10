#!/usr/bin/python2

from __future__ import print_function

import mmap
import os
import struct
import sys
from argparse import ArgumentParser
import newtimg
from ctypes import *
from Crypto.Signature import PKCS1_v1_5
from Crypto.Hash import SHA256
from Crypto.PublicKey import RSA

DEBUG = False

def get_args():
    parser = ArgumentParser(description='Script to create images on a format \
                            that Mynewts bootloader expects')

    parser.add_argument('--bin', required=True, dest='binary_file', \
                        help='Name of *.bin file (input)')

    parser.add_argument('--key', required=False, dest='key_file', \
                        help='Name of private key file (*.pem format)')

    parser.add_argument('--out', required=False, dest='image_file', \
                        default='zephyr.img.bin', \
                        help='Name of *.img file (output)')

    parser.add_argument('--sig', required=False, dest='sig_type', \
                        default='SHA256', \
                        help='Type of signature <SHA256|RSA|EC>')

    parser.add_argument('--off', required=False, dest='flash_offs_addr', \
                        default='0x08020000', \
                        help='Offset for the binary in flash (at what address \
                        should it be flashed?)')

    parser.add_argument('--word-size', required=False, dest='word_size',
            default=1,
            help='Writable size of flash device (1, 2, 4, or 8)')

    parser.add_argument('--vtoff', required=False, dest='vtable_offs', \
                        default=str(hex(newtimg.OFFSET_VECTOR_TABLE)), \
                        help='Offset to vector table in HEX (default: 0x80)')

    parser.add_argument('--pad', required=False, \
                        help='Pad file with 0xff up to this size (in hex)')

    parser.add_argument('--bit', required=False, action="store_true", \
                        default=False, \
                        help='Whether to add the Boot Image Trailer to the \
                        padded image or not (default: False)')

    parser.add_argument('--verbose', required=False, action="store_true", \
                        default=False, \
                        help='Enable verbose mode')

    parser.add_argument('--version', action='version', version='%(prog)s 1.0')

    parser.add_argument('-f', required=False, action="store_true", \
                        default=False, \
                        help='Flash using JLinkExe')

    return parser.parse_args()

def trailer_size(word_size):
    """
    Compute the size of the image trailer.

    The image trailer size depends on the writable unit size of the
    flash in question.  Given a word size of 1, 2, 4 or 8, return the
    size, in bytes, of the image trailer.  The magic number should be
    placed at this particular offset from the end of the segment
    """
    sizes = { 1: 402, 2: 788, 4: 1560, 8: 3104 }
    return sizes[word_size]

class Signature(object):
    """
    Sign an image appropriately.
    """

    def compute(self, payload, key_file):
        # Base computes sha256.
        ctx = SHA256.new()
        ctx.update(payload)
        self.hash = ctx.digest()
        self.ctx = ctx

    def get_trailer(self):
        return struct.pack('bxh32s', newtimg.IMAGE_TLV_SHA256,
                len(self.hash),
                self.hash)

    def trailer_len(self):
        return 32 + 4

    def get_flags(self):
        return newtimg.IMAGE_F_SHA256

class RSASignature(Signature):

    def compute(self, payload, key_file):
        super(RSASignature, self).compute(payload, key_file)
        with open(key_file, 'rb') as f:
            rsa_key = RSA.importKey(f.read())
        rsa = PKCS1_v1_5.new(rsa_key)
        self.signature = rsa.sign(self.ctx)

    def trailer_len(self):
        return super(RSASignature, self).trailer_len() + newtimg.RSA_SIZE

    def get_trailer(self):
        buf = bytearray(super(RSASignature, self).get_trailer())
        buf.extend(struct.pack('bxh', newtimg.IMAGE_TLV_RSA2048,
                newtimg.RSA_SIZE))
        buf.extend(self.signature)
        return buf

    def get_flags(self):
        return newtimg.IMAGE_F_PKCS15_RSA2048_SHA256 | newtimg.IMAGE_F_SHA256

sigs = {
        'SHA256': Signature,
        'RSA': RSASignature,
        }

class Convert():
    def __init__(self, args):
        self.args = args
        if args.verbose:
            for a in vars(args):
                print("Arg -> {}: {}".format(a, getattr(args, a)))
            self.debug = True
        else:
            self.debug = False

        self.vtable_offs = int(args.vtable_offs, 16)
        self.word_size = int(args.word_size)

        self.load_image(args.binary_file)
        self.validate_header()

        sig = sigs[args.sig_type]()
        header = self.make_header(sig)
        assert len(header) == 32
        self.image[:len(header)] = header

        sig.compute(self.image, args.key_file)
        self.trailer = sig.get_trailer()

        self.image.extend(self.trailer)

        if args.bit:
            self.add_trailer(args.pad)

        self.save_image(args.image_file)

    def load_image(self, name):
        with open(name, 'rb') as f:
            image = f.read()
        self.image = bytearray(image)

    def save_image(self, name):
        with open(name, 'wb') as f:
            f.write(self.image)

    def validate_header(self):
        """Ensure that the image has space for a header

        If the image is build with CONFIG_BOOT_HEADER off, the vector
        table will be at the beginning, rather than the zero padding.
        Verify that the padding is present.
        """
        if self.image[:self.vtable_offs] != bytearray(self.vtable_offs):
            raise Exception("Image does not have space for header")

    def make_header(self, sig):
        image_size = len(self.image) - self.vtable_offs
        tlv_size = sig.trailer_len()
        key_id = 0
        hd = struct.pack('IHBxHxxIIBBHI4x',
                newtimg.IMAGE_MAGIC,
                tlv_size,
                key_id,
                self.vtable_offs,
                image_size,
                sig.get_flags(),
                1, 0, 0, 0)
        return hd

    def add_trailer(self, pad):
        """
        Add the image trailer, to indicate to the bootloader that this
        image should be flashed
        """
        if not pad:
            raise Exception("Must specify image length with --pad to use --bit")
        pad = int(pad, 16)

        if len(self.image) > pad:
            raise Exception("Image is too large for padding")

        self.image.extend(b'\xFF' * (pad - len(self.image)))

        magic = struct.pack('4I', *newtimg.BOOT_IMG_MAGIC)
        pos = pad - trailer_size(self.word_size)
        self.image[pos:pos + len(magic)] = magic

def main(argv):
    args = get_args()
    erase = False

    conv = Convert(args)

if __name__ == "__main__":
    main(sys.argv)
