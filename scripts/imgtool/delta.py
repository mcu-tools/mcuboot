# SPDX-License-Identifier: Apache-2.0

import os.path
import struct
from dataclasses import dataclass

from intelhex import IntelHex

from .image import IMAGE_MAGIC, TLV_INFO_MAGIC, TLV_PROT_INFO_MAGIC, align_up

DELTA_MAGIC = 0x314c444d  # "MDL1"
DELTA_VERSION = 2
DELTA_FLAG_RESTORE = 0x00000001
DELTA_HEADER_SIZE = 32
DELTA_HEADER = '<IHHIIIIII'
DELTA_RECORD = '<II'


@dataclass
class DeltaPayload:
    payload: bytes
    base_hash: bytes
    target_hash: bytes
    target_size: int
    write_size: int
    record_count: int


def load_image_bytes(path):
    ext = os.path.splitext(path)[1][1:].lower()

    if ext == 'hex':
        return bytes(IntelHex(path).tobinarray())

    with open(path, 'rb') as fp:
        return fp.read()


def _struct_endian(endian):
    return {'little': '<', 'big': '>'}[endian]


def _image_layout(data, endian):
    e = _struct_endian(endian)
    hdr = struct.unpack(e + 'IIHHII', data[:20])
    magic = hdr[0]
    hdr_size = hdr[2]
    prot_tlv_size = hdr[3]
    img_size = hdr[4]

    if magic != IMAGE_MAGIC:
        raise ValueError('base image does not contain a valid MCUboot header')

    hash_end = hdr_size + img_size + prot_tlv_size
    if hash_end > len(data):
        raise ValueError('image is truncated before the protected TLV area')

    total_end = hash_end
    if total_end + 4 <= len(data):
        tlv_magic, tlv_tot = struct.unpack(e + 'HH', data[total_end:total_end + 4])
        if tlv_magic == TLV_INFO_MAGIC:
            total_end += tlv_tot
        elif tlv_magic == TLV_PROT_INFO_MAGIC:
            raise ValueError('unexpected protected TLV at unprotected TLV offset')

    if total_end > len(data):
        raise ValueError('image is truncated before the unprotected TLV area ends')

    return hash_end, total_end


def image_hash(data, hash_algorithm, endian):
    hash_end, _ = _image_layout(data, endian)
    digest = hash_algorithm()
    digest.update(data[:hash_end])
    return digest.digest()


def image_core(data, endian):
    _, total_end = _image_layout(data, endian)
    return data[:total_end]


def build_delta(base_image, target_image, block_size, erased_val, hash_algorithm,
                endian):
    if block_size <= 0 or block_size % 4 != 0 or (block_size & (block_size - 1)) != 0:
        raise ValueError('--delta-block-size must be a positive power-of-two multiple of 4')

    base_core = image_core(base_image, endian)
    target_core = image_core(target_image, endian)
    compare_size = align_up(max(len(base_core), len(target_core)), block_size)
    erased = bytes([erased_val])
    base_cmp = base_core.ljust(compare_size, erased)
    target_cmp = target_core.ljust(compare_size, erased)
    records = []
    run_offset = None
    run_new_data = bytearray()
    run_old_data = bytearray()

    def flush_run():
        nonlocal run_offset

        if run_offset is None:
            return

        records.append((run_offset, bytes(run_new_data), bytes(run_old_data)))
        run_offset = None
        run_new_data.clear()
        run_old_data.clear()

    for off in range(0, compare_size, block_size):
        base_block = base_cmp[off:off + block_size]
        target_block = target_cmp[off:off + block_size]

        if base_block == target_block:
            flush_run()
            continue

        if run_offset is None:
            run_offset = off

        run_new_data.extend(target_block)
        run_old_data.extend(base_block)

    flush_run()

    payload = bytearray(struct.pack(
        DELTA_HEADER,
        DELTA_MAGIC,
        DELTA_VERSION,
        DELTA_HEADER_SIZE,
        len(target_core),
        compare_size,
        len(records),
        block_size,
        DELTA_FLAG_RESTORE,
        len(base_core),
    ))

    for off, new_data, old_data in records:
        payload.extend(struct.pack(DELTA_RECORD, off, len(new_data)))
        payload.extend(new_data)
        payload.extend(old_data)
        payload.extend(bytes(align_up(len(payload), 4) - len(payload)))

    return DeltaPayload(
        payload=bytes(payload),
        base_hash=image_hash(base_core, hash_algorithm, endian),
        target_hash=image_hash(target_core, hash_algorithm, endian),
        target_size=len(target_core),
        write_size=compare_size,
        record_count=len(records),
    )
