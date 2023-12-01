# Copyright 2023-2024 Arm Limited
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
Parse and print header, TLV area and trailer information of a signed image.
"""
import os.path
import struct
import sys

import click
import yaml

from imgtool import image

HEADER_ITEMS = ("magic", "load_addr", "hdr_size", "protected_tlv_size",
                "img_size", "flags", "version")
TLV_TYPES = dict((value, key) for key, value in image.TLV_VALUES.items())
BOOT_MAGIC = bytes([
    0x77, 0xc2, 0x95, 0xf3,
    0x60, 0xd2, 0xef, 0x7f,
    0x35, 0x52, 0x50, 0x0f,
    0x2c, 0xb6, 0x79, 0x80, ])
BOOT_MAGIC_2 = bytes([
    0x2d, 0xe1, 0x5d, 0x29,
    0x41, 0x0b, 0x8d, 0x77,
    0x67, 0x9c, 0x11, 0x0f,
    0x1f, 0x8a, ])
BOOT_MAGIC_SIZE = len(BOOT_MAGIC)
_LINE_LENGTH = 60
STATUS = {
    '0x1': 'SET',
    '0x2': 'BAD',
    '0x3': 'UNSET',
    '0x4': 'ANY',
}


def parse_enc(key_field_len):
    if key_field_len is not None:
        return "(len: {}, if BOOT_SWAP_SAVE_ENCTLV is unset)".format(hex(key_field_len))
    else:
        return "Image not encrypted"


def parse_size(size_hex):
    if size_hex == '0xffffffff':
        return "unknown"
    return size_hex + " octal: " + str(int(size_hex, 0))


def parse_status(status_hex):
    return f"{STATUS[status_hex]} ({status_hex})" if status_hex in STATUS else f"INVALID ({status_hex})"


def parse_boot_magic(trailer_magic):
    magic = ""
    for i in range(BOOT_MAGIC_SIZE):
        magic += "{0:#04x} ".format(trailer_magic[i])
        if i == (BOOT_MAGIC_SIZE / 2 - 1):
            magic += ("\n" + "             ")
    return magic


def print_in_frame(header_text, content):
    sepc = " "
    header = "#### " + header_text + sepc
    post_header = "#" * (_LINE_LENGTH - len(header))
    print(header + post_header)

    print("|", sepc * (_LINE_LENGTH - 2), "|", sep="")
    offset = (_LINE_LENGTH - len(content)) // 2
    pre = "|" + (sepc * (offset - 1))
    post = sepc * (_LINE_LENGTH - len(pre) - len(content) - 1) + "|"
    print(pre, content, post, sep="")
    print("|", sepc * (_LINE_LENGTH - 2), "|", sep="")
    print("#" * _LINE_LENGTH)


def print_in_row(row_text):
    row_text = "#### " + row_text + " "
    fill = "#" * (_LINE_LENGTH - len(row_text))
    print(row_text + fill)


def print_tlv_records(tlv_list):
    indent = _LINE_LENGTH // 8
    for tlv in tlv_list:
        print(" " * indent, "-" * 45)
        tlv_type, tlv_length, tlv_data = tlv.keys()

        if tlv[tlv_type] in TLV_TYPES:
            print(" " * indent, "{}: {} ({})".format(
                tlv_type, TLV_TYPES[tlv[tlv_type]], hex(tlv[tlv_type])))
        else:
            print(" " * indent, "{}: {} ({})".format(
                tlv_type, "UNKNOWN", hex(tlv[tlv_type])))
        print(" " * indent, "{}: ".format(tlv_length), hex(tlv[tlv_length]))
        print(" " * indent, "{}: ".format(tlv_data), end="")

        for j, data in enumerate(tlv[tlv_data]):
            print("{0:#04x}".format(data), end=" ")
            if ((j + 1) % 8 == 0) and ((j + 1) != len(tlv[tlv_data])):
                print("\n", end=" " * (indent + 7))
        print()


def dump_imginfo(imgfile, outfile=None, silent=False):
    """Parse a signed image binary and print/save the available information."""
    trailer_magic = None
    #   set to INVALID by default
    swap_size = 0x99
    swap_info = 0x99
    copy_done = 0x99
    image_ok = 0x99
    trailer = {}
    key_field_len = None

    try:
        with open(imgfile, "rb") as f:
            b = f.read()
    except FileNotFoundError:
        raise click.UsageError("Image file not found ({})".format(imgfile))

    # Parsing the image header
    _header = struct.unpack('IIHHIIBBHI', b[:28])
    # Image version consists of the last 4 item ('BBHI')
    _version = _header[-4:]
    header = {}
    for i, key in enumerate(HEADER_ITEMS):
        if key == "version":
            header[key] = "{}.{}.{}+{}".format(*_version)
        else:
            header[key] = _header[i]

    # Parsing the TLV area
    tlv_area = {"tlv_hdr_prot": {},
                "tlvs_prot": [],
                "tlv_hdr": {},
                "tlvs": []}
    tlv_off = header["hdr_size"] + header["img_size"]
    protected_tlv_size = header["protected_tlv_size"]

    if protected_tlv_size != 0:
        _tlv_prot_head = struct.unpack(
            'HH',
            b[tlv_off:(tlv_off + image.TLV_INFO_SIZE)])
        tlv_area["tlv_hdr_prot"]["magic"] = _tlv_prot_head[0]
        tlv_area["tlv_hdr_prot"]["tlv_tot"] = _tlv_prot_head[1]
        tlv_end = tlv_off + tlv_area["tlv_hdr_prot"]["tlv_tot"]
        tlv_off += image.TLV_INFO_SIZE

        # Iterating through the protected TLV area
        while tlv_off < tlv_end:
            tlv_type, tlv_len = struct.unpack(
                'HH',
                b[tlv_off:(tlv_off + image.TLV_INFO_SIZE)])
            tlv_off += image.TLV_INFO_SIZE
            tlv_data = b[tlv_off:(tlv_off + tlv_len)]
            tlv_area["tlvs_prot"].append(
                {"type": tlv_type, "len": tlv_len, "data": tlv_data})
            tlv_off += tlv_len

    _tlv_head = struct.unpack('HH', b[tlv_off:(tlv_off + image.TLV_INFO_SIZE)])
    tlv_area["tlv_hdr"]["magic"] = _tlv_head[0]
    tlv_area["tlv_hdr"]["tlv_tot"] = _tlv_head[1]

    tlv_end = tlv_off + tlv_area["tlv_hdr"]["tlv_tot"]
    tlv_off += image.TLV_INFO_SIZE

    # Iterating through the TLV area
    while tlv_off < tlv_end:
        tlv_type, tlv_len = struct.unpack(
            'HH',
            b[tlv_off:(tlv_off + image.TLV_INFO_SIZE)])
        tlv_off += image.TLV_INFO_SIZE
        tlv_data = b[tlv_off:(tlv_off + tlv_len)]
        tlv_area["tlvs"].append(
            {"type": tlv_type, "len": tlv_len, "data": tlv_data})
        tlv_off += tlv_len

    _img_pad_size = len(b) - tlv_end

    if _img_pad_size:
        # Parsing the image trailer
        trailer_off = -BOOT_MAGIC_SIZE
        trailer_magic = b[trailer_off:]
        trailer["magic"] = trailer_magic
        max_align = None
        if trailer_magic == BOOT_MAGIC:
            # The maximum supported write alignment is the default 8 Bytes
            max_align = 8
        elif trailer_magic[-len(BOOT_MAGIC_2):] == BOOT_MAGIC_2:
            # The alignment value is encoded in the magic field
            max_align = int.from_bytes(trailer_magic[:2], "little")
        else:
            # Invalid magic: the rest of the image trailer cannot be processed.
            print("Warning: the trailer magic value is invalid!")

        if max_align is not None:
            if max_align > BOOT_MAGIC_SIZE:
                trailer_off -= max_align - BOOT_MAGIC_SIZE
            # Parsing rest of the trailer fields
            trailer_off -= max_align
            image_ok = b[trailer_off]
            trailer["image_ok"] = image_ok

            trailer_off -= max_align
            copy_done = b[trailer_off]
            trailer["copy_done"] = copy_done

            trailer_off -= max_align
            swap_info = b[trailer_off]
            trailer["swap_info"] = swap_info

            trailer_off -= max_align
            swap_size = int.from_bytes(b[trailer_off:(trailer_off + 4)],
                                       "little")
            trailer["swap_size"] = swap_size

            # Encryption key 0/1
            if ((header["flags"] & image.IMAGE_F["ENCRYPTED_AES128"]) or
                    (header["flags"] & image.IMAGE_F["ENCRYPTED_AES256"])):
                # The image is encrypted
                #    Estimated value of key_field_len is correct if
                #    BOOT_SWAP_SAVE_ENCTLV is unset
                key_field_len = image.align_up(16, max_align) * 2

    # Generating output yaml file
    if outfile is not None:
        imgdata = {"header": header,
                   "tlv_area": tlv_area,
                   "trailer": trailer}
        with open(outfile, "w") as outf:
            # sort_keys - from pyyaml 5.1
            yaml.dump(imgdata, outf, sort_keys=False)

    ###############################################################################

    if silent:
        sys.exit(0)

    print("Printing content of signed image:", os.path.basename(imgfile), "\n")

    # Image header
    section_name = "Image header (offset: 0x0)"
    print_in_row(section_name)
    for key, value in header.items():
        if key == "flags":
            if not value:
                flag_string = hex(value)
            else:
                flag_string = ""
                for flag in image.IMAGE_F.keys():
                    if value & image.IMAGE_F[flag]:
                        if flag_string:
                            flag_string += ("\n" + (" " * 20))
                        flag_string += "{} ({})".format(
                            flag, hex(image.IMAGE_F[flag]))
            value = flag_string

        if not isinstance(value, str):
            value = hex(value)
        print(key, ":", " " * (19 - len(key)), value, sep="")
    print("#" * _LINE_LENGTH)

    # Image payload
    _sectionoff = header["hdr_size"]
    frame_header_text = "Payload (offset: {})".format(hex(_sectionoff))
    frame_content = "FW image (size: {} Bytes)".format(hex(header["img_size"]))
    print_in_frame(frame_header_text, frame_content)

    # TLV area
    _sectionoff += header["img_size"]
    if protected_tlv_size != 0:
        # Protected TLV area
        section_name = "Protected TLV area (offset: {})".format(hex(_sectionoff))
        print_in_row(section_name)
        print("magic:    ", hex(tlv_area["tlv_hdr_prot"]["magic"]))
        print("area size:", hex(tlv_area["tlv_hdr_prot"]["tlv_tot"]))
        print_tlv_records(tlv_area["tlvs_prot"])
        print("#" * _LINE_LENGTH)

    _sectionoff += protected_tlv_size
    section_name = "TLV area (offset: {})".format(hex(_sectionoff))
    print_in_row(section_name)
    print("magic:    ", hex(tlv_area["tlv_hdr"]["magic"]))
    print("area size:", hex(tlv_area["tlv_hdr"]["tlv_tot"]))
    print_tlv_records(tlv_area["tlvs"])
    print("#" * _LINE_LENGTH)

    if _img_pad_size:
        _sectionoff += tlv_area["tlv_hdr"]["tlv_tot"]
        _erased_val = b[_sectionoff]
        frame_header_text = "Image padding (offset: {})".format(hex(_sectionoff))
        frame_content = "padding ({})".format(hex(_erased_val))
        print_in_frame(frame_header_text, frame_content)

        # Image trailer
        section_name = "Image trailer (offset: unknown)"
        print_in_row(section_name)
        notice = "(Note: some fields may not be used, depending on the update strategy)\n"
        notice = '\n'.join(notice[i:i + _LINE_LENGTH] for i in range(0, len(notice), _LINE_LENGTH))
        print(notice)
        print("swap status: (len: unknown)")
        print("enc. keys:  ", parse_enc(key_field_len))
        print("swap size:  ", parse_size(hex(swap_size)))
        print("swap_info:  ", parse_status(hex(swap_info)))
        print("copy_done:  ", parse_status(hex(copy_done)))
        print("image_ok:   ", parse_status(hex(image_ok)))
        print("boot magic: ", parse_boot_magic(trailer_magic))
        print()

    footer = "End of Image "
    print_in_row(footer)
