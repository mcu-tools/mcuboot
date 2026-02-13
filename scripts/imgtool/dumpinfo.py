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
import json
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
        return f"(len: {hex(key_field_len)}, if BOOT_SWAP_SAVE_ENCTLV is unset)"
    else:
        return "Image not encrypted"


def parse_size(size_hex):
    if size_hex == '0xffffffff':
        return "unknown"
    return size_hex + " octal: " + str(int(size_hex, 0))


def parse_status(status_hex):
    return (
        f"{STATUS[status_hex]} ({status_hex})"
        if status_hex in STATUS
        else f"INVALID ({status_hex})"
    )


def parse_boot_magic(trailer_magic):
    magic = ""
    for i in range(BOOT_MAGIC_SIZE):
        magic += f"{trailer_magic[i]:#04x} "
        if i == (BOOT_MAGIC_SIZE / 2 - 1):
            magic += ("\n" + "             ")
    return magic


def _human_format_frame(header_text, content):
    sepc = " "
    header = "#### " + header_text + sepc
    post_header = "#" * (_LINE_LENGTH - len(header))
    lines = []
    lines.append(header + post_header)
    lines.append("|" + sepc * (_LINE_LENGTH - 2) + "|")
    offset = (_LINE_LENGTH - len(content)) // 2
    pre = "|" + (sepc * (offset - 1))
    post = sepc * (_LINE_LENGTH - len(pre) - len(content) - 1) + "|"
    lines.append(pre + content + post)
    lines.append("|" + sepc * (_LINE_LENGTH - 2) + "|")
    lines.append("#" * _LINE_LENGTH)
    return "\n".join(lines)


def _human_format_row(row_text):
    row_text = "#### " + row_text + " "
    fill = "#" * (_LINE_LENGTH - len(row_text))
    return row_text + fill


def _human_format_tlv_records(tlv_list):
    indent = _LINE_LENGTH // 8
    lines = []
    for tlv in tlv_list:
        lines.append(" " * indent + " " + "-" * 45)

        type_name = tlv["type_name"]
        type_hex = hex(tlv["type"])
        lines.append(" " * indent + f" type: {type_name} ({type_hex})")
        lines.append(" " * indent + f" len:  {hex(tlv['len'])}")

        data_line = " " * indent + " data: "
        for j, data in enumerate(tlv["data"]):
            data_line += f"{data:#04x} "
            if ((j + 1) % 8 == 0) and ((j + 1) != len(tlv["data"])):
                lines.append(data_line)
                data_line = " " * (indent + 7)
        lines.append(data_line)
    return "\n".join(lines)


def _read_imginfo(imgfile):
    """Parse a signed image binary and return the image data structure."""
    trailer_magic = None
    #   set to INVALID by default
    swap_size = 0x99
    swap_info = 0x99
    copy_done = 0x99
    image_ok = 0x99
    trailer = {}

    try:
        with open(imgfile, "rb") as f:
            b = f.read()
    except FileNotFoundError:
        raise click.UsageError(f"Image file not found ({imgfile})") from None

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
                {"type": tlv_type,
                 "type_name": TLV_TYPES.get(tlv_type, "UNKNOWN"),
                 "len": tlv_len,
                 "data": tlv_data})
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
            {"type": tlv_type, "type_name": TLV_TYPES.get(tlv_type, "UNKNOWN"),
             "len": tlv_len, "data": tlv_data})
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
                trailer["key_field_len"] = key_field_len

    imginfo = {
            "filename": os.path.basename(imgfile),
            "header": header,
            "tlv_area": tlv_area,
            "trailer": trailer}

    return imginfo


def _write_format_human(imginfo, outfile):
    filename = imginfo["filename"]
    header = imginfo["header"]
    tlv_area = imginfo["tlv_area"]
    trailer  = imginfo["trailer"]

    print("Printing content of signed image:", filename, "\n", file=outfile)

    # Image header
    section_name = "Image header (offset: 0x0)"
    print(_human_format_row(section_name), file=outfile)
    for key, value in header.items():
        if key == "flags":
            if not value:
                flag_string = hex(value)
            else:
                flag_string = ""
                for flag in image.IMAGE_F:
                    if value & image.IMAGE_F[flag]:
                        if flag_string:
                            flag_string += ("\n" + (" " * 20))
                        flag_string += f"{flag} ({hex(image.IMAGE_F[flag])})"
            value = flag_string

        if not isinstance(value, str):
            value = hex(value)
        print(key, ":", " " * (19 - len(key)), value, sep="", file=outfile)
    print("#" * _LINE_LENGTH, file=outfile)

    # Image payload
    _sectionoff = header["hdr_size"]
    frame_header_text = f"Payload (offset: {hex(_sectionoff)})"
    frame_content = "FW image (size: {} Bytes)".format(hex(header["img_size"]))
    print(_human_format_frame(frame_header_text, frame_content), file=outfile)

    # TLV area
    _sectionoff += header["img_size"]
    protected_tlv_size = header["protected_tlv_size"]
    if protected_tlv_size != 0:
        # Protected TLV area
        section_name = f"Protected TLV area (offset: {hex(_sectionoff)})"
        print(_human_format_row(section_name), file=outfile)
        print("magic:    ", hex(tlv_area["tlv_hdr_prot"]["magic"]), file=outfile)
        print("area size:", hex(tlv_area["tlv_hdr_prot"]["tlv_tot"]), file=outfile)
        print(_human_format_tlv_records(tlv_area["tlvs_prot"]), file=outfile)
        print("#" * _LINE_LENGTH, file=outfile)

    _sectionoff += protected_tlv_size
    section_name = f"TLV area (offset: {hex(_sectionoff)})"
    print(_human_format_row(section_name), file=outfile)
    print("magic:    ", hex(tlv_area["tlv_hdr"]["magic"]), file=outfile)
    print("area size:", hex(tlv_area["tlv_hdr"]["tlv_tot"]), file=outfile)
    print(_human_format_tlv_records(tlv_area["tlvs"]), file=outfile)
    print("#" * _LINE_LENGTH, file=outfile)

    # Check if trailer has data (for image padding and trailer info)
    if trailer.get("magic"):
        trailer_magic = trailer["magic"]
        _sectionoff += tlv_area["tlv_hdr"]["tlv_tot"]
        # Note: We don't have access to original binary data here, so skip padding details

        # Image trailer
        section_name = "Image trailer (offset: unknown)"
        print(_human_format_row(section_name), file=outfile)
        notice = "(Note: some fields may not be used, depending on the update strategy)\n"
        notice = '\n'.join(notice[i:i + _LINE_LENGTH] for i in range(0, len(notice), _LINE_LENGTH))
        print(notice, file=outfile)
        print("swap status: (len: unknown)", file=outfile)
        print("enc. keys:  ", parse_enc(trailer.get("key_field_len")), file=outfile)

        # Only print trailer fields if they exist
        if "swap_size" in trailer:
            print("swap size:  ", parse_size(hex(trailer["swap_size"])), file=outfile)
        if "swap_info" in trailer:
            print("swap_info:  ", parse_status(hex(trailer["swap_info"])), file=outfile)
        if "copy_done" in trailer:
            print("copy_done:  ", parse_status(hex(trailer["copy_done"])), file=outfile)
        if "image_ok" in trailer:
            print("image_ok:   ", parse_status(hex(trailer["image_ok"])), file=outfile)
        print("boot magic: ", parse_boot_magic(trailer_magic), file=outfile)
        print(file=outfile)

    footer = "End of Image "
    print(_human_format_row(footer), file=outfile)

def _json_default_serializer(obj):
    """Convert non-JSON-serializable objects to JSON-serializable format."""
    if isinstance(obj, (bytes, bytearray)):
        return obj.hex()
    raise TypeError(f"Object of type {type(obj).__name__} is not JSON serializable")


def _write_format(imginfo, output_format, out):
    """Write image info in the specified format to the output stream."""
    if output_format == 'human':
        _write_format_human(imginfo, out)

    elif output_format == 'yaml':
        yaml.dump(imginfo, out, sort_keys=False)

    elif output_format == 'json':
        json.dump(imginfo, out, indent=2, default=_json_default_serializer)
        if out == sys.stdout:
            print()  # Add newline after JSON output to stdout
    else:
        raise ValueError(f"Invalid output format: {output_format}")


def dump_imginfo(imgfile, outfile=None, output_format=None, silent=False):
    """Parse a signed image binary and print/save the available information."""

    # Note: silent parameter is kept for backward compatibility but is ignored.
    # The function's purpose is to output data, so silent doesn't make sense here.

    # Determine output format based on backward compatibility rules
    if output_format is None:
        if outfile is None:
            output_format = 'human'  # no --outfile defaults to human-friendly
        else:
            output_format = 'yaml'  # --outfile without --format defaults to yaml

    imginfo = _read_imginfo(imgfile)

    if outfile:
        with open(outfile, "w") as out:
            _write_format(imginfo, output_format, out)
    else:
        _write_format(imginfo, output_format, sys.stdout)

