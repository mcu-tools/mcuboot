# Copyright (c) 2020 Arm Limited
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

import argparse
import logging
import struct
import sys

from imgtool.image import (IMAGE_HEADER_SIZE, IMAGE_MAGIC,
                           TLV_INFO_MAGIC, TLV_PROT_INFO_MAGIC, TLV_VALUES)
from shutil import copyfile


def get_tlv_type_string(tlv_type):
    tlvs = {v: f"IMAGE_TLV_{k}" for k, v in TLV_VALUES.items()}
    return tlvs.get(tlv_type, "UNKNOWN({:d})".format(tlv_type))


class ImageHeader:

    def __init__(self):
        self.ih_magic = 0
        self.ih_load_addr = 0
        self.ih_hdr_size = 0
        self.ih_protect_tlv_size = 0
        self.ih_img_size = 0
        self.ih_flags = 0
        self.iv_major = 0
        self.iv_minor = 0
        self.iv_revision = 0
        self.iv_build_num = 0
        self._pad1 = 0

    @staticmethod
    def read_from_binary(in_file):
        h = ImageHeader()

        (h.ih_magic, h.ih_load_addr, h.ih_hdr_size, h.ih_protect_tlv_size, h.ih_img_size,
            h.ih_flags, h.iv_major, h.iv_minor, h.iv_revision, h.iv_build_num, h._pad1
         ) = struct.unpack('<IIHHIIBBHII', in_file.read(IMAGE_HEADER_SIZE))
        return h

    def __repr__(self):
        return "\n".join([
            "    ih_magic = 0x{:X}".format(self.ih_magic),
            "    ih_load_addr = " + str(self.ih_load_addr),
            "    ih_hdr_size = " + str(self.ih_hdr_size),
            "    ih_protect_tlv_size = " + str(self.ih_protect_tlv_size),
            "    ih_img_size = " + str(self.ih_img_size),
            "    ih_flags = " + str(self.ih_flags),
            "    iv_major = " + str(self.iv_major),
            "    iv_minor = " + str(self.iv_minor),
            "    iv_revision = " + str(self.iv_revision),
            "    iv_build_num = " + str(self.iv_build_num),
            "    _pad1 = " + str(self._pad1)])


class ImageTLVInfo:
    def __init__(self):
        self.format_string = '<HH'

        self.it_magic = 0
        self.it_tlv_tot = 0

    @staticmethod
    def read_from_binary(in_file):
        i = ImageTLVInfo()

        (i.it_magic, i.it_tlv_tot) = struct.unpack('<HH', in_file.read(4))
        return i

    def __repr__(self):
        return "\n".join([
            "    it_magic = 0x{:X}".format(self.it_magic),
            "    it_tlv_tot = " + str(self.it_tlv_tot)])

    def __len__(self):
        return struct.calcsize(self.format_string)


class ImageTLV:
    def __init__(self):
        self.it_value = 0
        self.it_type = 0
        self.it_len = 0

    @staticmethod
    def read_from_binary(in_file):
        tlv = ImageTLV()
        (tlv.it_type, _, tlv.it_len) = struct.unpack('<BBH', in_file.read(4))
        (tlv.it_value) = struct.unpack('<{:d}s'.format(tlv.it_len), in_file.read(tlv.it_len))
        return tlv

    def __len__(self):
        round_to = 1
        return int((4 + self.it_len + round_to - 1) // round_to) * round_to


def get_arguments():
    parser = argparse.ArgumentParser(description='Corrupt an MCUBoot image')
    parser.add_argument("-i", "--in-file", required=True, help='The input image to be corrupted (read only)')
    parser.add_argument("-o", "--out-file", required=True, help='the corrupted image')
    parser.add_argument('-a', '--image-hash',
                        default=False,
                        action="store_true",
                        required=False,
                        help='Corrupt the image hash')
    parser.add_argument('-s', '--signature',
                        default=False,
                        action="store_true",
                        required=False,
                        help='Corrupt the signature of the image')
    return parser.parse_args()


def damage_tlv(image_offset, tlv_off, tlv, out_file_content):
    damage_offset = image_offset + tlv_off + 4
    logging.info("        Damaging TLV at offset 0x{:X}...".format(damage_offset))
    value = bytearray(tlv.it_value[0])
    value[0] = (value[0] + 1) % 256
    out_file_content[damage_offset] = value[0]


def is_valid_signature(tlv):
    return tlv.it_type == TLV_VALUES['RSA2048'] or tlv.it_type == TLV_VALUES['RSA3072']


def damage_image(args, in_file, out_file_content, image_offset):
    in_file.seek(image_offset, 0)

    # Find the Image header
    image_header = ImageHeader.read_from_binary(in_file)
    if image_header.ih_magic != IMAGE_MAGIC:
        raise Exception("Invalid magic in image_header: 0x{:X} instead of 0x{:X}".format(image_header.ih_magic, IMAGE_MAGIC))

    # Find the TLV header
    tlv_info_offset = image_header.ih_hdr_size + image_header.ih_img_size
    in_file.seek(image_offset + tlv_info_offset, 0)

    tlv_info = ImageTLVInfo.read_from_binary(in_file)
    if tlv_info.it_magic == TLV_PROT_INFO_MAGIC:
        logging.debug("Protected TLV found at offset 0x{:X}".format(tlv_info_offset))
        if image_header.ih_protect_tlv_size != tlv_info.it_tlv_tot:
            raise Exception("Invalid prot TLV len ({:d} vs. {:d})".format(image_header.ih_protect_tlv_size, tlv_info.it_tlv_tot))

        # seek to unprotected TLV
        tlv_info_offset += tlv_info.it_tlv_tot
        in_file.seek(image_offset + tlv_info_offset)
        tlv_info = ImageTLVInfo.read_from_binary(in_file)

    else:
        if image_header.ih_protect_tlv_size != 0:
            raise Exception("No prot TLV was found.")

    logging.debug("Unprotected TLV found at offset 0x{:X}".format(tlv_info_offset))
    if tlv_info.it_magic != TLV_INFO_MAGIC:
        raise Exception("Invalid magic in tlv info: 0x{:X} instead of 0x{:X}".format(tlv_info.it_magic, TLV_INFO_MAGIC))

    tlv_off = tlv_info_offset + len(ImageTLVInfo())
    tlv_end = tlv_info_offset + tlv_info.it_tlv_tot

    # iterate over the TLV entries
    while tlv_off < tlv_end:
        in_file.seek(image_offset + tlv_off, 0)
        tlv = ImageTLV.read_from_binary(in_file)

        logging.debug("    tlv {:24s} len = {:4d}, len = {:4d}".format(get_tlv_type_string(tlv.it_type), tlv.it_len, len(tlv)))

        if is_valid_signature(tlv) and args.signature:
            damage_tlv(image_offset, tlv_off, tlv, out_file_content)
        elif tlv.it_type == TLV_VALUES['SHA256'] and args.image_hash:
            damage_tlv(image_offset, tlv_off, tlv, out_file_content)

        tlv_off += len(tlv)


def main():
    args = get_arguments()

    logging.debug("The script was started")

    copyfile(args.in_file, args.out_file)
    in_file = open(args.in_file, 'rb')

    out_file_content = bytearray(in_file.read())

    damage_image(args, in_file, out_file_content, 0)

    in_file.close()

    file_to_damage = open(args.out_file, 'wb')
    file_to_damage.write(out_file_content)
    file_to_damage.close()


if __name__ == "__main__":
    logging.basicConfig(format='%(levelname)5s: %(message)s',
                        level=logging.DEBUG, stream=sys.stdout)

    main()
