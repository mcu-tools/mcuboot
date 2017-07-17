#! /usr/bin/env python3

"""
Assemble multiple images into a single image that can be flashed on the device.
"""

import argparse
import io
import re
import os.path

def same_keys(a, b):
    """Determine if the dicts a and b have the same keys in them"""
    for ak in a.keys():
        if ak not in b:
            return False
    for bk in b.keys():
        if bk not in a:
            return False
    return True

offset_re = re.compile(r"^#define FLASH_AREA_([0-9A-Z_]+)_OFFSET_0\s+((0x)?[0-9a-fA-F]+)")
size_re   = re.compile(r"^#define FLASH_AREA_([0-9A-Z_]+)_SIZE_0\s+((0x)?[0-9a-fA-F]+)")

class Assembly():
    def __init__(self, output, bootdir):
        self.find_slots(bootdir)
        os.unlink(output)
        self.output = output

    def find_slots(self, bootdir):
        offsets = {}
        sizes = {}
        with open(os.path.join(bootdir, 'include', 'generated', 'generated_dts_board.h'), 'r') as fd:
            for line in fd:
                m = offset_re.match(line)
                if m is not None:
                    offsets[m.group(1)] = int(m.group(2), 0)
                m = size_re.match(line)
                if m is not None:
                    sizes[m.group(1)] = int(m.group(2), 0)

        if not same_keys(offsets, sizes):
            raise Exception("Inconsistent data in generated_dts_board.h")

        # We care about the MCUBOOT, IMAGE_0, and IMAGE_1 partitions.
        if 'MCUBOOT' not in offsets:
            raise Exception("Board partition table does not have mcuboot partition")

        if 'IMAGE_0' not in offsets:
            raise Exception("Board partition table does not have image-0 partition")

        if 'IMAGE_1' not in offsets:
            raise Exception("Board partition table does not have image-1 partition")

        self.offsets = offsets
        self.sizes = sizes

    def add_image(self, source, partition):
        with open(self.output, 'ab') as ofd:
            pos = ofd.tell()
            print("partition {}, pos={}, offset={}".format(partition, pos, self.offsets[partition]))
            if pos > self.offsets[partition]:
                raise Exception("Partitions not in order, unsupported")
            if pos < self.offsets[partition]:
                buf = b'\xFF' * (self.offsets[partition] - pos)
                ofd.write(buf)
            with open(source, 'rb') as rfd:
                ibuf = rfd.read()
                if len(ibuf) > self.sizes[partition]:
                    raise Exception("Image {} is too large for partition".format(source))
            ofd.write(ibuf)

def main():
    parser = argparse.ArgumentParser()

    parser.add_argument('-b', '--bootdir', required=True,
            help='Directory of built bootloader')
    parser.add_argument('-p', '--primary', required=True,
            help='Signed image file for primary image')
    parser.add_argument('-s', '--secondary',
            help='Signed image file for secondary image')
    parser.add_argument('-o', '--output', required=True,
            help='Filename to write full image to')

    args = parser.parse_args()
    output = Assembly(args.output, args.bootdir)

    output.add_image(os.path.join(args.bootdir, "zephyr.bin"), 'MCUBOOT')
    output.add_image(args.primary, "IMAGE_0")
    if args.secondary is not None:
        output.add_image(args.secondary, "IMAGE_1")

if __name__ == '__main__':
    main()
