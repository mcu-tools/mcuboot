# Copyright 2023 Arm Limited
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
from imgtool import image
import click
import struct
import yaml
import os.path
import sys

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


def print_tlv_records(tlv_list):
    indent = _LINE_LENGTH // 8
    for tlv in tlv_list:
        print(" " * indent, "-" * 45)
        tlv_type, tlv_length, tlv_data = tlv.keys()

        print(" " * indent, "{}: {} ({})".format(
                tlv_type, TLV_TYPES[tlv[tlv_type]], hex(tlv[tlv_type])))
        print(" " * indent, "{}: ".format(tlv_length), hex(tlv[tlv_length]))
        print(" " * indent, "{}: ".format(tlv_data), end="")

        for j, data in enumerate(tlv[tlv_data]):
            print("{0:#04x}".format(data),  end=" ")
            if ((j+1) % 8 == 0) and ((j+1) != len(tlv[tlv_data])):
                print("\n", end=" " * (indent+7))
        print()


def dump_imginfo(imgfile, outfile=None, silent=False):
    '''Parse a signed image binary and print/save the available information.'''
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
                "tlvs_prot":    [],
                "tlv_hdr":      {},
                "tlvs":         []}
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
        trailer = {}
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
            key_field_len = None
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
    str1 = "#### Image header (offset: 0x0) "
    str2 = "#" * (_LINE_LENGTH - len(str1))
    print(str1 + str2)
    for key, value in header.items():
        if key == "flags":
            if not value:
                flag_string = hex(value)
            else:
                flag_string = ""
                for flag in image.IMAGE_F.keys():
                    if (value & image.IMAGE_F[flag]):
                        if flag_string:
                            flag_string += ("\n" + (" " * 20))
                        flag_string += "{} ({})".format(
                                    flag, hex(image.IMAGE_F[flag]))
            value = flag_string

        if type(value) != str:
            value = hex(value)
        print(key, ":", " " * (19-len(key)), value, sep="")
    print("#" * _LINE_LENGTH)

    # Image payload
    _sectionoff = header["hdr_size"]
    sepc = " "
    str1 = "#### Payload (offset: {}) ".format(hex(_sectionoff))
    str2 = "#" * (_LINE_LENGTH - len(str1))
    print(str1 + str2)
    print("|", sepc * (_LINE_LENGTH - 2), "|", sep="")
    str1 = "FW image (size: {} Bytes)".format(hex(header["img_size"]))
    numc = (_LINE_LENGTH - len(str1)) // 2
    str2 = "|" + (sepc * (numc - 1))
    str3 = sepc * (_LINE_LENGTH - len(str2) - len(str1) - 1) + "|"
    print(str2, str1, str3, sep="")
    print("|", sepc * (_LINE_LENGTH - 2), "|", sep="")
    print("#" * _LINE_LENGTH)

    # TLV area
    _sectionoff += header["img_size"]
    if protected_tlv_size != 0:
        # Protected TLV area
        str1 = "#### Protected TLV area (offset: {}) ".format(hex(_sectionoff))
        str2 = "#" * (_LINE_LENGTH - len(str1))
        print(str1 + str2)
        print("magic:    ", hex(tlv_area["tlv_hdr_prot"]["magic"]))
        print("area size:", hex(tlv_area["tlv_hdr_prot"]["tlv_tot"]))
        print_tlv_records(tlv_area["tlvs_prot"])
        print("#" * _LINE_LENGTH)

    _sectionoff += protected_tlv_size
    str1 = "#### TLV area (offset: {}) ".format(hex(_sectionoff))
    str2 = "#" * (_LINE_LENGTH - len(str1))
    print(str1 + str2)
    print("magic:    ", hex(tlv_area["tlv_hdr"]["magic"]))
    print("area size:", hex(tlv_area["tlv_hdr"]["tlv_tot"]))
    print_tlv_records(tlv_area["tlvs"])
    print("#" * _LINE_LENGTH)

    if _img_pad_size:
        _sectionoff += tlv_area["tlv_hdr"]["tlv_tot"]
        _erased_val = b[_sectionoff]
        str1 = "#### Image padding (offset: {}) ".format(hex(_sectionoff))
        str2 = "#" * (_LINE_LENGTH - len(str1))
        print(str1 + str2)
        print("|", sepc * (_LINE_LENGTH - 2), "|", sep="")
        str1 = "padding ({})".format(hex(_erased_val))
        numc = (_LINE_LENGTH - len(str1)) // 2
        str2 = "|" + (sepc * (numc - 1))
        str3 = sepc * (_LINE_LENGTH - len(str2) - len(str1) - 1) + "|"
        print(str2, str1, str3, sep="")
        print("|", sepc * (_LINE_LENGTH - 2), "|", sep="")
        print("#" * _LINE_LENGTH)

        # Image trailer
        str1 = "#### Image trailer (offset: unknown) "
        str2 = "#" * (_LINE_LENGTH - len(str1))
        print(str1 + str2)
        print("(Note: some field may not be used, \n"
              " depending on the update strategy)\n")

        print("swap status: (len: unknown)")
        if key_field_len is not None:
            print("enc. keys:   (len: {}, if BOOT_SWAP_SAVE_ENCTLV is unset)"
                  .format(hex(key_field_len)))
        print("swap size:  ", hex(swap_size))
        print("swap_info:  ", hex(swap_info))
        print("copy_done:  ", hex(copy_done))
        print("image_ok:   ", hex(image_ok))
        print("boot magic:  ", end="")
        for i in range(BOOT_MAGIC_SIZE):
            print("{0:#04x}".format(trailer_magic[i]),  end=" ")
            if (i == (BOOT_MAGIC_SIZE/2 - 1)):
                print("\n", end="             ")
        print()

    str1 = "#### End of Image "
    str2 = "#" * (_LINE_LENGTH - len(str1))
    print(str1 + str2)
