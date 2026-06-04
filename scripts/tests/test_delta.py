# SPDX-License-Identifier: Apache-2.0

import struct
from pathlib import Path

from click.testing import CliRunner
from imgtool import delta, image
from imgtool.main import imgtool

VERSION_BASE = '1.0.0'
VERSION_TARGET = '2.0.0'
HEADER_SIZE = 0x200
SLOT_SIZE = 0x20000


def sign_image(in_file: Path, out_file: Path, version: str,
               *extra_args: str, slot_size: int = SLOT_SIZE):
    runner = CliRunner()
    result = runner.invoke(
        imgtool,
        [
            'sign',
            str(in_file),
            str(out_file),
            f'--header-size={HEADER_SIZE}',
            f'--slot-size={slot_size}',
            f'--version={version}',
            '--pad-header',
            *extra_args,
        ],
    )
    assert result.exit_code == 0, result.output
    assert out_file.exists()
    return result


def image_flags(path: Path) -> int:
    data = path.read_bytes()
    return struct.unpack('<I', data[16:20])[0]


def apply_delta(base_signed: Path, patch_signed: Path, reverse: bool = False) -> bytes:
    patch = patch_signed.read_bytes()
    hdr_size, img_size = struct.unpack('<HI', patch[8:10] + patch[12:16])
    patch_payload = patch[hdr_size:hdr_size + img_size]
    magic, version, delta_header_size, target_size, write_size, record_count, \
        block_size, flags, base_size = struct.unpack(
            delta.DELTA_HEADER, patch_payload[:delta.DELTA_HEADER_SIZE])

    assert magic == delta.DELTA_MAGIC
    assert version == delta.DELTA_VERSION
    assert write_size >= target_size
    assert not reverse or flags & delta.DELTA_FLAG_RESTORE

    base_core = delta.image_core(base_signed.read_bytes(), 'little')
    out = bytearray(base_core.ljust(write_size, b'\xff'))
    off = delta_header_size

    for _ in range(record_count):
        rec_off, rec_size = struct.unpack(delta.DELTA_RECORD, patch_payload[off:off + 8])
        off += 8
        new_data_off = off
        old_data_off = new_data_off + rec_size
        source_off = old_data_off if reverse else new_data_off
        out[rec_off:rec_off + rec_size] = patch_payload[source_off:source_off + rec_size]
        off += rec_size
        off += rec_size
        off = image.align_up(off, 4)

    assert off == len(patch_payload)
    return bytes(out[:base_size if reverse else target_size])


def test_delta_image_reconstructs_signed_target_and_base(tmp_path: Path):
    base_raw = tmp_path / 'base.bin'
    target_raw = tmp_path / 'target.bin'
    base_signed = tmp_path / 'base-signed.bin'
    target_signed = tmp_path / 'target-signed.bin'
    patch_signed = tmp_path / 'target-delta.bin'

    base = bytearray(b'A' * 8192)
    target = bytearray(base)
    target[1024:1040] = b'delta-dfu-target'
    target[4096:4112] = b'mcuboot-delta!!'

    base_raw.write_bytes(base)
    target_raw.write_bytes(target)

    sign_image(base_raw, base_signed, VERSION_BASE)
    sign_image(target_raw, target_signed, VERSION_TARGET)
    result = sign_image(
        target_raw,
        patch_signed,
        VERSION_TARGET,
        f'--delta-base={base_signed}',
        '--delta-block-size=16',
        '--overwrite-only',
    )

    assert 'delta payload size:' in result.output
    assert 'delta restore data: included' in result.output
    assert 'delta write span:' in result.output
    assert image_flags(patch_signed) & image.IMAGE_F['DELTA']
    assert patch_signed.stat().st_size < target_signed.stat().st_size
    assert apply_delta(base_signed, patch_signed) == \
        delta.image_core(target_signed.read_bytes(), 'little')
    assert apply_delta(target_signed, patch_signed, reverse=True) == \
        delta.image_core(base_signed.read_bytes(), 'little')

def test_delta_image_fails_when_patch_exceeds_slot(tmp_path: Path):
    base_raw = tmp_path / 'base.bin'
    target_raw = tmp_path / 'target.bin'
    base_signed = tmp_path / 'base-signed.bin'
    target_signed = tmp_path / 'target-signed.bin'
    patch_signed = tmp_path / 'target-delta.bin'
    runner = CliRunner()

    base_raw.write_bytes(b'A' * (70 * 1024))
    target_raw.write_bytes(b'B' * (70 * 1024))

    sign_image(base_raw, base_signed, VERSION_BASE, '--overwrite-only')
    sign_image(target_raw, target_signed, VERSION_TARGET, '--overwrite-only')
    assert target_signed.stat().st_size < SLOT_SIZE

    result = runner.invoke(
        imgtool,
        [
            'sign',
            str(target_raw),
            str(patch_signed),
            f'--header-size={HEADER_SIZE}',
            f'--slot-size={SLOT_SIZE}',
            f'--version={VERSION_TARGET}',
            '--pad-header',
            f'--delta-base={base_signed}',
            '--delta-block-size=16',
            '--overwrite-only',
        ],
    )

    assert result.exit_code != 0
    assert 'exceeds requested size' in result.output
    assert not patch_signed.exists()
