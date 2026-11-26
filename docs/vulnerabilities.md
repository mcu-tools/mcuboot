# MCUboot vulnerabilities

This page collects the security vulnerabilities fixed in MCUboot.
It often has more detail than the release notes. Because the
release notes are locked to a version, the information here can
be updated after a release, for instance once an embargo is
lifted.

The entries are grouped by the MCUboot release that first
contains the fix, newest release first.

- Table of Contents
{:toc}

## Fixed on main, after v2.5.0

These fixes are on `main` and will be in the next release.

### CVE-2026-16664

Unchecked serial-recovery image index causes out-of-bounds slot-table
read in MCUboot Zephyr port

MCUboot's Zephyr serial-recovery upload handler `bs_upload()` in
`boot/boot_serial/src/boot_serial.c` decodes the host-supplied `image`
field of an MCUmgr image-upload request into `img_num` (a full 32-bit
value) and latches it on the `off == 0` packet without any range check.
It then passes `img_num` straight to
`flash_area_id_from_multi_image_slot(img_num, 0)`, which on the Zephyr
port resolves to `__flash_area_ids_for_slot()` in
`boot/zephyr/include/sysflash/sysflash.h` and indexes the static `const
int all_slots[]` table as `all_slots[img_num * 2]`. That table holds
only `MCUBOOT_IMAGE_NUMBER * 2` entries, so any `img_num >=
MCUBOOT_IMAGE_NUMBER` is an out-of-bounds read.

The path is reachable by anyone with access to the serial / SMP recovery
console, which is unauthenticated by design, and the bad value is
supplied before any image signature is verified. The defect is specific
to the default non-direct upload path
(`MCUBOOT_SERIAL_DIRECT_IMAGE_UPLOAD` disabled) on the Zephyr port with
the array-backed slot layout (not `CONFIG_SINGLE_APPLICATION_SLOT`). It
is not gated on a multi-image configuration: `MCUBOOT_IMAGE_NUMBER`
defaults to 1, so even a default single-image build is affected for any
`image >= 1`. The `MCUBOOT_SERIAL_DIRECT_IMAGE_UPLOAD` path is safe (its
`flash_area_id_from_direct_image()` switch returns `-EINVAL` for unknown
indices).

`flash_area_id_from_multi_image_slot()` takes a **signed** `int
image_index` while `img_num` is `uint32_t`, so the attacker controls the
sign of the index as well as its magnitude: an `image` of `0xFFFFFFFE`
converts to `-2` and reads `all_slots[-4]`, off the front of the table,
while large positive values read off the end (CWE-192). (`0xFFFFFFFF`
itself is the "no image given" sentinel and selects image 0.) Either
way the index multiply `img * 2` is performed in `int`, so a wild offset
yields a bus/hard fault, giving a reliable denial of service of the
bootloader during recovery. For moderately out-of-range indices the read
returns adjacent `.rodata` as a garbage flash-area id that is handed to
`flash_area_open()`; this normally fails with `MGMT_ERR_EINVAL`, but if
the garbage id happens to match a valid non-image partition, the
subsequent erase/write can target a partition outside the image set
(CWE-787). The attacker-supplied value is never echoed back, so there is
no information disclosure.

The fix rejects `img_num >= BOOT_IMAGE_NUMBER` on the `off == 0`
packet, before the slot lookup, comparing the unsigned `img_num` so that
the negative case above is also rejected, and makes
`__flash_area_ids_for_slot()` reject an out-of-range image or slot.
v2.5.0 contains only part of this. Its check,
`img_num > BOOT_IMAGE_NUMBER`, covers the negative and the large
indices but still accepts `img_num == BOOT_IMAGE_NUMBER`, which reads
the entry just past the end of the table, including in a single-image
build. PR 2967 corrects the comparison and adds the bounds check in the
lookup.

- [CVE-2026-16664](https://www.cve.org/CVERecord?id=CVE-2026-16664)
- [MCUboot advisory GHSA-8rcq-2mgr-fhp8](https://github.com/mcu-tools/mcuboot/security/advisories/GHSA-8rcq-2mgr-fhp8)
- Partially fixed in v2.5.0 by [PR 2836](https://github.com/mcu-tools/mcuboot/pull/2836)
- Fixed by [PR 2967](https://github.com/mcu-tools/mcuboot/pull/2967)

## Fixed in v2.5.0

### CVE-2026-16659

Pre-authentication NULL pointer dereference in MCUboot
serial-recovery image upload (encrypted-image builds)

MCUboot's serial-recovery image-upload handler `bs_upload()` in
`boot/boot_serial/src/boot_serial.c` declares `const struct flash_area
*fap = NULL` and assigns it only via `flash_area_open()` part-way
through the function. Two early validation failures (`goto
out_invalid_data`) — a CBOR map that fails to decode, or an upload
packet missing the `off`/`data` members — branch past the open while
`fap` is still `NULL`, then fall into the shared epilogue. In
`MCUBOOT_ENC_IMAGES` builds that epilogue unconditionally calls
`boot_handle_enc_fw(fap)`, which reaches `boot_image_load_header()` and
`flash_area_read()` and dereferences the `NULL` `flash_area`.

The defect is reachable pre-authentication: the trigger is the very
first serial-recovery upload command carrying a malformed payload. The
guarding conditions are default-true on a fresh recovery session —
`img_num`, `curr_off` and `img_size` are zero-initialized `static`s, so
`curr_off == img_size` and the primary-slot test both hold. It requires
a non-default build: encrypted images (`MCUBOOT_ENC_IMAGES` / Zephyr
`CONFIG_BOOT_ENCRYPT_IMAGE`) together with an active serial-recovery
transport, on the Zephyr or Mynewt port (the two that build both
`boot/boot_serial/src/boot_serial.c` and `boot_serial_encryption.c` with
`MCUBOOT_ENC_IMAGES`).

The impact is a transient denial of service of the bootloader during
recovery. `fap` is a hard `NULL` rather than an attacker-controlled
value, so there is no memory-corruption or information-leak primitive
and image authentication is unaffected. Whether the dereference faults
is platform-dependent — on targets where address `0x0` is mapped
readable flash the read merely returns an error and execution continues,
while targets that trap NULL (MPU / null-pointer-exception detection)
take a hard fault. Because serial recovery is unauthenticated by design
and already permits the console attacker to write the secondary slot,
the added capability is only a low-value transient crash.

The related unconditional `flash_area_close(fap)` call in the same
epilogue is a safe no-op on the ports that reach it, so it does not
contribute to the dereference.

The affected ports are the Zephyr port — `boot/zephyr/CMakeLists.txt`
compiles `boot_serial.c` under `CONFIG_MCUBOOT_SERIAL` and
`boot_serial_encryption.c` under `CONFIG_BOOT_ENCRYPT_IMAGE` — and the
Mynewt port, whose `mcuboot_config.h` defines `MCUBOOT_SERIAL`
unconditionally and `MCUBOOT_ENC_IMAGES` under any `BOOTUTIL_ENCRYPT_*`
syscfg, with `boot/boot_serial/pkg.yml` declaring no explicit source
list so the whole directory is built. The **Espressif port is not
affected**: it compiles `boot_serial.c` but not
`boot_serial_encryption.c`, and defines no `MCUBOOT_ENC_IMAGES`, so the
`#ifdef MCUBOOT_ENC_IMAGES` block containing the call is preprocessed
away. The NuttX, Cypress and Mbed ports do not build `boot_serial.c` at
all.

- [CVE-2026-16659](https://www.cve.org/CVERecord?id=CVE-2026-16659)
- [MCUboot advisory GHSA-777r-3chm-p78c](https://github.com/mcu-tools/mcuboot/security/advisories/GHSA-777r-3chm-p78c)
- Fixed by [PR 2837](https://github.com/mcu-tools/mcuboot/pull/2837)

### CVE-2026-16662

Uninitialised bootloader stack disclosed into the measured-boot
attestation record via a short EXPECTED_HASH TLV in MCUboot

`boot_save_boot_status()` in `boot/bootutil/src/boot_record.c` reads the
image's `EXPECTED_HASH_TLV` into a stack buffer
`image_hash[IMAGE_HASH_SIZE]` that is never zero-initialised. The TLV
handler validates only the upper bound (`len > sizeof(image_hash)`) and
then reads exactly `len` bytes, so a TLV shorter than `IMAGE_HASH_SIZE`
leaves the tail of `image_hash` holding stale stack content. The
subsequent copy into the boot record, `memcpy(buf + offset, image_hash,
sizeof(image_hash))`, uses the fixed buffer size rather than `len`, so
up to `IMAGE_HASH_SIZE - 1` bytes of uninitialised bootloader stack are
written into the measured-boot record.

The defect is reachable only under `MCUBOOT_MEASURED_BOOT`, and only
when nothing has checked the length of the hash TLV before
`boot_save_boot_status()` reads it. `bootutil_img_validate()` rejects a
hash TLV whose length is not exactly `sizeof(hash)`
(`boot/bootutil/src/image_validate.c:345`), and primary-slot validation
is on by default (Zephyr's `CONFIG_BOOT_VALIDATE_SLOT0` defaults to
`y`), so in a default build the malformed image never reaches this code.
Two kinds of build let a short TLV through:

- Builds that use pure signatures (`MCUBOOT_SIGN_PURE`, Zephyr
  `CONFIG_BOOT_SIGNATURE_TYPE_PURE`). The signature covers the image
  itself, so `bootutil_img_validate()` does not check the hash TLV at
  all, and a pure-signed image normally carries none. A hash TLV is
  still allowed in the unprotected TLV area, which the signature does
  not cover, so a short one can be added to a correctly signed image
  without invalidating it. This applies with every upgrade strategy,
  including direct-XIP and RAM-load, and the modified image can be
  delivered through any path that accepts a signed image.
- Builds with primary-slot validation turned off: the swap and
  overwrite loader built without `MCUBOOT_VALIDATE_PRIMARY_SLOT`, which
  checks only the image magic before calling `boot_add_shared_data()`,
  and the Zephyr single-slot loader (`boot/zephyr/single_loader.c`)
  built without it, including the `MCUBOOT_VALIDATE_PRIMARY_SLOT_ONCE`
  variant, which skips validation once the trailer records an earlier
  successful one. Placing the crafted image requires physical or
  flash-write access to the device; in a single-slot build with serial
  recovery enabled, the serial-recovery upload writes the primary slot
  directly and can place it as well. Outside pure-signature builds, the
  direct-XIP and RAM-load strategies in `boot/bootutil/src/loader.c`
  validate the selected slot unconditionally and are not affected.

It is also reachable through a time-of-check/time-of-use change to
tamperable external flash between validation and the boot-record read.

In these configurations the boot record's measurement is already not
trustworthy: the hash copied into the record is the image's own
`EXPECTED_HASH_TLV`, which nothing has verified and which the attacker
chooses. The defect therefore does not weaken attestation integrity any
further. What it adds is a path for bootloader memory to leave the
device: up to 63 bytes (`IMAGE_HASH_SIZE - 1`) of uninitialised
bootloader stack are embedded in the measurement field that a
downstream attestation service sends to a remote verifier. In the
configurations analysed the residue is bootloader internal state (TLV
offsets and lengths, flash addresses, loop state), not key material,
but what occupies that stack region depends on configuration and on
which code ran before `boot_save_boot_status()`. It is reported as a
low-severity, defense-in-depth disclosure with no integrity or
availability impact.

This is distinct from the already-fixed `record_len` integer underflow
in the same routine (GHSA-8hrv-4cp5-4rg3). The fix rejects any
`EXPECTED_HASH_TLV` whose length is not exactly `IMAGE_HASH_SIZE`.

- [CVE-2026-16662](https://www.cve.org/CVERecord?id=CVE-2026-16662)
- [MCUboot advisory GHSA-779w-4x76-5chf](https://github.com/mcu-tools/mcuboot/security/advisories/GHSA-779w-4x76-5chf)
- Fixed by [PR 2802](https://github.com/mcu-tools/mcuboot/pull/2802)

### CVE-2026-16671

Out-of-bounds stack write in MCUboot split-image validation with
SHA-384/SHA-512 builds

`split_image_check()` in `boot/bootutil/src/loader.c` declares a fixed
`uint8_t loader_hash[32]` buffer and passes it as the `out_hash`
out-parameter to `bootutil_img_validate()`. That routine copies
`IMAGE_HASH_SIZE` bytes into `out_hash` (`memcpy(out_hash, hash,
IMAGE_HASH_SIZE)` in `boot/bootutil/src/image_validate.c`).
`IMAGE_HASH_SIZE` is a compile-time constant
(`boot/bootutil/include/bootutil/crypto/sha.h`): `64` when
`MCUBOOT_SHA512` is defined, and `48` when `MCUBOOT_SIGN_EC384` is
defined, which selects SHA-384 (there is no separate SHA-384 option).
The buffer tracks SHA-256 (32 bytes) while the copy length tracks the
configured algorithm, so the write overruns the buffer by 16 or 32
bytes.

The overflow is only present in a non-default configuration: the legacy
Mynewt split-image feature must be compiled
(`split_go()`/`split_image_check()`, i.e. neither `MCUBOOT_DIRECT_XIP`
nor `MCUBOOT_RAM_LOAD`), the digest must be larger than 32 bytes
(`MCUBOOT_SHA512` or `MCUBOOT_SIGN_EC384`), and `MCUBOOT_SIGN_PURE` must
be off. `split_go()` is a public API with no caller inside MCUboot; it
is invoked from a Mynewt application. In the default SHA-256 build there
is no overflow.

The over-written bytes are the one-way digest of the loader image and
the overflow length is fixed at build time, so the contents are not
meaningfully attacker-controlled and the size is not attacker-chosen.
The realistic impact is bounded, uncontrolled corruption of the
bootloader's stack on the split-boot path, manifesting as instability or
failed boot in the affected configuration rather than reliable code
execution. The fix is to size `loader_hash` to `IMAGE_HASH_SIZE` and
pass `IMAGE_HASH_SIZE` as the matching seed length.

- [CVE-2026-16671](https://www.cve.org/CVERecord?id=CVE-2026-16671)
- [MCUboot advisory GHSA-vj6q-r752-fpqv](https://github.com/mcu-tools/mcuboot/security/advisories/GHSA-vj6q-r752-fpqv)
- Fixed by [PR 2817](https://github.com/mcu-tools/mcuboot/pull/2817)

### CVE-2026-127915

TLV iterator does not bound each entry against the declared TLV area in
MCUboot bootutil

The MCUboot TLV iterator `bootutil_tlv_iter_next()` in
`boot/bootutil/src/tlv.c` walks image trailer TLVs with a single loop
guard (`it->tlv_off < it->tlv_end`) and advances by `sizeof(struct
image_tlv) + tlv.it_len` using the attacker-controlled `it_len` field,
without validating that the entry's header or payload stays within
`it->tlv_end`. As a result the 4-byte header read can extend up to 3
bytes past the declared TLV area, and the returned `(off, len)` can
describe a payload that runs past it.

The path is pre-authentication: TLVs are parsed during
`bootutil_img_validate()` before the signature is verified, and a
crafted image can be presented through DFU / serial-recovery / OTA.
However the impact is contained by surrounding guards:
`bootutil_img_validate()` first rejects a TLV area larger than
`bootutil_max_image_size()`, the default `LOAD_IMAGE_DATA` sink is
`flash_area_read()` which bounds-checks against the flash partition and
fails an out-of-partition read, and every payload consumer
(`EXPECTED_HASH_TLV`, `EXPECTED_KEY_TLV`, `EXPECTED_SIG_TLV`,
`IMAGE_TLV_SEC_CNT`) clamps `len` against a fixed buffer before copying.

The residual effect in a default flash build is therefore an
out-of-region read confined to the attacker's own slot partition whose
result is never observable, yielding at worst a clean validation failure
rather than memory corruption or information disclosure. A genuine
(though still bounded) out-of-bounds read remains under the non-default
`MCUBOOT_RAM_LOAD` configuration, where `LOAD_IMAGE_DATA` is an
unchecked `memcpy`. The fix is to validate each entry's header and
payload against `it->tlv_end` inside the iterator.

- [CVE-2026-127915](https://www.cve.org/CVERecord?id=CVE-2026-127915)
- [MCUboot advisory GHSA-4fp3-3qw2-25jv](https://github.com/mcu-tools/mcuboot/security/advisories/GHSA-4fp3-3qw2-25jv)
- Fixed by [PR 2733](https://github.com/mcu-tools/mcuboot/pull/2733)

### CVE-2026-127916

MCUboot image hash span is not bounded against the slot, allowing a read
past the flash area during image validation

`bootutil_img_hash()` computes the image digest over `ih_hdr_size +
ih_img_size + ih_protect_tlv_size` bytes, all three taken from the
candidate image's header, and reads that span out of the slot without
checking that it fits in the flash area. The three sizes are also summed
without an overflow check. Nothing earlier on the validation path bounds
the whole span either: `bootutil_img_validate()` compares the image size
against `bootutil_max_image_size()` only after the hash has been
computed.

The callers bound part of the span, or none of it:

- On the swap and overwrite upgrade path, `boot_validate_slot()` first
  runs the image header check (`boot_is_header_valid()`, later
  `boot_check_header_valid()`). Before MCUboot 2.2.0 that check covered
  only `ih_hdr_size + ih_img_size`, so for every image the protected-TLV
  area (`ih_protect_tlv_size`, a `uint16_t`, up to 65535 bytes) was
  hashed without being bounded. From 2.2.0 the check includes
  `ih_protect_tlv_size`, except for a compressed image in the secondary
  slot when `MCUBOOT_DECOMPRESS_IMAGES` (`CONFIG_BOOT_DECOMPRESSION`) is
  enabled, which leaves the same gap of up to 64 KB.
- Zephyr's single-slot and firmware loaders
  (`boot/zephyr/single_loader.c`, `boot/zephyr/firmware_loader.c`) read
  the header with `boot_image_load_header()`, which bounds
  `ih_hdr_size + ih_img_size` against the flash area. Before MCUboot
  2.4.0 that bound did not include `ih_protect_tlv_size`, which leaves
  the same gap of up to 64 KB. From 2.4.0 these loaders bound the whole
  span.
- The serial recovery image-list and image-state handlers (`bs_list()`
  and `bs_set()` in `boot/boot_serial/src/boot_serial.c`) call
  `bootutil_img_validate()` with no header check at all, so on those
  paths `ih_img_size` is unbounded as well.

The image is hashed before its signature is checked, so anyone who can
place an image in a slot (an OTA or SMP update delivered through the
application, or serial recovery) reaches this read before
authentication.

Whether a malformed header produces an actual read beyond the area
depends on the flash backend:

- Backends whose `flash_area_read()` bounds-checks the request (Zephyr,
  Espressif, NuttX, Mbed) return an error once the chunked read crosses
  the end of the area. Validation fails and the image is rejected, with
  no over-read.
- With `MCUBOOT_HASH_STORAGE_DIRECTLY` (Zephyr
  `CONFIG_BOOT_IMG_HASH_DIRECTLY_ON_STORAGE`, available from 2.2.0, not
  enabled by default) the span is hashed through a single direct pointer
  into memory-mapped flash, with no bounds check.
- The Cypress PSoC6 port's `flash_area_read()`
  (`boot/cypress/cy_flash_pal/cy_flash_map.c`, from 1.6.0) does not
  bound the read: its `assert()`s compare against the area's absolute
  offset rather than its size, and it copies with `memcpy()`.

The bytes read past the area feed only the digest, which is compared
internally and never returned, so nothing is disclosed. The realistic
effect is a read fault during image validation: up to 64 KB past the
slot on the upgrade path and, before 2.4.0, on the Zephyr loaders, and
potentially much further on the serial recovery handlers, which skip the
header check. Where validation is repeated at every boot (a pending
upgrade, or, before 2.4.0, the single-slot loader checking its image),
the fault can recur on each reset and keep the device from booting.

The fix makes `bootutil_img_hash()` add the three sizes with
`boot_u32_safe_add()` and reject an image whose span, including the
sector offset used in swap-using-offset mode, does not fit in the flash
area, before any of it is hashed. Because the bound is applied where the
bytes are read, it covers every caller.

- [CVE-2026-127916](https://www.cve.org/CVERecord?id=CVE-2026-127916)
- [MCUboot advisory GHSA-c7cx-5pqv-x9m6](https://github.com/mcu-tools/mcuboot/security/advisories/GHSA-c7cx-5pqv-x9m6)
- Fixed by [PR 2819](https://github.com/mcu-tools/mcuboot/pull/2819)

### CVE-2026-127928

AES image key left on the stack after serial-recovery encrypted-image
handling in MCUboot

In MCUboot's serial-recovery encrypted-image support
(`boot/boot_serial/src/boot_serial_encryption.c`),
`decrypt_image_inplace()` and `boot_image_validate_encrypted()` each
keep a stack-local `struct boot_status`. `boot_enc_load()` writes the
plaintext per-image AES key into its `enckey[]` field. Neither function
clears this structure before returning. In addition, a decrypt failure
inside the per-sector loop of `decrypt_image_inplace()` returns early
and skips the `total_out:` cleanup, which leaves the expanded AES key
context (`enc_data`) on the stack as well. The error paths of
`boot_image_validate_encrypted()` similarly skip `boot_enc_zeroize()`.

The affected code is built when encrypted images (`MCUBOOT_ENC_IMAGES`,
Zephyr `CONFIG_BOOT_ENCRYPT_IMAGE`) are combined with serial recovery.
It runs when an encrypted image is uploaded to the primary slot over the
recovery protocol, and when the image list or state is queried while an
encrypted image is in the secondary slot. After these functions return,
the image's AES content key remains in dead stack memory. An attacker
who can later read device RAM, for example through a debug port or
retained RAM, can recover it.

This weakens the confidentiality of image-encryption keys within the
bootloader trust boundary. A recovered key exposes the plaintext of the
encrypted firmware image it protects. The device's long-term private
decryption key is not exposed. No remote or recovery-protocol channel
that returns this memory is known, so exploitation requires a separate
memory-read capability.

- [CVE-2026-127928](https://www.cve.org/CVERecord?id=CVE-2026-127928)
- [MCUboot advisory GHSA-hqmg-jwp9-8cf8](https://github.com/mcu-tools/mcuboot/security/advisories/GHSA-hqmg-jwp9-8cf8)
- Fixed by [PR 2734](https://github.com/mcu-tools/mcuboot/pull/2734)

### CVE-2026-128091

AES key schedule not cleared by the TinyCrypt AES-CTR backend in MCUboot
bootutil

When MCUboot is built with the TinyCrypt crypto backend
(`MCUBOOT_USE_TINYCRYPT`), `bootutil_aes_ctr_drop()` in
`boot/bootutil/include/bootutil/crypto/aes_ctr_tinycrypt.h` is an empty
stub. The AES-CTR context for this backend is the fully expanded AES-128
key schedule (`struct tc_aes_key_sched_struct`). Generic code relies on
this hook to release and clear key state, so with TinyCrypt nothing is
cleared. The mbedTLS and PSA backends do free or destroy their key state
here.

As a result, `boot_state_clear()`, which `boot_go()` calls before
handing off to the application, leaves the image's AES
content-encryption key schedule in the static `boot_data` state. This
is the case after an encrypted upgrade in single-image builds, where
`boot_enc_zeroize()` is not called. `boot_decrypt_key()` likewise
leaves the ECIES-derived key-encryption key schedule on the stack. The
affected build uses `MCUBOOT_ENC_IMAGES` with TinyCrypt, which is the
default ECDSA and Ed25519 backend in the Zephyr port.

The first AES-128 round key equals the cipher key, so anyone who can
read the leftover bootloader RAM after boot can recover the image
content key and decrypt that encrypted image. That includes a physical
attacker with debug or RAM access, or code later running on the device.
This weakens image-encryption key confidentiality (and so the
confidentiality of the encrypted image's plaintext). The device's
long-term private decryption key is not exposed, and image authenticity
is not affected. The fix wipes the context with
`bootutil_wipe_memory()`.

- [CVE-2026-128091](https://www.cve.org/CVERecord?id=CVE-2026-128091)
- [MCUboot advisory GHSA-wx3x-935q-6rcf](https://github.com/mcu-tools/mcuboot/security/advisories/GHSA-wx3x-935q-6rcf)
- Fixed by [PR 2734](https://github.com/mcu-tools/mcuboot/pull/2734)
