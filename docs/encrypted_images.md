<!--
    -
    - Licensed to the Apache Software Foundation (ASF) under one
    - or more contributor license agreements.  See the NOTICE file
    - distributed with this work for additional information
    - regarding copyright ownership.  The ASF licenses this file
    - to you under the Apache License, Version 2.0 (the
    - "License"); you may not use this file except in compliance
    - with the License.  You may obtain a copy of the License at
    -
    -  http://www.apache.org/licenses/LICENSE-2.0
    -
    - Unless required by applicable law or agreed to in writing,
    - software distributed under the License is distributed on an
    - "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
    - KIND, either express or implied.  See the License for the
    - specific language governing permissions and limitations
    - under the License.
    -
-->

# Encrypted images

## [Rationale](#rationale)

To provide confidentiality of image data while in transport to the
device or while residing on an external flash, `MCUboot` has support
for encrypting/decrypting images on-the-fly while upgrading.

The image header needs to flag this image as `ENCRYPTED` (0x04) and
a TLV with the key must be present in the image. When upgrading the
image from the `secondary slot` to the `primary slot` it is automatically
decrypted (after validation). If swap upgrades are enabled, the image
located in the `primary slot`, also having the `ENCRYPTED` flag set and the
TLV present, is re-encrypted while swapping to the `secondary slot`.

## [Threat model](#threat-model)

The encrypted image support is supposed to allow for confidentiality
if the image is not residing on the device or is written to external
storage, eg a SPI flash being used for the secondary slot.

It does not protect against the possibility of attaching a JTAG and
reading the internal flash memory, or using some attack vector that
enables dumping the internal flash in any way.

Since decrypting requires a private key (or secret if using symmetric
crypto) to reside inside the device, it is the responsibility of the
device manufacturer to guarantee that this key is already in the device
and not possible to extract.

## [Design](#design)

When encrypting an image, only the payload (FW) is encrypted. The header,
TLVs are still sent as plain data.

Hashing and signing also remain functionally the same way as before,
applied over the un-encrypted data. Validation on encrypted images, checks
that the encrypted flag is set and TLV data is OK, then it decrypts each
image block before sending the data to the hash routines.

The image is encrypted using AES-CTR-128 or AES-CTR-256, with a counter
that starts from zero (over the payload blocks) and increments by 1 for each
16-byte block. AES-CTR was chosen for speed/simplicity and allowing for any
block to be encrypted/decrypted without requiring knowledge of any other
block (allowing for simple resume operations on swap interruptions).

The key used is a randomized when creating a new image, by `imgtool` or
`newt`. This key should never be reused and no checks are done for this,
but randomizing a 16-byte block with a TRNG should make it highly
improbable that duplicates ever happen.

To distribute this AES-CTR key, new TLVs were defined. The key can be
encrypted using either RSA-OAEP, AES-KW (128 or 256 bits depending on the
AES-CTR key length), ECIES-P256 or ECIES-X25519.

For RSA-OAEP a new TLV with value `0x30` is added to the image, for
AES-KW a new TLV with value `0x31` is added to the image, for
ECIES-P256 a new TLV with value `0x32` is added, and for ECIES-X25519 a
newt TLV with value `0x33` is added. The contents of those TLVs
are the results of applying the given operations over the AES-CTR key.

## [ECIES encryption](#ecies-encryption)

ECIES follows a well defined protocol to generate an encryption key. There are
multiple standards which differ only on which building blocks are used; for
MCUboot we settled on some primitives that are easily found on our crypto
libraries. The whole key encryption can be summarized as:

* Generate a new private key and derive the public key; when using ECIES-P256
  this is a secp256r1 keypair, when using ECIES-X25519 this will be a x25519
  keypair. Those keys will be our ephemeral keys.
* Generate a new secret (DH) using the ephemeral private key and the public key
  that corresponds to the private key embedded in the HW.
* Derive the new keys from the secret using HKDF. We are not using a `salt`
  and using an `info` of `MCUBoot_ECIES_v1`, generating 48 bytes of key material.
* A new random encryption key is generated (for AES). This is
  the AES key used to encrypt the images.
* The key is encrypted with AES-128-CTR or AES-256-CTR and a `nonce` of 0 using
  the first 16 bytes of key material generated previously by the HKDF.
* The encrypted key now goes through a HMAC using the remaining 32
  bytes of key material from the HKDF.

There are different TLVs for ECIES-P256, ECIES-X25519 with SHA256 HKDF/HMAC
and ECIES-X25519 with SHA512 HKDF/HMAC.
The final TLV is built from the 65 bytes for ECIES-P256 or 32 bytes for
ECIES-X25519, which correspond to the ephemeral public key, followed by the
MAC tag and the 16 or 32 bytes of the encrypted key, resulting in final TLV
length:
 * ECIES-P256 has TLV length 113 to 129 bytes, depending on AES key length.
 * ECIES-X25519 on SHA256 TLV length is 80 or 96 bytes, depending on AES key
   length.
 * ECIES-X25519 on SHA512 TLV length is 112 or 128, depending on AES key
   length.

The implemenation of ECIES-P256 is named ENC_EC256 in the source code and
artifacts while ECIES-X25519 is named ENC_X25519.

Note that MCUboot is built to support only one ECIES and HMAC SHA at once,
and truncated HMAC is not supported at this time

## [Upgrade process](#upgrade-process)

When starting a new upgrade process, `MCUboot` checks that the image in the
`secondary slot` has the `ENCRYPTED` flag set and has the required TLV with the
encrypted key. It then uses its internal private/secret key to decrypt
the TLV containing the key. Given that no errors are found, it will then
start the validation process, decrypting the blocks before check. A good
image being determined, the upgrade consists in reading the blocks from
the `secondary slot`, decrypting and writing to the `primary slot`.

If swap using scratch is used for the upgrade process, the decryption happens
when copying the content of the scratch area to the `primary slot`, which means
the scratch area does not contain the image unencrypted. However, unless
`MCUBOOT_SWAP_SAVE_ENCTLV` is enabled, the decryption keys are stored in
plaintext in the scratch area. Therefore, `MCUBOOT_SWAP_SAVE_ENCTLV` must be
enabled if the scratch area does not reside in the internal flash memory of the
MCU, to avoid attacks that could interrupt the upgrade and read the plaintext
decryption keys from external flash memory.

Also when swap is used, the image in the `primary slot` is checked for
presence of the `ENCRYPTED` flag and the key TLV. If those are present the
sectors are re-encrypted when copying from the `primary slot` to
the `secondary slot`.

---
***Note***

*Each encrypted image must have its own key TLV that should be unique*
*and used only for this particular image.*

---

Also when swap method is employed, the sizes of both images are saved to
the status area just before starting the upgrade process, because it
would be very hard to determine this information when an interruption
occurs and the information is spread across multiple areas.

## [Factory-programing requirement](#factory-programing-requirement)

It is important to have updates without any voids in encryption. 
Therefore, from the very beginning, flags and TLV's must be set accordingly.
Perform the first flashing with an image signed by imgtool with encryption settings
intended for DFU.
Append it with the `--clear` flag to keep the image unencrypted and ready for use.

## [Creating your keys with imgtool](#creating-your-keys-with-imgtool)

`imgtool` can generate keys by using `imgtool keygen -k <output.pem> -t <type>`,
 where type can be one of `rsa-2048`, `rsa-3072`, `ecdsa-p256`
or `ed25519`. This will generate a keypair or private key.

To extract the public key in source file form, use
`imgtool getpub -k <input.pem> -e <encoding>`, where `encoding` can be one of
`lang-c` or `lang-rust` (defaults to `lang-c`). To extract a public key in PEM
format, use `imgtool getpub -k <input.pem> -e pem`.

If using AES-KW, follow the steps in the next section to generate the
required keys.

## [Creating your keys with Unix tooling](#creating-your-keys-with-unix-tooling)

* If using RSA-OAEP, generate a keypair following steps similar to those
  described in [signed_images](signed_images.md) to create RSA keys.
* If using ECIES-P256, generate a keypair following steps similar to those
  described in [signed_images](signed_images.md) to create ECDSA256 keys.
* If using ECIES-X25519, generate a private key passing the option `-t x25519`
  to `imgtool keygen` command. To generate public key PEM file the following
  command can be used: `openssl pkey -in <generated-private-key.pem> -pubout`
* If using AES-KW (`newt` only), the `kek` can be generated with a
  command like (change count to 32 for a 256 bit key)
  `dd if=/dev/urandom bs=1 count=16 | base64 > my_kek.b64`

## [XIP Encryption (Hardware-Accelerated)](#xip-encryption-hardware-accelerated)

### Overview

Standard `MCUboot` encryption decrypts images during the swap process so that
the `primary slot` always contains plaintext. This works well for internal flash
but prevents execute-in-place (XIP) from external memory, because decrypted
data would be exposed on the external bus.

XIP encryption keeps images encrypted in **all** slots. A hardware crypto engine
(e.g., Infineon SMIF with on-the-fly AES decryption) provides transparent
decryption during code execution. The CPU fetches ciphertext from flash and the
hardware returns plaintext -- no software decryption step is needed at runtime.

### Security properties

The XIP encryption mode has fundamentally different security properties from
the standard swap-with-software-decryption flow. These differences are
important for any security analysis of a system that uses this mode:

* **Software never encrypts or decrypts the image.** All encryption is
  performed off-device at build time by `imgtool` / `edgeprotecttools`. At
  runtime, decryption is performed transparently by the hardware crypto
  engine on each flash fetch. The bootloader handles only ciphertext: it
  copies ciphertext between slots during swap, validates ciphertext, and
  hands the AES key/IV to the hardware. There is no software code path that
  produces plaintext of the image.

* **Signatures cover the encrypted image (ciphertext).** The SHA-256 hash and
  ECDSA signature in the image TLVs are computed over the exact bytes that
  reside in flash (header + ciphertext + protected TLVs). This is the
  opposite of standard MCUboot encryption, where the signature is over the
  plaintext (because at runtime in the standard flow, the plaintext is what
  the device actually executes). Signing the ciphertext lets the bootloader
  fully verify the image without ever performing software decryption, which
  is the whole point of XIP encryption.

* **A signature is mandatory.** Encrypted XIP images must be signed, and the
  bootloader rejects unsigned encrypted images. Without signature
  verification an attacker can mount a trivial XOR attack against AES-CTR
  ciphertext: flipping bits in the ciphertext flips the corresponding bits
  in the plaintext that the CPU will execute. Well-known regions such as the
  vector table could be manipulated to redirect execution to attacker-chosen
  code. The mandatory signature, computed over the ciphertext, closes this
  attack vector before the AES key is ever loaded into the hardware.

* **Every image must use a unique encryption key and IV.** This is a hard
  requirement of AES-CTR, not just a recommendation. Reusing a (key, nonce)
  pair across two different plaintexts allows an attacker who recovers (or
  can guess) plaintext from one image to recover the keystream and thereby
  decrypt the other. The extended ECIES TLV used for XIP carries a 32-byte
  per-image HKDF salt that diversifies both the AES key and the IV for every
  build, so every signed image — including different versions of the same
  application — derives a unique (key, IV) pair from the same long-term
  ECIES keypair. Tooling and provisioning flows MUST preserve this property:
  never reuse the HKDF salt across images, and never reuse a (key, IV)
  derived for one image to encrypt a different image.

* **No authenticated encryption.** It is common in modern security guidance
  to mandate authenticated encryption (AEAD) such as AES-GCM. For XIP from
  external flash this is generally unworkable: the hardware crypto engine
  performs on-the-fly decryption at the granularity of a CPU bus fetch and
  has no opportunity to verify a per-block authentication tag, and the
  particular cipher mode is usually fixed by the SoC. Authentication of the
  image is instead provided once, up front, by the mandatory signature over
  the ciphertext. Because there is no per-fetch integrity check at runtime,
  **key hygiene and counter (IV) hygiene are critical** — see the previous
  bullet.

### Configuration

Enable XIP encryption with the following defines:

```c
#define MCUBOOT_ENC_IMAGES_XIP
#define MCUBOOT_IMAGE_ACCESS_HOOKS
```

Do **not** define `MCUBOOT_ENC_IMAGES`. The standard software encryption path
is not used; the platform library handles image validation and key extraction
independently through the hook interface.

For Zephyr builds, enable `CONFIG_BOOT_ENCRYPT_IMAGE_XIP` in Kconfig. This
option automatically selects `BOOT_IMAGE_ACCESS_HOOKS` and is mutually
exclusive with `BOOT_ENCRYPT_IMAGE` (standard software encryption).

### How it works

1. **Build-time encryption** -- Images are encrypted by `imgtool` (or
   `edgeprotecttools`) using AES-CTR before signing. The AES-CTR counter
   follows the edgeprotecttools format:

   ```
   AES-CTR block input = counter_LE32 || nonce[0:12]
   ```

   where `counter_LE32` is the absolute byte address (`base_address + offset`)
   encoded as a little-endian 32-bit integer, and `nonce[0:12]` is the first
   12 bytes of the HKDF-derived `xip_iv`. The counter is not slot-dependent;
   both the primary and secondary slots store identical ciphertext.

2. **Ciphertext signing** -- The SHA-256 hash and ECDSA signature are computed
   over the encrypted image (header + ciphertext + protected TLVs). The
   bootloader never performs software decryption during validation. Validating
   over ciphertext means the signature covers the exact bytes that reside in
   flash.

3. **Mandatory signature** -- Encrypted XIP images must be signed. The
   bootloader rejects unsigned encrypted images to prevent XOR attacks: an
   attacker who knows plaintext at a given offset could XOR it against the
   ciphertext to recover the keystream, then forge arbitrary content at that
   offset. A mandatory signature closes this attack.

4. **Identical ciphertext in both slots** -- Because encryption is tied to
   flash-area-relative offsets (not slot-specific addresses), both the primary
   and secondary slots store the same ciphertext. There is no slot-specific
   re-encryption.

5. **Swap copies raw bytes** -- During an upgrade, swap moves encrypted blocks
   between slots without any encryption or decryption. The data is byte-for-byte
   identical regardless of which slot it resides in.

6. **Post-boot hardware setup** -- After `boot_go()` returns, the application
   startup code configures hardware crypto regions for each image. The AES key
   and IV are available in the `boot_rsp` structure:

   ```c
   struct boot_rsp rsp;
   /* ... boot_go(&rsp) ... */

   /* rsp.xip_key[4] -- 16-byte AES-128 key */
   /* rsp.xip_iv[4]  -- 16-byte IV/nonce     */
   ```

7. **Bootloader validation flow** -- The `boot_image_check_hook()` validates
   encrypted XIP images without software decryption:
   1. Compute SHA-256 over raw flash (header + ciphertext + protected TLVs)
   2. Verify hash against `IMAGE_TLV_SHA256`
   3. Verify mandatory ECDSA-P256 signature against `IMAGE_TLV_ECDSA_SIG`
   4. ECIES-P256 key unwrap from `IMAGE_TLV_ENC_EC256` to extract AES key/IV

   The extracted keys are held in RAM only for the duration of boot and are
   cleared before jumping to the application.

8. **imgtool image creation** -- The `imgtool sign --encrypt-xip` command
   creates XIP images by encrypting the payload **before** computing the
   hash and signature:
   1. Encrypt image body with AES-CTR (edgeprotecttools format)
   2. Append protected TLVs (not encrypted)
   3. Compute SHA-256 over header + ciphertext + protected TLVs
   4. Sign the hash with ECDSA-P256 (same as for non-encrypted images)
   5. Add unprotected TLVs: SHA-256, ECDSA signature, key hash, ECIES envelope

   A signing key (`--key`) is mandatory when using `--encrypt-xip`.

   Example command:
   ```
   imgtool sign \
       --key root-ec-p256.pem \
       --encrypt-xip enc-ec256-pub.pem \
       --xip-base-address 0x60010000 \
       --version 1.0.0 --header-size 0x400 --slot-size 0x100000 \
       app.bin app_signed_enc.bin
   ```

   The `--xip-base-address` argument is mandatory and specifies the absolute
   flash address of the image start (slot base). This matches the
   edgeprotecttools convention where `initial_counter = base_address +
   header_size`.

   A standalone verification script is provided at `scripts/verify_xip_image.py`:
   ```
   python verify_xip_image.py image.bin --sign-key root-ec-p256.pem --enc-key enc-ec256-priv.pem
   ```

### Extended ECIES TLV format

XIP images use an extended ECIES TLV (177 bytes) that adds a 32-byte HKDF
salt field beyond the standard ECIES-P256 layout:

```
[ ephemeral_pubkey (65 bytes) | hmac_tag (32 bytes) | enc_key (16 bytes) |
  hkdf_salt (32 bytes) | reserved/padding (32 bytes) ]
```

The `hkdf_salt` is a per-image random value used as an HKDF diversifier.
When the image base address is non-zero, the last 4 bytes of the salt are
replaced with the base address (little-endian), binding key derivation to
the target slot address. This ensures unique key/IV derivation per
(image, address) pair.

The final 32-byte field is reserved padding for future use and must be zero.

---
***Note***

*Every image must use a unique encryption key. AES-CTR with the same key and*
*overlapping counter values is catastrophically insecure: it exposes the*
*plaintext XOR relationship of two messages. The per-image random HKDF salt*
*in the extended ECIES TLV ensures unique key/IV derivation for every image.*

---

### Platform requirements

A platform using XIP encryption must provide the following hook functions:

* **`boot_image_check_hook(state, img_index, slot)`** -- Performs complete image
  validation including hash and signature verification over the encrypted image
  and ECIES TLV processing. Returns `FIH_SUCCESS` if the image is valid,
  `FIH_FAILURE` otherwise. This replaces the standard `MCUboot` validation path.
  The `state` pointer provides access to boot loader context (slot flash areas,
  secondary offset for swap-offset mode, max image size); it may be `NULL` when
  called outside the normal boot flow (e.g. serial recovery).

  ```c
  fih_ret boot_image_check_hook(struct boot_loader_state *state,
                                int img_index, int slot);
  ```

* **`boot_xip_populate_rsp(img_index, rsp)`** -- Called after successful
  validation to copy the extracted AES key and IV into the `boot_rsp`
  structure. The application uses these values to configure hardware crypto
  regions.

  ```c
  void boot_xip_populate_rsp(int img_index, struct boot_rsp *rsp);
  ```

* **Hardware crypto setup** -- After `boot_go()` returns, the application
  must configure the hardware crypto engine (e.g., SMIF crypto regions) using
  the key material from `boot_rsp` before jumping to the application image.

### Multi-key model

When multiple images are used (`MCUBOOT_IMAGE_NUMBER > 1`), each image carries
its own independent AES key and IV in its ECIES TLV. The hardware crypto engine
must support multiple decryption regions -- one per image -- to allow
simultaneous XIP execution from different flash areas.

The `boot_rsp` structure carries key material for the current image. The
platform's startup code is responsible for collecting keys from all images and
configuring a separate crypto region for each one.

### Security considerations

* **No plaintext keys in flash** -- AES keys exist only in RAM during the boot
  process and in hardware crypto registers after configuration.
* **Key zeroization** -- `MCUboot` clears the `boot_status` structure
  (which may hold plaintext key material) before jumping to the application.
  Platforms should zeroize any additional key buffers after hardware crypto
  setup is complete.
* **ECIES-P256 wrapping** -- The per-image AES key is protected using
  ECIES-P256 in the image TLV, requiring the device's private key for
  extraction.
* **Key uniqueness** -- The 32-byte HKDF salt in the extended TLV ensures each
  image derives a unique key and IV. Reusing the same key with overlapping AES-CTR
  counters would be catastrophic.
* **Mandatory signature** -- The bootloader rejects unsigned encrypted XIP images
  to prevent XOR attacks on known-plaintext ciphertext regions.
* **FIH hardening** -- The `boot_image_check_hook()` return path uses
  `MCUboot`'s fault injection hardening (FIH) to protect against glitch
  attacks on the validation result.
* **12-byte nonce** -- Only the first 12 bytes of the HKDF-derived `xip_iv`
  are used as the AES-CTR nonce. The last 4 bytes of the 16-byte IV are
  zeroed (counter portion). The hardware computes the counter from the
  absolute flash address being fetched.
