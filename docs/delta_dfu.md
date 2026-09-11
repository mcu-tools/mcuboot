# Delta DFU

Delta DFU lets MCUboot accept a signed patch image in the secondary slot and
apply it to the current primary image. The patch image is a normal MCUboot image
with the `IMAGE_F_DELTA` header flag set. It is transport-neutral: any DFU
transport that can write the patch image to the secondary slot and mark it
pending can use it.

Delta images are supported by the overwrite-only boot path. They are not
compressed images and they do not use the decompression path.

## Image Format

The delta image payload starts with this little-endian header:

```c
struct boot_delta_header {
    uint32_t magic;       /* "MDL1" */
    uint16_t version;     /* 1 */
    uint16_t header_size; /* sizeof(struct boot_delta_header) */
    uint32_t target_size; /* reconstructed signed target image size */
    uint32_t write_size;  /* primary-slot span covered by the patch */
    uint32_t record_count;
    uint32_t block_size;
    uint32_t flags;
    uint32_t base_size;
};
```

The header is followed by `record_count` records:

```c
struct boot_delta_record {
    uint32_t offset;
    uint32_t size;
    uint8_t new_data[size];
    uint8_t old_data[size];
};
```

Each record replaces `size` bytes at `offset` in the primary slot. Records are
strictly ordered and non-overlapping, and record data is padded to 4 bytes in
the patch payload.

Delta records contain the old bytes from the base image. The old bytes let
MCUboot recover from interrupted updates by restoring the touched regions back
to the signed base image before retrying the forward apply.

The patch image contains protected TLVs with the hash of the expected base
image and the hash of the reconstructed target image:

- `IMAGE_TLV_DELTA_BASE_SHA`
- `IMAGE_TLV_OUTPUT_SHA`

The regular image SHA authenticates the delta payload itself, so it cannot also
identify the base image.

During boot, MCUboot validates the signed delta image. If the active primary
hash already matches `IMAGE_TLV_OUTPUT_SHA`, MCUboot validates the complete
target image before finishing the update metadata. A matching hash with an
incomplete or damaged validation TLV causes the affected records to be written
again. If the hash matches `IMAGE_TLV_DELTA_BASE_SHA`, MCUboot applies the
records to the primary slot. If the hash matches neither value, MCUboot treats
the previous delta apply as interrupted, restores the touched regions using the
old bytes, validates the base image, and retries the update.

## Zephyr Configuration

Enable delta DFU in the Zephyr MCUboot image with:

```text
CONFIG_BOOT_UPGRADE_ONLY=y
CONFIG_BOOT_DELTA_DFU=y
```

For sysbuild, select overwrite-only mode at the sysbuild level:

```text
SB_CONFIG_MCUBOOT_MODE_OVERWRITE_ONLY=y
```

On flash devices that require erase before write, delta records must cover whole
primary-slot erase sectors and the delta write span must end before the final
primary-slot erase sector. That sector is reserved for recovery state so its
marker is never lost while image records are being restored. Configure the RAM buffer with
`CONFIG_BOOT_DELTA_DFU_SECTOR_BUFFER_SIZE`; it must be at least as large as the
largest primary-slot erase sector touched by a delta update or by the primary
trailer transition.

## Creating Delta Images

Create the current signed base image and the full signed target image normally.
Then create the delta image by passing the signed base image to `imgtool sign`:

```bash
./scripts/imgtool.py sign \
  --version 2.0.0+0 \
  --header-size 0x800 \
  --slot-size 729088 \
  --overwrite-only \
  --align 1 \
  --key root-rsa-2048.pem \
  --delta-base app-base.signed.bin \
  --delta-block-size 4096 \
  app-target.bin \
  app-target.delta.signed.bin
```

`--delta-block-size` controls the comparison granularity. It must be a positive
power-of-two multiple of 4 and should be a multiple of the target flash write
alignment. On flash devices that require erase before write, use the primary
slot erase sector size so every changed record can be safely restored after an
interruption. The reconstructed base and target images must end before the final
primary-slot erase sector, and the signed delta image and its TLVs must end
before the final secondary-slot erase sector. MCUboot uses those sectors for
durable recovery state. `imgtool` reserves at least `--delta-block-size` bytes
at the end of both slots for this purpose. If either final erase sector is
larger, use that larger size as `--delta-block-size`; MCUboot rejects a delta
whose write span or patch overlaps a reserved sector.

Delta images always include old bytes for each changed record. This makes the
delta payload larger than a forward-only patch, but it is required for
interruption recovery and is still usually smaller than keeping a full second
image slot.

Each changed region stores both the new bytes and the old bytes, plus record
metadata and padding. A delta that touches many scattered regions can therefore
cost about 2x the changed bytes and can exceed the size of a plain overwrite
image. Compare the generated delta size with the full signed target image before
shipping a delta update.

## Restore Behavior

Delta restore uses the same secondary slot that received the update; it does not
require a transport-specific path. When a delta image is marked as a test update,
MCUboot applies the forward records and validates the new primary image. It then
clears the reserved primary trailer sector, writes durable primary restore
state, and only then erases the secondary-slot trailer sector. The signed delta
payload remains in the secondary slot, outside that final erase sector.

If the new application confirms itself, the primary trailer `image_ok` flag is
set and the restore is cancelled. If the device reboots before confirmation,
MCUboot sees the primary trailer state, validates the same signed delta image in
the secondary slot, applies the old bytes from each record, validates the
restored image against `IMAGE_TLV_DELTA_BASE_SHA`, and commits the restored base
with `image_ok` before invalidating the patch. A reset before the primary restore
state is durable leaves the secondary test marker intact, so MCUboot retries the
forward apply. A reset during either forward apply or restore is recovered by
restoring the base records and replaying the requested direction.
