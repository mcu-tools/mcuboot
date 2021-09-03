<!--
  - SPDX-License-Identifier: Apache-2.0

  - Copyright (c) 2017-2020 Linaro LTD
  - Copyright (c) 2017-2019 JUUL Labs
  - Copyright (c) 2019-2021 Arm Limited

  - Original license:

  - Licensed to the Apache Software Foundation (ASF) under one
  - or more contributor license agreements.  See the NOTICE file
  - distributed with this work for additional information
  - regarding copyright ownership.  The ASF licenses this file
  - to you under the Apache License, Version 2.0 (the
  - "License"); you may not use this file except in compliance
  - with the License.  You may obtain a copy of the License at

  -  http://www.apache.org/licenses/LICENSE-2.0

  - Unless required by applicable law or agreed to in writing,
  - software distributed under the License is distributed on an
  - "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
  - KIND, either express or implied.  See the License for the
  - specific language governing permissions and limitations
  - under the License.
-->

# Bootloader design

This document describes the design of MCUboot.

## [Summary](#summary)

MCUboot is composed of two packages:

* The bootutil library (boot/bootutil)
* The boot application (each port has its own at boot/<port>)

The bootutil library performs most of the functions of a bootloader, except the final step of actually jumping to the main image.
This last step is instead implemented by the boot application.

The bootloader functionality is separated in this manner to enable unit testing of the bootloader.
A library can be unit tested, while an application can't be.
Therefore, functionality is delegated to the bootutil library, when possible.

## [Limitations](#limitations)

The bootloader currently supports only images with the following characteristics:
* Built to run from flash memory.
* Built to run from a fixed location (namely, not position-independent).

## [Image format](#image-format)

The following definitions describe the image format.

``` c
#define IMAGE_MAGIC                 0x96f3b83d

#define IMAGE_HEADER_SIZE           32

struct image_version {
    uint8_t iv_major;
    uint8_t iv_minor;
    uint16_t iv_revision;
    uint32_t iv_build_num;
};

/** Image header.  All fields are in little endian byte order. */
struct image_header {
    uint32_t ih_magic;
    uint32_t ih_load_addr;
    uint16_t ih_hdr_size;           /* Size of image header (bytes). */
    uint16_t ih_protect_tlv_size;   /* Size of protected TLV area (bytes). */
    uint32_t ih_img_size;           /* Does not include header. */
    uint32_t ih_flags;              /* IMAGE_F_[...]. */
    struct image_version ih_ver;
    uint32_t _pad1;
};

#define IMAGE_TLV_INFO_MAGIC        0x6907
#define IMAGE_TLV_PROT_INFO_MAGIC   0x6908

/** Image TLV header.  All fields in little endian. */
struct image_tlv_info {
    uint16_t it_magic;
    uint16_t it_tlv_tot;  /* size of TLV area (including tlv_info header) */
};

/** Image trailer TLV format. All fields in little endian. */
struct image_tlv {
    uint8_t  it_type;   /* IMAGE_TLV_[...]. */
    uint8_t  _pad;
    uint16_t it_len;    /* Data length (not including TLV header). */
};

/*
 * Image header flags.
 */
#define IMAGE_F_PIC                      0x00000001 /* Not supported. */
#define IMAGE_F_ENCRYPTED_AES128         0x00000004 /* Encrypted using AES128. */
#define IMAGE_F_ENCRYPTED_AES256         0x00000008 /* Encrypted using AES256. */
#define IMAGE_F_NON_BOOTABLE             0x00000010 /* Split image app. */
#define IMAGE_F_RAM_LOAD                 0x00000020

/*
 * Image trailer TLV types.
 */
#define IMAGE_TLV_KEYHASH           0x01   /* hash of the public key */
#define IMAGE_TLV_SHA256            0x10   /* SHA256 of image hdr and body */
#define IMAGE_TLV_RSA2048_PSS       0x20   /* RSA2048 of hash output */
#define IMAGE_TLV_ECDSA224          0x21   /* ECDSA of hash output */
#define IMAGE_TLV_ECDSA256          0x22   /* ECDSA of hash output */
#define IMAGE_TLV_RSA3072_PSS       0x23   /* RSA3072 of hash output */
#define IMAGE_TLV_ED25519           0x24   /* ED25519 of hash output */
#define IMAGE_TLV_ENC_RSA2048       0x30   /* Key encrypted with RSA-OAEP-2048 */
#define IMAGE_TLV_ENC_KW            0x31   /* Key encrypted with AES-KW-128 or
                                              256 */
#define IMAGE_TLV_ENC_EC256         0x32   /* Key encrypted with ECIES-P256 */
#define IMAGE_TLV_ENC_X25519        0x33   /* Key encrypted with ECIES-X25519 */
#define IMAGE_TLV_DEPENDENCY        0x40   /* Image depends on other image */
#define IMAGE_TLV_SEC_CNT           0x50   /* security counter */
```

Optional type-length-value records (TLVs) containing image metadata are placed after the end of the image.

The `ih_protect_tlv_size` field indicates the length of the protected TLV area.
If protected TLVs are present then a TLV info header with magic equal to `IMAGE_TLV_PROT_INFO_MAGIC` must be present and the protected TLVs (plus the info header itself) must be included in the hash calculation.
Otherwise, the hash is only calculated over the image header and the image itself.
In this case, the value of the `ih_protect_tlv_size` field is 0.

The `ih_hdr_size` field indicates the length of the header, and therefore the offset of the image itself.
This field provides for backward compatibility in case of changes to the format of the image header.

## [Flash map](#flash-map)

A device's flash memory is partitioned according to its _flash map_.
At a high level, the flash map maps numeric IDs to `_flash areas_.
A flash memory area is a region of the disk with the following properties:
1. It can be fully erased without affecting any other areas.
2. A write to this area does not restrict writes to other areas.

The bootloader uses the following flash area IDs:
```c
/* Independent from multiple image boot */
#define FLASH_AREA_BOOTLOADER         0
#define FLASH_AREA_IMAGE_SCRATCH      3
```
```c
/* If the bootloader is working with the first image */
#define FLASH_AREA_IMAGE_PRIMARY      1
#define FLASH_AREA_IMAGE_SECONDARY    2
```
```c
/* If the bootloader is working with the second image */
#define FLASH_AREA_IMAGE_PRIMARY      5
#define FLASH_AREA_IMAGE_SECONDARY    6
```

The bootloader area contains the bootloader image itself.
The other areas are described in subsequent sections.
The flash memory could contain multiple executable images.
Therefore, the flash area IDs of primary and secondary areas are mapped based on the number of the active image (namely, the image on which the bootloader is currently working).

## [Image slots](#image-slots)

A portion of the flash memory can be partitioned into multiple image areas, each area containing two image slots: a primary slot and a secondary slot.

Normally, the bootloader will only run an image from the primary slot, so images must be built such that they can run from that fixed location in the flash memory (an exception to this is the [direct-xip](#direct-xip) and the [ram-load](#ram-load) upgrade mode).
If the bootloader needs to run the image resident in the secondary slot, it must copy its content into the primary slot before doing so, either by swapping the two images or by overwriting the content of the primary slot.

The bootloader supports either swap-based or overwrite-based image upgrades, but must be configured at build time to choose one of these two strategies.

In addition to the slots of image areas, the bootloader requires a scratch area to allow for reliable image swapping.
The scratch area must have a size that is big enough to store at least the largest sector that is going to be swapped.
Many devices have small equally sized flash sectors, for example, 4K, while others have variable sized sectors where the largest sectors might be 128K or 256K, so the scratch must be big enough to store that.
The scratch is only ever used when swapping firmware, which means only when doing an upgrade.
Given that, the main reason for using a larger size for the scratch is that flash memory wear will be more evenly distributed, because a single sector would be written twice the number of times than using two sectors, for example.

To evaluate the ideal size of the scratch for your use case the following parameters are relevant:
- the ratio of "image size / scratch size"
- the number of erase cycles supported by the flash memory hardware

The image size is used (instead of slot size) because only the slot's sectors that are actually used for storing the image are copied.
The image/scratch ratio is the number of times the scratch will be erased on every upgrade.
The number of erase cycles divided by the image/scratch ratio will give you the number of times an upgrade can be performed before the device goes out of spec:

```
num_upgrades = number_of_erase_cycles / (image_size / scratch_size)
```

Let's assume, for example, a device with 10000 erase cycles, an image size of 150K, and a scratch of 4K (usual minimum size of 4K sector devices).
This would result in the following total:

```
10000 / (150 / 4) ~ 267
```

Increasing the scratch to 16K would result in the following:

```
10000 / (150 / 16) ~ 1067
```

There isn't a *best* ratio, as the right size is use-case dependent.
Factors to consider include the number of times a device will be upgraded both in the field and during development, as well as any desired safety margin on the manufacturer's specified number of erase cycles.
In general, using a ratio that allows hundreds to thousands of field upgrades in production is recommended.

### [Equal slots (direct-xip)](#direct-xip)

When the direct-xip mode is enabled, the active image flag is "moved" between the slots during the image upgrade.
Also, in direct-xip mode, the bootloader can run an image directly from either the primary or the secondary slot (without having to move/copy it into the primary slot).
Therefore, the image update client, which downloads the new images, must be aware of which slot contains the active image and which acts as a staging area.
It is also responsible for loading the proper images into the proper slot.

All this requires that the images are built to be executed from the corresponding slot.
At boot time the bootloader first looks for images in the slots and then inspects the version numbers in the image headers.
It selects the newest image (having the highest version number) and then checks its validity (integrity check, signature verification, and so on).
If the image is invalid, MCUboot erases its memory slot and starts to validate the other image.
After successful validation of the selected image, the bootloader chain-loads it.

An additional "revert" mechanism is also supported.
For more information, read the [corresponding section](#direct-xip-revert).

Handling the primary and secondary slots as equals has its drawbacks.
Since the images are not moved between the slots, the on-the-fly image encryption and decryption can't be supported.
This limitation only applies to storing the image in an external flash on the device, the transport of encrypted image data is still feasible.

---
***Note***

*The overwrite and the direct-xip upgrade strategies are substantially simpler to implement than the image swapping strategy, especially since the bootloader must work properly even when it is reset during the middle of an image swap.*
*For this reason, the rest of the document describes its behavior when configured to swap images during an upgrade.*

---

### [RAM loading](#ram-load)

In ram-load mode the slots are equal.
Like the direct-xip mode, this mode also selects the newest image by reading the image version numbers in the image headers.
But instead of executing it in place, the newest image is copied to the RAM for execution.
The load address (namely, the location in RAM where the image is copied to) is stored in the image header.

The ram-load upgrade mode can be useful when there is no internal flash memory in the SoC, but there is a big enough internal RAM to hold the images.
Usually, in this case, the images are stored in an external storage device.

Execution from external storage has some drawbacks (lower execution speed, image is exposed to attacks) therefore the image is always copied to the internal RAM before the authentication and execution.
The ram-load mode requires the image to be built to be executed from the RAM address range instead of the storage device address range.
If ram-load is enabled then the platform must define the following parameters:

```c
#define IMAGE_EXECUTABLE_RAM_START    <area_base_addr>
#define IMAGE_EXECUTABLE_RAM_SIZE     <area_size_in_bytes>
```

For multiple image loads, if multiple ram regions are used, the platform must define the `MULTIPLE_EXECUTABLE_RAM_REGIONS` flag instead and implement the following function:

```c
int boot_get_image_exec_ram_info(uint32_t image_id,
                                 uint32_t *exec_ram_start,
                                 uint32_t *exec_ram_size)
```

When ram-load is enabled, the `--load-addr <addr>` option of the `imgtool` script must also be used when signing the images.
This option sets the `RAM_LOAD` flag in the image header, indicating that the image should be loaded to the RAM, and also sets the load address in the image header.

The ram-load mode currently does not support the image encryption feature.

## [Boot swap types](#boot-swap-types)

When the device first boots under normal circumstances, there is an up-to-date firmware image in each primary slot, which MCUboot can validate and then chain-load.
In this case, no image swaps are necessary.
During device upgrades, however, new candidate image(s) is present in the secondary slot(s), which MCUboot must swap into the primary slot(s) before booting as discussed above.

Upgrading an old image with a new one by swapping is a two-step process.
In this process, MCUboot performs a "test" swap of the image data in the flash memory and boots the new image or it will be executed during operation.
The new image can then update the content of the flash memory at runtime to mark itself "OK", and MCUboot will then still choose to run it during the next boot.
When this happens, the swap is made "permanent".
If this doesn't happen, MCUboot will perform a "revert" swap during the next boot by swapping the image(s) back into its original location(s) and attempting to boot the old image(s).

Depending on the use case, the first swap can also be made permanent directly.
In this case, MCUboot will never attempt to revert the images on the next reset.

Test swaps are supported to provide a rollback mechanism to prevent devices from becoming "bricked" by bad firmware.
If the device crashes immediately upon booting a new (bad) image, MCUboot will revert to the old (working) image at the next device reset, rather than booting the bad image again.
This allows device firmware to make test swaps that are made permanent only after performing a self-test routine.

On startup, MCUboot inspects the content of the flash memory to decide for each image which of these "swap types" to perform.
This decision determines how it proceeds.

The possible swap types, and their meanings, are the following:

- `BOOT_SWAP_TYPE_NONE`: The "usual" or "no upgrade" case.
  It attempts to boot the content of the primary slot.

- `BOOT_SWAP_TYPE_TEST`: It boots the content of the secondary slot by swapping images.
  Unless the swap is made permanent, it reverts back on the next boot.

- `BOOT_SWAP_TYPE_PERM`: Permanently swaps images, and boots the upgraded image firmware.

- `BOOT_SWAP_TYPE_REVERT`: A previous test swap was not made permanent.
  It swaps back to the old image whose data are now in the secondary slot.
  If the old image marks itself "OK" when it boots, the next boot will have swap type `BOOT_SWAP_TYPE_NONE`.

- `BOOT_SWAP_TYPE_FAIL`: The swap failed because the image to be run is not valid.

- `BOOT_SWAP_TYPE_PANIC`: Swapping encountered an unrecoverable error.

The "swap type" is a high-level representation of the outcome of the boot.
Subsequent sections describe how MCUboot determines the swap type from the bit-level content of the flash memory.

### [Revert mechanism in direct-xip mode](#direct-xip-revert)

The direct-xip mode also supports a "revert" mechanism which is the equivalent of the swap mode's "revert" swap.
When the direct-xip mode is selected it can be enabled with the `MCUBOOT_DIRECT_XIP_REVERT` config option and an image trailer must also be added to the signed images (the `--pad` option of the `imgtool` script must be used).

For more information on this, read the [Image Trailer](#image-trailer) section and the [imgtool](imgtool.md) documentation.

Making the images permanent (marking them as confirmed in advance) is also supported, just like in swap mode.
The individual steps of the direct-xip mode's "revert" mechanism are the following:

1. Select the slot which holds the newest potential image.
2. Was the image previously selected to run (during a previous boot)?
    + Yes: Did the image mark itself "OK" (was the self-test successful)?
        + Yes.
            - Proceed to step 3.
        + No.
            - Erase the image from the slot to prevent it from being selected again during the next boot.
            - Return to step 1 (the bootloader will attempt to select and possibly boot the previous image if there is one).
    + No.
        - Mark the image as "selected" (set the copy_done flag in the trailer).
        - Proceed to step 3.
3. Proceed to image validation.

## [Image trailer](#image-trailer)

The bootloader uses metadata stored in the image flash memory areas to be able to determine the current state and what actions should be taken during the current boot operation.
While swapping, some of this metadata is temporarily copied into and out of the scratch area.

This metadata is located at the end of the image flash areas and is called an image trailer.
An image trailer has the following structure:

```
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ~                                                               ~
    ~    Swap status (BOOT_MAX_IMG_SECTORS * min-write-size * 3)    ~
    ~                                                               ~
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                 Encryption key 0 (16 octets) [*]              |
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                 Encryption key 1 (16 octets) [*]              |
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                      Swap size (4 octets)                     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   Swap info   |           0xff padding (7 octets)             |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   Copy done   |           0xff padding (7 octets)             |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   Image OK    |           0xff padding (7 octets)             |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                       MAGIC (16 octets)                       |
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

[*]: Only present if the encryption option is enabled (`MCUBOOT_ENC_IMAGES`).

The offset immediately following such a record represents the start of the next flash memory area.

---
***Note***

*`min-write-size` is a property of the flash hardware:*
*- If the hardware allows individual bytes to be written at arbitrary addresses, then `min-write-size` is 1.*
*- If the hardware only allows writes at even addresses, then `min-write-size` is 2, and so on.*

---

An image trailer contains the following fields:

1. Swap status: A series of records that records the progress of an image swap.
   To swap entire images, data are swapped between the two image areas one or more sectors at a time, like this:

   - sector data in the primary slot is copied into the scratch, then erased.
   - sector data in the secondary slot is copied into the primary slot, then erased.
   - sector data in scratch is copied into the secondary slot.

   As it swaps images, the bootloader updates the swap status field in a way that allows it to compute how far this swap operation has progressed for each sector.
   The swap status field can thus be used to resume a swap operation if the bootloader is halted while a swap operation is ongoing and later reset.
   The `BOOT_MAX_IMG_SECTORS` value is the configurable maximum number of sectors MCUboot supports for each image.
   Its value defaults to 128 but allows for either decreasing this size, limiting RAM usage, or increasing it in devices that have massive amounts of flash memory or very small-sized sectors and thus require a bigger configuration to allow for the handling of all slot's sectors.
   The factor of `min-write-size` is due to the behavior of the flash memory hardware.
   The factor of 3 is explained below.

2. Encryption keys: key-encrypting keys (KEKs).
   These keys are needed for image encryption and decryption.
   See the [encrypted images](encrypted_images.md) document for more information.

3. Swap size: When beginning a new swap operation, the total size that needs to be swapped (based on the slot with the largest image + TLVs) is written to this location for easier recovery in case of a reset while performing the swap.

4. Swap info: A single byte that encodes the following information:
    - Swap type: Stored in bits 0-3.
      Indicating the type of swap operation in progress.
      When MCUboot resumes an interrupted swap, it uses this field to determine the type of operation to perform.
      This field contains one of the following values in the table below.
    - Image number: Stored in bits 4-7.
      It has always 0 value at a single-image boot.
      In the case of a multi-image boot, it indicates which image was swapped when the interrupt happened.
      The same scratch area is used during all image swap operations.
      Therefore this field is used to determine which image the trailer belongs to if boot status is found on the scratch area when the swap operation is resumed.

```
+ ------------------------- + ----- +
| Name                      | Value |
+ ------------------------- + ----- +
| `BOOT_SWAP_TYPE_TEST`     | 2     |
| `BOOT_SWAP_TYPE_PERM`     | 3     |
| `BOOT_SWAP_TYPE_REVERT`   | 4     |
+ ------------------------- + ----- +

```


5. Copy done: A single byte indicating whether the image in this slot is complete (`0x01`=done, `0xff`=not done).

6. Image OK: A single byte indicating whether the image in this slot has been confirmed as good by the user (`0x01`=confirmed, `0xff`=not confirmed).

7. MAGIC: The following 16 bytes, written in host-byte-order:

``` c
    const uint32_t boot_img_magic[4] = {
        0xf395c277,
        0x7fefd260,
        0x0f505235,
        0x8079b62c,
    };
```

## [Image trailers](#image-trailers)

At startup, the bootloader determines the boot swap type by inspecting the image trailers.
When using the term "image trailers" what is meant is the aggregate information provided by both image slot trailers.

### [New swaps (non-resumes)](#new-swaps-non-resumes)

For new swaps, MCUboot must inspect a collection of fields to determine which swap operation to perform.

The image trailers records are structured around the limitations imposed by the flash memory hardware.
As a consequence, they do not have a very intuitive design, and it is difficult to get a sense of the state of the device just by looking at the image trailers.
It is better to map all the possible trailer states to the swap types described above via a set of tables.
These tables are reproduced below.

---
***Note***

*An important caveat about the tables described below is that they must be evaluated in the order presented here.*
*Lower state numbers must have a higher priority when testing the image trailers.*

---

```
    State I
                     | primary slot | secondary slot |
    -----------------+--------------+----------------|
               magic | Any          | Good           |
            image-ok | Any          | Unset          |
           copy-done | Any          | Any            |
    -----------------+--------------+----------------'
     result: BOOT_SWAP_TYPE_TEST                     |
    -------------------------------------------------'


    State II
                     | primary slot | secondary slot |
    -----------------+--------------+----------------|
               magic | Any          | Good           |
            image-ok | Any          | 0x01           |
           copy-done | Any          | Any            |
    -----------------+--------------+----------------'
     result: BOOT_SWAP_TYPE_PERM                     |
    -------------------------------------------------'


    State III
                     | primary slot | secondary slot |
    -----------------+--------------+----------------|
               magic | Good         | Unset          |
            image-ok | 0xff         | Any            |
           copy-done | 0x01         | Any            |
    -----------------+--------------+----------------'
     result: BOOT_SWAP_TYPE_REVERT                   |
    -------------------------------------------------'
```

Any of the above three states results in MCUboot attempting to swap images.

Otherwise, MCUboot does not attempt to swap images, resulting in one of the other three swap types, as illustrated by State IV.

```
    State IV
                     | primary slot | secondary slot |
    -----------------+--------------+----------------|
               magic | Any          | Any            |
            image-ok | Any          | Any            |
           copy-done | Any          | Any            |
    -----------------+--------------+----------------'
     result: BOOT_SWAP_TYPE_NONE,                    |
             BOOT_SWAP_TYPE_FAIL, or                 |
             BOOT_SWAP_TYPE_PANIC                    |
    -------------------------------------------------'
```

In State IV, when no errors occur, MCUboot will attempt to boot the content of the primary slot directly, and the result is `BOOT_SWAP_TYPE_NONE`.
If the image in the primary slot is not valid, the result is `BOOT_SWAP_TYPE_FAIL`.
If a fatal error occurs during boot, the result is `BOOT_SWAP_TYPE_PANIC`.
If the result is either `BOOT_SWAP_TYPE_FAIL` or `BOOT_SWAP_TYPE_PANIC`, MCUboot hangs rather than booting an invalid or compromised image.

---
***Note***

*An important caveat to the above is the result when a swap is requested and the image in the secondary slot fails to validate, due to a hashing or signing error.*
*This state behaves as State IV with the extra action of marking the image in the primary slot as "OK", to prevent further attempts to swap.*

---

### [Resumed swaps](#resumed-swaps)

If MCUboot determines that it is resuming an interrupted swap (namely, a reset occurred mid-swap), it fully determines the operation to resume by reading the `swap info` field from the active trailer and extracting the swap type from bits 0-3
The set of tables in the previous section are not necessary in the resume case.

## [High-level operation](#high-level-operation)

With the terms defined, we can now explore the bootloader's operation.
First, a high-level overview of the boot process is presented.
Then, the following sections describe each step of the process in more detail.

Procedure:

1. Inspect swap status region; is an interrupted swap being resumed?
    + Yes: Complete the partial swap operation; skip to step 3.
    + No: Proceed to step 2.

2. Inspect image trailers; is a swap requested?
    + Yes:
        1. Is the requested image valid (integrity and security check)?
            + Yes.
                a. Perform swap operation.
                b. Persist completion of swap procedure to image trailers.
                c. Proceed to step 3.
            + No.
                a. Erase the invalid image.
                b. Persist failure of swap procedure to image trailers.
                c. Proceed to step 3.

    + No: Proceed to step 3.

3. Boot into the image in the primary slot.

### [Multiple image boot](#multiple-image-boot)

When the flash memory contains multiple executable images, the bootloader's operation is a bit more complex but still similar to the previously described procedure with one image.
As every image can be updated independently, the flash memory is partitioned further to arrange two slots for each image.

```
+--------------------+
| MCUBoot            |
+--------------------+
        ~~~~~            <- memory might be not contiguous
+--------------------+
| Image 0            |
| primary   slot     |
+--------------------+
| Image 0            |
| secondary slot     |
+--------------------+
        ~~~~~            <- memory might be not contiguous
+--------------------+
| Image N            |
| primary   slot     |
+--------------------+
| Image N            |
| secondary slot     |
+--------------------+
| Scratch            |
+--------------------+
```

MCUBoot can also handle dependencies between images.
For example, if an image needs to be reverted it might be necessary to revert another one too (namely due to API incompatibilities) or simply to prevent it from being updated because of an unsatisfied dependency.
Therefore all aborted swaps have to be completed and all the swap types have to be determined for each image before the dependency checks.
See the following section for more details on dependency handling.

The multi-image boot procedure is organized in loops that iterate over all the firmware images.
The high-level overview of the boot process is presented below.

+  Loop 1. Iterate over all images
    1. Inspect swap status region of the current image; is an interrupted swap being resumed?
        + Yes:
            + Review the validity of previously determined swap types of other images.
            + Complete the partial swap operation.
            + Mark the swap type as `None`.
            + Skip to the next image.
        + No: Proceed to step 2.

    2. Inspect image trailers in the primary and secondary slot; is an image swap requested?
        + Yes: Review the validity of previously determined swap types of other images.
          Is the requested image valid (integrity and security check)?
            + Yes:
                + Set the previously determined swap type for the current image.
                + Skip to the next image.
            + No:
                + Erase the invalid image.
                + Persist failure of swap procedure to image trailers.
                + Mark the swap type as `Fail`.
                + Skip to next image.
        + No:
            + Mark the swap type as `None`.
            + Skip to the next image.

+  Loop 2. Iterate over all images
    1. Does the current image depend on other images?
        + Yes: Are all the image dependencies satisfied?
            + Yes: Skip to the next image.
            + No:
                + Modify swap type depending on what the previous type was.
                + Restart dependency check from the first image.
        + No: Skip to next image.

+  Loop 3. Iterate over all images
    1. Is an image swap requested?
        + Yes:
            + Perform image update operation.
            + Persist completion of swap procedure to image trailers.
            + Skip to next image.
        + No: Skip to next image.

+  Loop 4. Iterate over all images
    1. Validate the image in the primary slot (integrity and security check) or at least do a basic sanity check to avoid booting into an empty flash memory area.

+ Boot into the image in the primary slot of the 0th image position (the other image in the boot chain is started by another image).

### [Multiple image boot for RAM loading and direct-xip](#multiple-image-boot-for-ram-loading-and-direct-xip)

The operation of the bootloader is different when the ram-load or the direct-xip strategy is chosen.
The flash map is very similar to the swap strategy but there is no need for a scratch area.

+  Loop 1. Until all images are loaded and all dependencies are satisfied
    1. Subloop 1. Iterate over all images
        + Does any of the slots contain an image?
            + Yes:
                + Choose the newer image.
                + Copy it to RAM in case of ram-load strategy.
                + Validate the image (integrity and security check).
                + If validation fails delete the image from flash and try the other slot.
                  (Image must be deleted from RAM too in case of ram-load strategy.)
            + No: Return with failure.

    2. Subloop 2. Iterate over all images
        + Does the current image depend on other images?
            + Yes: Are all the image dependencies satisfied?
                + Yes: Skip to the next image.
                + No:
                    + Delete the image from RAM in case of ram-load strategy, but do not delete it from flash.
                    + Try to load the image from the other slot.
                    + Restart dependency check from the first image.
            + No: Skip to next image.

+  Loop 2. Iterate over all images
    + Increase the security counter if needed.
    + Do the measured boot and the data sharing if needed.

+ Boot the loaded slot of image 0.

## [Image swapping](#image-swapping)

The bootloader swaps the content of the two image slots for two reasons:

  * The user has issued a "set pending" operation.
    The image in the secondary slot should be run once (state I) or repeatedly (state II), depending on whether a permanent swap was specified.
  * The test image gets rebooted without being confirmed.
    The bootloader should revert to the original image currently in the secondary slot (state III).

If the image trailers indicate that the image in the secondary slot should be run, the bootloader needs to copy it to the primary slot.
The image currently in the primary slot also needs to be retained in the flash memory so that it can be used later.
Furthermore, both images need to be recoverable if the bootloader resets in the middle of the swap operation.
The two images are swapped according to the following procedure:

1. Determine if both slots are compatible enough to have their images swapped.
   To be compatible, both have to have only sectors that can fit into the scratch area and if one of them has larger sectors than the other, it must be able to entirely fit some rounded number of sectors from the other slot.
   In the next steps, we'll use the terminology "region" for the total amount of data copied or erased because this can be any amount of sectors depending on how many the scratch can fit for some swap operation.
2. Iterate the list of region indices in descending order (namely, starting with the greatest index).
   Only regions that are predetermined to be part of the image are copied (current element = "index").
    + a. Erase scratch area.
    + b. Copy the secondary_slot[index] to scratch area.
        - If this is the last region in the slot, the scratch area has a temporary status area initialized to store the initial state, because the primary slot's last region will have to be erased.
          In this case, only the data that was calculated to the amount to the image is copied.
        - Else if this is the first swapped region but not the last region in the slot, initialize the status area in the primary slot and copy the full region content.
        - Else, copy the entire region content.
    + c. Write the updated swap status (i).
    + d. Erase the secondary_slot[index]
    + e. Copy the primary_slot[index] to the secondary_slot[index] according to the amount previosly copied at step b.
        - If this is not the last region in the slot, erase the trailer in the secondary slot, to always use the one in the primary slot.
    + f. Write the updated swap status (ii).
    + g. Erase the primary_slot[index].
    + h. Copy scratch area to primary_slot[index] according to the amount previously copied at step b.
        - If this is the last region in the slot, the status is read from the scratch area (where it was stored temporarily) and written anew in the primary slot.
    + i. Write the updated swap status (iii).
3. Persist on the completion of the swap procedure to the primary slot image trailer.

The additional caveats in step 2f are necessary so that the secondary slot image trailer can be written by the user at a later time.
With the image trailer unwritten, the user can test the image in the secondary slot (namely, transition to state I).

---
***Note***

*If the region being copied contains the last sector, then the swap status is temporarily maintained on the scratch area for the duration of this operation.*
*Otherwise, it always uses the primary slot's area.*

---
***Note***

*The bootloader tries to copy only used sectors (based on the largest image installed on any of the slots), minimizing the number of sectors copied and reducing the amount of time required for a swap operation.*

---

The particulars of step 3 vary depending on whether an image is being tested, permanently used, reverted, or a validation failure of the secondary slot happened when a swap was requested:

```
    * test:
        o Write primary_slot.copy_done = 1
        (swap caused the following values to be written:
            primary_slot.magic = BOOT_MAGIC
            secondary_slot.magic = UNSET
            primary_slot.image_ok = Unset)

    * permanent:
        o Write primary_slot.copy_done = 1
        (swap caused the following values to be written:
            primary_slot.magic = BOOT_MAGIC
            secondary_slot.magic = UNSET
            primary_slot.image_ok = 0x01)

    * revert:
        o Write primary_slot.copy_done = 1
        o Write primary_slot.image_ok = 1
        (swap caused the following values to be written:
            primary_slot.magic = BOOT_MAGIC)

    * failure to validate the secondary slot:
        o Write primary_slot.image_ok = 1
```

After completing the operations as described above, the image in the primary slot is booted.

## [Swap status](#swap-status)

The swap status region allows the bootloader to recover in case it restarts in the middle of an image swap operation.
The swap status region consists of a series of single-byte records.
These records are written independently, and therefore must be padded according to the minimum write size imposed by the flash hardware.

The structure of the swap status region is illustrated below.
In this figure, a `min-write-size` of 1 is assumed for simplicity.

```
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |sec127,state 0 |sec127,state 1 |sec127,state 2 |sec126,state 0 |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |sec126,state 1 |sec126,state 2 |sec125,state 0 |sec125,state 1 |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |sec125,state 2 |                                               |
    +-+-+-+-+-+-+-+-+                                               +
    ~                                                               ~
    ~               [Records for indices 124 through 1              ~
    ~                                                               ~
    ~               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ~               |sec000,state 0 |sec000,state 1 |sec000,state 2 |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

As described in the picture, each image slot is partitioned into a sequence of flash memory sectors.

If we were to enumerate the sectors in a single slot, starting at 0, we would have a list of sector indices.
Since there are two image slots, each sector index would correspond to a pair of sectors.
For example, sector index 0 corresponds to the first sector in the primary slot and the first sector in the secondary slot.
Finally, reverse the list of indices such that the list starts with index `BOOT_MAX_IMG_SECTORS - 1` and ends with 0.
The swap status region is a representation of this reversed list.

During a swap operation, each sector index transitions through four separate states:
```
0. primary slot: image 0,   secondary slot: image 1,   scratch: N/A
1. primary slot: image 0,   secondary slot: N/A,       scratch: image 1 (1->s, erase 1)
2. primary slot: N/A,       secondary slot: image 0,   scratch: image 1 (0->1, erase 0)
3. primary slot: image 1,   secondary slot: image 0,   scratch: N/A     (s->0)
```

Each time a sector index transitions to a new state, the bootloader writes a record to the swap status region.
Logically, the bootloader only needs one record per sector index to keep track of the current swap state.
However, due to limitations imposed by the flash hardware, a record cannot be overwritten when an index's state changes.
To solve this problem, the bootloader uses three records per sector index rather than just one.

Each sector-state pair is represented as a set of three records.
The record values map to the above four states as follows:

```
            | rec0 | rec1 | rec2
    --------+------+------+------
    state 0 | 0xff | 0xff | 0xff
    state 1 | 0x01 | 0xff | 0xff
    state 2 | 0x01 | 0x02 | 0xff
    state 3 | 0x01 | 0x02 | 0x03
```

The swap status region can accommodate `BOOT_MAX_IMG_SECTORS` sector indices.
Hence, the size of the region, in bytes, is obtained by the following multiplication: `BOOT_MAX_IMG_SECTORS * min-write-size * 3`.

The only requirement for the index count is that it must be large enough to account for a maximum-sized image (namely, at least as large as the total sector count in an image slot).
If a device's image slots have been configured with `BOOT_MAX_IMG_SECTORS: 128` and use less than 128 sectors, the first record that gets written will be somewhere in the middle of the region.
For example, if a slot uses 64 sectors, the first sector index that gets swapped is 63, which corresponds to the exact halfway point within the region.

---
***Note***

*Since the scratch area only needs to record the swapping of the last sector, it uses at most `min-write-size * 3` bytes for its own status area.*

---

## [Reset recovery](#reset-recovery)

If the bootloader resets in the middle of a swap operation, the two images may be discontiguous in flash.
Bootutil recovers from this condition by using the image trailers to determine how the image parts are distributed in flash.

First, it determines where the relevant swap status region is located.
Because this region is embedded within the image slots, its location in the flash memory changes during a swap operation.

The set of tables shown below maps image trailers content to swap status location.
In these tables, the `source` field indicates where the swap status region is located:

- If the swap status is found on the scratch area then it might not belong to the current image.
  The swap_info field of the swap status stores the corresponding image number.
- If it does not match then `source: none` is returned.

In the case of a multi-image boot, the images' primary area and the single scratch area are always examined in pairs.

```
              | primary slot | scratch      |
    ----------+--------------+--------------|
        magic | Good         | Any          |
    copy-done | 0x01         | N/A          |
    ----------+--------------+--------------'
    source: none                            |
    ----------------------------------------'

              | primary slot | scratch      |
    ----------+--------------+--------------|
        magic | Good         | Any          |
    copy-done | 0xff         | N/A          |
    ----------+--------------+--------------'
    source: primary slot                    |
    ----------------------------------------'

              | primary slot | scratch      |
    ----------+--------------+--------------|
        magic | Any          | Good         |
    copy-done | Any          | N/A          |
    ----------+--------------+--------------'
    source: scratch                         |
    ----------------------------------------'

              | primary slot | scratch      |
    ----------+--------------+--------------|
        magic | Unset        | Any          |
    copy-done | 0xff         | N/A          |
    ----------+--------------+--------------|
    source: primary slot                    |
    ----------------------------------------+------------------------------+
    This represents one of two cases:                                      |
    o No swaps ever (no status to read, so no harm in checking).           |
    o Mid-revert; status in the primary slot.                              |
    For this reason, we assume the primary slot as source, to trigger a    |
    check of the status area and find out if there was swapping underway.  |
    -----------------------------------------------------------------------'
```

- If the swap status region indicates that the images are not contiguous, MCUboot determines the type of swap operation that was interrupted by reading the `swap info` field in the active image trailer and extracting the swap type from bits 0-3.
  Then, it resumes the operation.
  In other words, it applies the procedure defined in the previous section, moving image 1 into the primary slot and image 0 into the secondary slot.
- If the boot status indicates that an image part is present in the scratch area, this part is copied into the correct location by starting at step e or step h in the area-swap procedure, depending on whether the part belongs to image 0 or image 1.

After the swap operation has been completed, the bootloader proceeds as if it had just been started.

## [Integrity check](#integrity-check)

An image is checked for integrity immediately before it gets copied into the primary slot.
If the bootloader doesn't perform an image swap, then it can perform an optional integrity check of the image in the primary slot if `MCUBOOT_VALIDATE_PRIMARY_SLOT` is set, otherwise, it doesn't perform an integrity check.

During the integrity check, the bootloader verifies the following aspects of an image:

  * The 32-bit magic number must be correct (`IMAGE_MAGIC`).
  * The image must contain an `image_tlv_info` struct, identified by its magic (`IMAGE_TLV_PROT_INFO_MAGIC` or `IMAGE_TLV_INFO_MAGIC`) exactly following the firmware (`hdr_size` + `img_size`).
    If `IMAGE_TLV_PROT_INFO_MAGIC` is found, then, after `ih_protect_tlv_size` bytes, another `image_tlv_info` with magic equal to `IMAGE_TLV_INFO_MAGIC` must be present.
  * The image must contain a SHA256 TLV.
  * The calculated SHA256 must match SHA256 TLV content.
  * The image *may* contain a signature TLV.
    If it does, it must also have a KEYHASH TLV with the hash of the key that was used to sign.
    The list of keys will then be iterated over looking for the matching key, which then will be used to verify the image content.

## [Security](#security)

As indicated above, the final step of the integrity check is the signature verification.
The bootloader can have one or more public keys embedded in it at build time.
During signature verification, the bootloader verifies that an image was signed with a private key that corresponds to the embedded KEYHASH TLV.

For information on embedding public keys in the bootloader, as well as producing signed images, see [imgtool](imgtool.md).

If you want to enable and use encrypted images, see [encrypted images](encrypted_images.md).

---
***Note***

*Image encryption is not supported when the direct-xip or the ram-load upgrade strategy is selected.*

---

### [Using hardware keys for verification](#hw-key-support)

By default, the whole public key is embedded in the bootloader code and its hash is added to the image manifest as a KEYHASH TLV entry.
As an alternative, the bootloader can be made independent of the keys by setting the `MCUBOOT_HW_KEY` option.
In this case, the hash of the public key must be provisioned to the target device and MCUboot must be able to retrieve the key-hash from there.
For this reason the target must provide a definition for the `boot_retrieve_public_key_hash()` function which is declared in `boot/bootutil/include/bootutil/sign_key.h`.
It is also required to use the `full` option for the `--public-key-format` imgtool argument to add the whole public key (PUBKEY TLV) to the image manifest instead of its hash (KEYHASH TLV).

During the boot, the public key is validated before being used for signature verification.
MCUboot calculates the hash of the public key from the TLV area and compares it with the key hash that was retrieved from the device.
This way MCUboot is independent of the public key(s).
The key(s) can be provisioned at any time and by different parties.

## [Protected TLVs](#protected-tlvs)

If the TLV area contains protected TLV entries, by beginning with a `struct image_tlv_info` with a magic value of `IMAGE_TLV_PROT_INFO_MAGIC`, then the data of those TLVs must also be integrity and authenticity protected.
Beyond the full size of the protected TLVs being stored in the `image_tlv_info`, the size of the protected TLVs is also saved, together with the size of the `image_tlv_info` struct itself, in the `ih_protected_size` field inside the header.

Whenever an image has protected TLVs, the SHA256 has to be calculated, not just over the image header and the image, but also over the TLV info header and the protected TLVs.

```
A +---------------------+
  | Header              | <- struct image_header
  +---------------------+
  | Payload             |
  +---------------------+
  | TLV area            |
  | +-----------------+ |    struct image_tlv_info with
  | | TLV area header | | <- IMAGE_TLV_PROT_INFO_MAGIC (optional)
  | +-----------------+ |
  | | Protected TLVs  | | <- Protected TLVs (struct image_tlv)
B | +-----------------+ |
  | | TLV area header | | <- struct image_tlv_info with IMAGE_TLV_INFO_MAGIC
C | +-----------------+ |
  | | SHA256 hash     | | <- hash from A - B (struct image_tlv)
D | +-----------------+ |
  | | Keyhash         | | <- indicates which pub. key for sig (struct image_tlv)
  | +-----------------+ |
  | | Signature       | | <- signature from C - D (struct image_tlv), only hash
  | +-----------------+ |
  +---------------------+
```

## [Dependency check](#dependency-check)

MCUboot can handle multiple firmware images.
It is possible to update them independently but, in many cases, it can be desired to be able to describe dependencies between the images (namely to ensure API compliance and avoid interoperability issues).

The dependencies between images can be described with additional TLV entries, located in the protected TLV area after the end of an image.
There can be more than one dependency entry but, in practice, if the platform only supports two individual images, then there can be a maximum of one entry that refers to the other image.

At the phase of dependency check, all aborted swaps are finalized, if there were any.
During the dependency check, the bootloader verifies whether the image dependencies are all satisfied.
If at least one of the dependencies of an image is not fulfilled, the swap type of that image has to be modified accordingly and the dependency check needs to be restarted.
This way the number of unsatisfied dependencies will decrease or remain the same.
There is always at least 1 valid configuration.
In the worst case, the system returns to the initial state after the dependency check.

For more information on adding dependency entries to an image, see [Image tools](imgtool.md).

## [Downgrade prevention](#downgrade-prevention)

Downgrade prevention is a feature that enforces that the new image must have a higher version or security counter number than the image it is replacing, thus preventing the malicious downgrading of the device to an older and possibly vulnerable version of its firmware.

### [Software-based downgrade prevention](#sw-downgrade-prevention)

During the software-based downgrade prevention, the image version numbers are compared.
This feature is enabled with the `MCUBOOT_DOWNGRADE_PREVENTION` option.
In this case, downgrade prevention is only available when the overwrite-based image update strategy is used (namely `MCUBOOT_OVERWRITE_ONLY` is set).

### [Hardware-based downgrade prevention](#hw-downgrade-prevention)

Each signed image can contain a security counter in its protected TLV area, which can be added to the image using the `-s` option of the [imgtool](imgtool.md) script.
During the hardware-based downgrade prevention (also called rollback protection) the new image's security counter will be compared with the currently active security counter value, which must be stored in a non-volatile and trusted component of the device.
It is beneficial to handle this counter independently from the image version number:

  - It does not need to increase with each software release,
  - It makes it possible to do software downgrade to some extent:
    if the security counter has the same value in the older image, then it is accepted.

It is an optional step of the image validation process and can be enabled with the `MCUBOOT_HW_ROLLBACK_PROT` config option.
When enabled, the target must provide an implementation of the interface of the security counter defined in `boot/bootutil/include/security_cnt.h`.

## [Measured boot and data sharing](#boot-data-sharing)

MCUBoot defines both a mechanism for sharing boot status information (also known as measured boot) and an interface for sharing application-specific information with the runtime software.
If any of these mechanisms are enabled, the target must provide a shared data area between the bootloader and the runtime firmware and define the following parameters:

```c
#define MCUBOOT_SHARED_DATA_BASE    <area_base_addr>
#define MCUBOOT_SHARED_DATA_SIZE    <area_size_in_bytes>
```

In the shared memory area all data entries are stored in a type-length-value (TLV) format.
Before adding the first data entry, the whole area is overwritten with zeros and a TLV header is added at the beginning of the area during the initialization phase.
This TLV header contains a `tlv_magic` field with a value of `SHARED_DATA_TLV_INFO_MAGIC` and a `tlv_tot_len` field which is indicating the total length of the shared TLV area including this header.

The header is followed by the data TLV entries which are composed of a `shared_data_tlv_entry` header and the data itself.
In the data header, there is a `tlv_type` field that identifies the consumer of the entry (in the runtime software) and specifies the subtype of that data item.

More information about the `tlv_type` field and data types can be found in the `boot/bootutil/include/bootutil/boot_status.h` file.

The type is followed by a `tlv_len` field which indicates the size of the data entry in bytes, not including the entry header.
After this header structure comes the actual data.

```c
/** Shared data TLV header.  All fields in little endian. */
struct shared_data_tlv_header {
    uint16_t tlv_magic;
    uint16_t tlv_tot_len; /* size of whole TLV area (including this header) */
};

/** Shared data TLV entry header format. All fields in little endian. */
struct shared_data_tlv_entry {
    uint16_t tlv_type;
    uint16_t tlv_len; /* TLV data length (not including this header). */
};
```

The measured boot can be enabled with the `MCUBOOT_MEASURED_BOOT` config option.
When enabled, the `--boot_record` argument of the imgtool script must also be used during the image signing process to add a BOOT_RECORD TLV to the image manifest.
This TLV contains the following attributes/measurements of the image in CBOR encoded format:

  * Software type (role of the software component)
  * Software version
  * Signer ID (identifies the signing authority)
  * Measurement value (hash of the image)
  * Measurement type (algorithm used to calculate the measurement value)

The `sw_type` string that is passed as the `--boot_record` option's parameter will be the value of the "Software type" attribute in the generated BOOT_RECORD TLV.
The target must also define the `MAX_BOOT_RECORD_SZ` macro which indicates the maximum size of the CBOR-encoded boot record in bytes.
During the boot, MCUBoot looks for these TLVs (in case of multiple images) in the manifests of the active images (the latest and validated) and copies the CBOR-encoded binary data to the shared data area.
Preserving all these image attributes from the boot stage for use by later runtime services (such as an attestation service) is known as a measured boot.

Setting the `MCUBOOT_DATA_SHARING` option enables the sharing of application-specific data using the same shared data area, as for the measured boot.
For this, the target must provide a definition for the `boot_save_shared_data()` function which is declared in `boot/bootutil/include/bootutil/boot_record.h`.
The `boot_add_data_to_shared_area()` function can be used for adding new TLV entries to the shared data area.

## [Testing Fault Injection Hardening (FIH) in CI](#testing-fih-in-ci)

The CI currently tests the Fault Injection Hardening feature of MCUboot by executing instruction skip during execution, and looking at whether a corrupted image was booted by the bootloader or not.

The main idea is that instruction skipping can be automated by scripting a debugger to automatically execute the following steps:

- Set breakpoint at the specified address.
- Continue execution.
- On breakpoint hit to increase the Program Counter.
- Continue execution.
- Detach from the target after a timeout reached.

Whether or not the corrupted image was booted or not can be decided by looking for certain entries in the log.

As MCUboot is deployed on a microcontroller, testing FI would not make much sense in the simulator environment running on a host machine with a different architecture than the MCU's, as the degree of hardening depends on the compiler behavior.
For example, (a bit counterintuitively) the code produced by gcc with `-O0` optimization is more resilient against FI attacks than the code generated with `-O3` or `-Os` optimizations.

To run on the desired architecture in the CI, the tests need to be executed on an emulator (as real devices are not available in the CI environment).
For this implementation, QEMU is selected.

For the tests, MCUboot needs a set of drivers and an implementation of the main function.
For this test, Trusted-Firmware-M has been selected as it supports Armv8-M platforms that are also emulated by QEMU.

The tests run in a docker container inside the CI VMs, to make it easier to deploy, build, and test environments (QEMU, compilers, interpreters).
The CI VMs seem to be using a relatively old version of Ubuntu (16.04).

The sequence of the testing is the following (pseudo code):

```sh
fn main()
  # Implemented in ci/fih-tests_install.sh
  generate_docker_image(Dockerfile)

  # See details below. Implemented in ci/fih-tests_run.sh.
  # Calling the function with different parameters is done by Travis CI based on
  # the values provided in the .travis.yaml
  start_docker_image(skip_sizes, build_type, damage_type, fih_level)

fn start_docker_image(skip_sizes, build_type, damage_type, fih_level)
  # implemented in ci/fih_test_docker/execute_test.sh
  compile_mcuboot(build_type)

  # implemented in ci/fih_test_docker/damage_image.py
  damage_image(damage_type)

  # implemented in ci/fih_test_docker/run_fi_test.sh
  ranges = generate_address_ranges()
  for s in skip_sizes
    for r in ranges
      do_skip_in_qemu(s, r) # See details below
  evaluate_logs()

fn do_skip_in_qemu(size, range)
  for a in r
    run_qemu(a, size)  # See details below

# this part is implemented in ci/fih_test_docker/fi_tester_gdb.sh
fn run_qemu(a, size)
  script = create_debugger_script(a, size)
  start_qemu_in_bacground() # logs serial out to a file
  gdb_attach_to_qemu(script)
  kill_qemu()

  # This checks the debugger and the QEMU logs, and decides whether the test
  # was executed successfully, and whether the image is booted or not. Then
  # emits a yaml fragment on the standard out to be processed by the caller
  # script
  evaluate_run(qemu_log_file)
```

Further notes:

- The image is corrupted by changing its signature.
- `MCUBOOT_FIH_PROFILE_MAX` is not tested as it requires TRNG, and the AN521 platform has no support for it.
  However, this profile adds the random execution delay to the code, so should not affect the instruction skip results too much, because the breakpoint is placed at the exact address.
  But, in practice, this makes harder the accurate timing of the attack.
- The test cases defined in `.travis.yml` always return `passed`, if they were executed successfully.
  A yaml file is created during the test execution with the details of the test execution results.
  A summary of the collected results is printed in the log at the end of the test.

An advantage of having the tests running in a docker image is that it is possible to run the tests on a local machine that has git and docker, without installing any additional software.

So, running the test on the host looks like the following (the commands below are issued from the MCUboot source directory):

```sh
$ ./ci/fih-tests_install.sh
$ FIH_LEVEL=MCUBOOT_FIH_PROFILE_MEDIUM BUILD_TYPE=RELEASE SKIP_SIZE=2 \
    DAMAGE_TYPE=SIGNATURE ./ci/fih-tests_run.sh
```
On the travis CI, the environment variables in the last command are set based on the configs provided in the `.travis.yml`

This starts the tests, however, the shell that it is running in is not interactive.
As such, it is not possible to examine the results of the test run.
To have an interactive shell where the results can be examined, do the following:

- The docker image needs to be built with `ci/fih-tests_install.sh`, as described above.
- Start the docker image with the following command:
  `docker run -i -t mcuboot/fih-test`.
- Execute the test with a command similar to the following:
  `/root/execute_test.sh 8 RELEASE SIGNATURE MEDIUM`.
  After the test finishes, the shell returns, and it is possible to investigate the results.
  It is also possible to stop the test with _Ctrl+C_.
  The parameters to the `execute_test.sh` are, in order, `SKIP_SIZE`, `BUILD_TYPE`, `DAMAGE_TYPE`, and `FIH_LEVEL`.
