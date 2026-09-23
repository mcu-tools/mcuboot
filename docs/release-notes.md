# MCUboot release notes

- Table of Contents
{:toc}

## Version 2.5.0

This release adds support for Mbed TLS 4.x and TF-PSA-Crypto,
experimental logical sectors for the swap modes, a raw binary
transport and an inactivity timeout for serial recovery, and
multiple verification keys in Zephyr builds. Zephyr users of USB DFU,
CDC ACM serial recovery or RAM load need to update their
configuration; see the upgrade notes. It also hardens image
validation and the handling of image encryption keys.

### Upgrade notes

- The Mbed TLS submodule has moved from `ext/mbedtls` to
  `ext/mbedtls-3.6.0` (still Mbed TLS 3.6.0). If you build a port
  that uses the in-tree copy, such as the standalone Espressif port,
  run `git submodule update --init --recursive` after updating. The
  old `ext/mbedtls` directory is no longer used and can be removed.
- **Serial recovery:** On Zephyr, CDC ACM serial recovery
  (`CONFIG_BOOT_SERIAL_CDC_ACM`) now uses Zephyr's USB device-next
  stack, and USB is only started when recovery is entered. The
  legacy `CONFIG_USB_DEVICE_*` settings no longer apply. Set the USB
  descriptors with the new `CONFIG_BOOT_SERIAL_CDC_ACM_VID`,
  `CONFIG_BOOT_SERIAL_CDC_ACM_PID`,
  `CONFIG_BOOT_SERIAL_CDC_ACM_MANUFACTURER_STRING` and
  `CONFIG_BOOT_SERIAL_CDC_ACM_PRODUCT_STRING` options. Their defaults
  (product ID 0x0004, product string "CDC ACM serial recovery") differ
  from before, so update any host tooling that matches on them. The
  new stack also makes the bootloader larger; check that it still
  fits its partition.
- **Serial recovery:** On Zephyr, UART serial recovery
  (`CONFIG_BOOT_SERIAL_UART`) and the UART log backend
  (`CONFIG_LOG_BACKEND_UART`) can no longer share a UART, and such a
  build now fails with an error. Point the `zephyr,uart-mcumgr`
  chosen node at a separate UART, or disable the UART log backend.
- **Serial recovery:** On Zephyr, `CONFIG_BOOT_SERIAL_MAX_RECEIVE_SIZE`
  must now be at least `CONFIG_BOOT_MAX_LINE_INPUT_LEN`. Smaller
  values are rejected when the configuration is processed.
- **Zephyr:** This release follows changes made on Zephyr's main
  branch after Zephyr 4.4.0, including the reworked boot banner, the
  removal of `CONFIG_PSA_CRYPTO_CLIENT` and the rename of
  `CONFIG_STM32_MEMMAP`. Use it with a Zephyr version newer than
  4.4.0. See also the pending questions at the end of this section.
- **Zephyr:** USB DFU now uses Zephyr's USB device-next stack and is
  configured the same way as serial recovery. `CONFIG_BOOT_USB_DFU`
  enables USB DFU, and the entrance methods `CONFIG_BOOT_USB_DFU_WAIT`
  and `CONFIG_BOOT_USB_DFU_GPIO` are now independent options that can
  be enabled together. `CONFIG_BOOT_USB_DFU_NO` has been removed. If
  you use USB DFU, set `CONFIG_BOOT_USB_DFU=y` together with at least
  one entrance method. The legacy `CONFIG_USB_DFU_*` and
  `CONFIG_USB_DEVICE_*` settings no longer apply: use
  `CONFIG_BOOT_USB_DFU_PERMANENT_DOWNLOAD` and the new
  `CONFIG_BOOT_USB_DFU_VID`, `CONFIG_BOOT_USB_DFU_PID`,
  `CONFIG_BOOT_USB_DFU_MANUFACTURER_STRING` and
  `CONFIG_BOOT_USB_DFU_PRODUCT_STRING` options, whose defaults match
  the old ones. Downloaded images go to the secondary slot.
- **Zephyr:** RAM load (`CONFIG_BOOT_RAM_LOAD` and
  `CONFIG_SINGLE_APPLICATION_SLOT_RAM_LOAD`) now takes the RAM region
  to load images into from a `mcuboot,image-ram` chosen devicetree
  node. Setting `CONFIG_BOOT_IMAGE_EXECUTABLE_RAM_START` and
  `CONFIG_BOOT_IMAGE_EXECUTABLE_RAM_SIZE` by hand is deprecated and
  only possible when that node is missing. Add the chosen node to
  your board overlay.
- **Zephyr:** `CONFIG_MULTIPLE_EXECUTABLE_RAM_REGIONS_DEFAULT_FILE`
  and the built-in default `boot_get_image_exec_ram_info()` have been
  removed. Builds with `CONFIG_MULTIPLE_EXECUTABLE_RAM_REGIONS` must
  now provide their own `boot_get_image_exec_ram_info()`.
- **Zephyr:** MCUboot no longer replaces Zephyr's boot banner, which
  changed after Zephyr 4.4. The `*** Booting MCUboot ... ***` line is
  now printed after Zephyr's own banner instead of before it, so
  update anything that parses boot output. The line is controlled by
  `CONFIG_MCUBOOT_BOOT_BANNER`, which is enabled by default when
  `CONFIG_BOOT_BANNER` is, but no longer requires it.
- **Zephyr:** The STM32 external flash configurations now use
  `CONFIG_FLASH_STM32_NOR_MEMMAP`, following its rename in Zephyr from
  `CONFIG_STM32_MEMMAP`. Rename the option in your own configuration
  files.
- **Zephyr:** `SOC_FLASH_0_ID` and `SPI_FLASH_0_ID` have been removed
  from the Zephyr port's `sysflash.h`. The flash device of each slot
  is now taken from its devicetree partition. Out-of-tree code that
  used these macros must use the devicetree partition macros instead.
- **Zephyr:** `CONFIG_MCUBOOT_CLEANUP_RAM` now clears the RAM region
  of the `zephyr,sram` chosen devicetree node instead of the region
  set by `CONFIG_SRAM_BASE_ADDRESS` and `CONFIG_SRAM_SIZE`. If your
  board sets those options to a different region, check which RAM is
  cleared before the application starts.
- **Espressif:** The Espressif port now uses the ESP-IDF v6.0.0 HAL.
  Building the standalone port requires ESP-IDF v6.0 sources.
- **Espressif:** The bootloader's RAM layout has changed on all SoCs.
  A new `dram_loader_seg` region holds data used while the
  application is loaded, and the other bootloader regions have moved.
  The application image must not load anything into the bootloader's
  `iram_loader_seg`, `dram_loader_seg` or `dram_seg` regions. Check
  your OS linker script against the updated memory maps in
  `docs/readme-espressif.md`.
- **Espressif:** The default ESP32-C6 `bootloader.conf` no longer
  enables virtual eFuses. Builds that use it now program the real
  eFuses. To keep emulating them in flash, set `CONFIG_EFUSE_VIRTUAL`
  and the related options in your configuration.
- **Espressif:** The default console baud rate on ESP32-C2 is now
  74880.
- **NuttX:** MCUboot no longer initialises the board through
  `boardctl(BOARDIOC_INIT)`. Board initialisation must now be done by
  the NuttX kernel (for example with NuttX's
  `CONFIG_BOARD_LATE_INITIALIZE`).

### New features

- Added experimental support for logical sectors. A non-zero
  `MCUBOOT_LOGICAL_SECTOR_SIZE` makes the swap algorithms work in
  units of that size rather than the device's hardware sectors. The
  size must be a power of two and a multiple of the hardware erase
  size, and each slot must be a multiple of it. With
  `MCUBOOT_VERIFY_LOGICAL_SECTORS`, MCUboot checks the logical layout
  against the flash at boot and, if they do not match, boots the
  primary slot without upgrading. On Zephyr, set
  `CONFIG_MCUBOOT_LOGICAL_SECTOR_SIZE` and
  `CONFIG_MCUBOOT_VERIFY_LOGICAL_SECTORS`.
- Added `MCUBOOT_USE_CUSTOM_CRYPTO`, which lets a port supply its own
  hash, signature and encryption primitives (another library, a
  hardware accelerator or a vendor SDK) without modifying MCUboot.
  See `docs/custom_crypto.md`.
- Added `MCUBOOT_BOOT_TMPBUF_SZ`, which lets a port set the size of
  the buffer used to read an image while hashing it (default 256
  bytes). A larger buffer means fewer flash reads, which helps on
  flash that is not memory mapped. A smaller one saves RAM.
- The Mbed TLS crypto backend (`MCUBOOT_USE_MBED_TLS`) and PSA image
  encryption now also build against Mbed TLS 4.x and TF-PSA-Crypto
  1.x. The Mbed TLS header layout is detected automatically.
  `MCUBOOT_MBEDTLS_CRYPTO_IN_PRIVATE_SUBDIR` can override the
  detection. With PSA crypto and `MCUBOOT_FIH_PROFILE_HIGH`, the
  fault injection random delay now uses `psa_generate_random()`.
- AES key wrap image encryption (`MCUBOOT_ENCRYPT_KW`) can now use the
  PSA Crypto backend (`MCUBOOT_USE_PSA_CRYPTO`). It needs a PSA
  implementation that provides `psa_unwrap_key()`, and has been
  tested with TF-M.
- **Serial recovery:** Added a raw binary SMP transport,
  `CONFIG_BOOT_SERIAL_RAW_PROTOCOL`, which drops the base64, length,
  CRC and console framing of SMP over console. It uses less flash and
  RAM and transfers faster, but needs a binary-capable serial link.
  It matches Zephyr's `CONFIG_UART_MCUMGR_RAW_PROTOCOL`. By default,
  a partly received packet is discarded after
  `CONFIG_BOOT_SERIAL_RAW_PROTOCOL_INPUT_TIMEOUT_MS` without new data
  (`CONFIG_BOOT_SERIAL_RAW_PROTOCOL_INPUT_TIMEOUT`).
- **Serial recovery:** Added the optional MCUmgr parameters command
  (`CONFIG_BOOT_MGMT_MCUMGR_PARAMS`), which reports the SMP buffer
  size and count so that clients can choose the best fragment size.
  It requires `CONFIG_BOOT_MAX_LINE_INPUT_LEN` to stay at its default
  of 128.
- **Serial recovery:** Added `CONFIG_BOOT_SERIAL_INACTIVITY_TIMEOUT`.
  When serial recovery was entered through
  `CONFIG_BOOT_SERIAL_BOOT_MODE` or `CONFIG_BOOT_SERIAL_WAIT_FOR_DFU`,
  it resets the device once no MCUmgr command has arrived for the set
  time. An abandoned upload then no longer leaves the device in
  recovery. The default of 0 keeps the previous behaviour.
- **Serial recovery:** On Zephyr, the UART receive interrupt now
  reads data in batches rather than one byte at a time, cutting
  per-byte overhead during uploads. The batch size is set with
  `CONFIG_BOOT_SERIAL_UART_RX_BATCH_SIZE` (default 512).
- **Zephyr:** `CONFIG_BOOT_SIGNATURE_KEY_FILE` now accepts a
  comma-separated list of PEM files, each embedded as a verification
  key. A development bootloader can then accept both production- and
  development-signed images, for example. Every key after the first
  must be public-key only, and all keys must be of the same type.
- **Zephyr:** Added `CONFIG_BOOT_IMAGE_JUMP_HOOKS`, which calls a
  user-supplied `boot_image_jump_hook()` after an image has been
  selected and just before jumping to it. The hook can inspect or
  change the boot response, for example to set flash access rights
  or remap memory.

### Bug fixes

- Hardened image validation against malformed image headers and
  TLVs. TLV lengths are now checked against the bounds of the TLV
  area. An image whose header describes more data than its slot
  holds is rejected before it is hashed.
- Hardened the handling of image encryption keys. With serial
  recovery, the AES key and any decrypted image data are now wiped
  from memory after an encrypted upload is validated or decrypted in
  place. The TinyCrypt backend now wipes the AES key schedule when a
  decryption context is released, as the other backends already
  did.
- Fixed a hang at boot with `MCUBOOT_DATA_SHARING_BOOTINFO` and
  hardware rollback protection when the security counter of an image
  could not be read. The shared boot information now records the
  security counter of every image, not only the first one found.
- Fixed the secondary slot address check done by
  `MCUBOOT_VERIFY_IMG_ADDRESS` (without
  `MCUBOOT_CHECK_HEADER_LOAD_ADDRESS`) on Cortex-M. It read the wrong
  vector table entry, so a valid update could be rejected and erased,
  or an image linked for the wrong address accepted.
- Fixed bootstrap (`MCUBOOT_BOOTSTRAP`) in swap-move and swap-offset
  modes. It could read beyond the flash area and erase the wrong
  sectors, and it copied the whole slot instead of only the image.
- The watchdog is now fed while an image is hashed. Before, a large
  image on slow or external flash could trigger a watchdog reset
  during validation.
- Fixed the build with `MCUBOOT_HW_ROLLBACK_PROT_LOCK`.
- Fixed a stack overflow when checking split images (`split_go()`)
  in builds with `MCUBOOT_SIGN_EC384` or `MCUBOOT_SHA512`.
- Fixed decryption of RSA-OAEP encrypted images with the Mbed TLS
  backend built against TF-PSA-Crypto (Mbed TLS 4.x). The private key
  was misparsed, so the image could not be decrypted.
- Fixed image encryption with the PSA Crypto backend in builds that
  use MCUboot's generic CMake file, such as TF-M. The PSA encryption
  code was not built, and defining both `MCUBOOT_USE_PSA_CRYPTO` and
  `MCUBOOT_USE_MBED_TLS` caused a compile error. Encryption schemes
  other than EC256 and X25519 no longer need Mbed TLS headers.
- `docs/PORTING.md` now lists all the flash functions a port must
  provide. `flash_area_get_sector()`, `flash_device_base()` and
  `flash_area_id_from_image_slot()` were required but not documented.
- **Serial recovery:** The slot info command reported wrong image
  IDs for upload when direct image upload was disabled.
- **Serial recovery:** An upload with an image number beyond the
  number of images is now rejected with an error instead of writing
  to another flash area. After a failed request, an encrypted image
  is no longer decrypted in place.
- **Serial recovery:** Fixed a possible division by zero during
  in-place decryption of an uploaded encrypted image when the flash
  driver fails to report the sector layout.
- **Serial recovery:** On Zephyr, direct image upload
  (`CONFIG_MCUBOOT_SERIAL_DIRECT_IMAGE_UPLOAD`) now works for image
  IDs 7 to 16. Before, uploads to the slots of images 4 to 8 failed.
- **Serial recovery:** On Zephyr, the wait for a DFU command
  (`CONFIG_BOOT_SERIAL_WAIT_FOR_DFU`) now ends after
  `CONFIG_BOOT_SERIAL_WAIT_FOR_DFU_TIMEOUT`. Before, it could last
  several times longer.
- **Serial recovery:** On Zephyr with
  `CONFIG_BOOT_SERIAL_WAIT_FOR_DFU`, the console was initialised a
  second time when recovery was also entered through boot mode or
  because there was no application. With CDC ACM this made the
  bootloader panic, and on a UART it lost received bytes.
- **Serial recovery:** Fixed a link error on Zephyr with image
  encryption and deferred logging.
- **Serial recovery:** On ports other than Zephyr and Espressif, such
  as Mynewt, a base64 decoding error in a received frame is now
  detected. Before, the error went unnoticed.
- **Zephyr:** With `CONFIG_BOOT_MAX_IMG_SECTORS_AUTO`, the maximum
  sector count now takes the slots of every image into account, not
  only image 0. The erase and write sizes of the secondary slot are
  now read from the flash it is actually on.
- **Zephyr:** Fixed the trailer size that sysbuild reserves at the
  end of the application for swap-move and swap-offset. Swap-offset
  could build an application too large to swap, and swap-move
  reserved a sector more than needed.
- **Espressif:** Fixed the build of the standalone port with serial
  recovery (`CONFIG_ESP_MCUBOOT_SERIAL`), which lacked an Mbed TLS
  source file.
- **Mynewt:** Fixed the build of serial recovery without image
  encryption.

### Board and SoC support

- **Zephyr:** Added configurations for the nRF93M1DK, the MR-NAVQ95B
  (Cortex-M7 core, with MCUboot placed in the boot container by
  default), the FRDM-MCXL255 and the KIT_PSE84_EVAL, and SoC
  configurations for the ESP32-C5, ESP32-C61, ESP32-P4 and ESP32-S31.
- **Zephyr:** On the nRF54H20 application core, multithreading is
  now enabled so that MRAM power-down can be held off while MCUboot
  writes to MRAM, which avoids stalled writes.
- **Espressif:** Added initial support for the ESP32-C5, ESP32-C61
  and ESP32-P4. The ESP32-C61 uses ECDSA image signing.

### imgtool

- imgtool now requires Python 3.8 or later. This also fixes key
  export commands, which failed on Python 3.8 and 3.9.
- Added `imgtool keyinfo`, which reports whether a PEM file holds a
  key pair or only a public key. With `--require`, it fails if the
  kind does not match, which build systems can use as a check.
- Added `--name-suffix` to `imgtool getpub` and `imgtool getpubhash`,
  which appends a suffix to the generated C or Rust symbol names so
  that several keys can be embedded in one image.
- Public-key-only PEM files are now supported by `imgtool getpub`,
  `imgtool getpubhash` and `imgtool verify`. `imgtool sign` given a
  public-only PEM now fails with a clear error.

### Pending questions

These questions are open for this release candidate and will be
settled before the final 2.5.0 release. If your testing answers one
of them, please report it on the MCUboot GitHub issue tracker.

- **Zephyr:** Does this release still build and work with Zephyr
  4.4.x, or only with Zephyr newer than 4.4.0? The in-tree STM32
  external flash configurations already need the newer Zephyr.
- **Espressif:** The standalone port's `flash` build target now calls
  esptool with its hyphenated option names (`write-flash`,
  `--before default-reset`, `--after no-reset`). Which esptool
  versions accept them? If flashing fails, please report your esptool
  version.
- **NuttX, Mbed:** Extra `flash_area_close()` calls were removed from
  the swap code that resumes an interrupted swap and that reads an
  image's size. On NuttX and Mbed, closing a flash area releases the
  device, so these calls may have caused swap upgrade failures with
  2.4.0. If you saw such failures, does this release candidate fix
  them?
- **Mynewt:** The Mynewt port is built in CI, but the core
  maintainers no longer test it on hardware. Mynewt users, please
  test this release candidate, especially serial recovery.

## Version 2.4.0

- Added support for using an inbuilt (compiled-in) key in Zephyr
  builds.
- Added support for using an external PSA crypto library backend
  (a non-mbedTLS PSA backend) in Zephyr builds.
- Added a Kconfig choice in Zephyr builds to select between Mbed
  TLS legacy crypto and the PSA API for RSA operations. Legacy
  crypto remains the default since the PSA API increases the
  flash footprint and is not acceptable for all targets.
- Added ECDSA support to the Zephyr port using mbedTLS.
- ``BOOT_SIGNATURE_TYPE_RSA`` no longer selects RSA key exchange
  support, since MCUboot only requires RSA for signature
  verification.
- Use the new ``MBEDTLS_VERSION_4_x`` Kconfig boolean in Zephyr
  builds to select between Mbed TLS 3.x legacy crypto and the
  TF-PSA-Crypto 1.x backend.
- Automatically enable ``TEST_RANDOM_GENERATOR`` in Zephyr builds
  when PSA crypto is enabled and no entropy driver is available,
  since ``MBEDTLS_PSA_CRYPTO_LEGACY_RNG`` no longer selects it
  implicitly.
- Renamed ``CONFIG_MBEDTLS_CFG_FILE`` usage to follow the rename
  in Zephyr.
- Renamed nRF54H Kconfig symbol usage to follow the rename in
  Zephyr.
- imgtool: added a new ``--custom-tlv-file`` option that works
  like ``--custom-tlv`` but reads the TLV value from a binary
  file instead of taking it on the command line.
- imgtool: ``dumpinfo`` now supports a ``-f``/``--format`` option
  to select between human, yaml and json output. The defaults
  remain backwards compatible (human for stdout, yaml when
  writing to a file).
- imgtool: ``dumpinfo`` can now read Intel hex (``.hex``) files
  in addition to binary files.
- Zephyr's sysbuild hooks have been reworked to support
  arbitrary-named MCUboot images, also allowing for multiple
  MCUboot builds in a single sysbuild project to update
  different images with estimated image overhead sizes.
- Zephyr builds now use partition macros without the ``FIXED_``
  prefix, allowing MCUboot to be used on devices that use
  ``fixed-partitions`` and ``zephyr,memory-mapped`` compatibles.
- Use the ``DT_REG_ADDR()`` and ``DT_REG_SIZE()`` devicetree
  macros to obtain the target load area address range, allowing
  nodes that rely on a devicetree ``ranges`` property to be
  used.
- Removed the forced ``CONFIG_BOOT_MAX_IMG_SECTORS`` for
  Espressif targets so that auto detection can take place.
- Added support for placing image slots in sub-partition
  devicetree nodes when computing MCUboot image overhead.
- Improved the Zephyr CMake support for finding NVM devices,
  including reading the write and erase block sizes from the
  device.
- Added support for an ``ext_flash_app`` variant on the
  ``stm32h7s3xx``, allowing chainloading applications from
  external flash while MCUboot runs from internal flash.
- Updated the nrf52840 board overlay bindings to use the new
  ``zephyr,memory-mapped`` binding, and added missing ``ranges``
  properties on a few board overlays.
- Espressif: separated the ``do_boot`` path so that RISC-V based
  Espressif SoCs no longer fall through to the wrong ``do_boot``
  implementation.
- Espressif: updated the default ``bootloader.conf`` files to
  reflect the default flash layout configuration for most
  Espressif boards on Zephyr.
- Espressif: added a default SoC configuration for ESP32-H2 so
  that DRAM usage does not overflow.
- Mbed: added ``flash_area_get_sector`` to fix an undefined
  reference for Mbed CE.
- Mbed: fixed the ``MCUBOOT_SWAP_SAVE_ENCTLV`` configuration
  option by switching to the canonical name and correcting the
  macro name.
- Mynewt: improved the BOOTUTIL configuration so that only
  ``bootutil_public.c`` is built for non-bootloader builds,
  allowing applications to skip bootloader-only syscfgs.
- Fixed image size validation to include the
  ``ih_protect_tlv_size`` field.
- Fix: Corrected the copy size calculation when bootstrapping
  and swapping using ``MCUBOOT_SWAP_USING_MOVE``. Previously,
  the primary region size was used, which could be larger than
  the secondary region, when using the optimal region sizes.
  Now, the size of the secondary region (excluding the swap
  sector and sectors needed for swapping) is used, ensuring
  only the valid image area is copied. This prevents potential
  over-copying and related issues during image upgrade or
  bootstrap operations.
- Fixed ``image_validate`` so that the offset of the
  swap-using-move sector is included when pure mode is used in
  swap-offset.
- Fixed ``image_ed25519`` to no longer call mbedTLS public key
  functions when ``MCUBOOT_BUILTIN_KEY`` is enabled.
- Fixed the definition of ``bootutil_find_key`` when
  ``MCUBOOT_BYPASS_KEY_MATCH`` is set and ``MCUBOOT_HW_KEY`` is
  not.
- Fixed typos and incorrect types/pointer indirection in
  ``boot_serial_encryption``.
- Added the missing swap-offset source file to the bootutil
  CMake list, and fixed the RAM load source file which was
  using Zephyr-specific Kconfigs to decide whether it should be
  included.
- RISCV targets in swap mode will no longer erroneously attempt
  to load the image to RAM and will boot the image directly, as
  this is fully supported by RISCV and looks to have been an
  error in a previous code submission.
- Fixed devicetree ``compatible`` property handling in CMake so
  that matching ``soc-nv-flash`` works for nodes whose
  ``compatible`` property contains multiple strings.
- Fixed the regression where the mbedTLS include path was not
  added to the MCUboot build, breaking RSA support with
  encryption.
- Fixed an extra ``.`` in a log message.
- Call ``LOG_PANIC()`` before jumping to the application so log
  backends have an opportunity to flush in-flight messages
  before the jump.
- Capture log events that were previously lost very early or
  very late in the boot process: the deferred logging thread
  now starts with ``K_NO_WAIT`` and is woken in
  ``zephyr_boot_log_stop()`` so it drains pending messages
  before MCUboot jumps to the application.
- Reworked the Zephyr CMake support to fix many issues,
  including a missing project name, casing fixes, deduplicated
  statements, and stopped abusing ``zephyr_library_*``
  functions where MCUboot is not actually a library.
- Added error codes to several bootutil loader log messages and
  reformatted others to fit on fewer lines for easier
  readability and grepping.
- Removed the outdated ``hello-world`` Zephyr sample, since
  Zephyr's tree contains a sysbuild MCUboot sample that should
  be used instead.
- Fixed the ``ext/nrf/cc310_glue`` include path to drop the
  deprecated non-``zephyr/`` prefix.
- Fixed Kconfig options that were leaking outside of the
  MCUboot menu.
- Fixed a missing ``tsa-crypto`` dependency twister error.
- Updated the design documentation to correct an outdated
  comment that suggested the TLV type field is 8-bit when it is
  actually 16-bit.

## Version 2.3.0

- Added support for booting Cortex-R5 images
- Add support for cleaning up the Cortex-R core before final jumping
- Aligned the project security policy with the [TrustedFirmware.org security
  policy](https://www.trustedfirmware.org/.well-known/security.txt).
- Fixed imgtool dependency on click package version.
- Enabled support for ram-load revert mode, which functions using the same
  logic as direct-xip revert mode but loads the executable image to ram.
- Add cache flush after write/erase operations to avoid getting invalid
  data when these are followed by read operation.
- Fix image wrong state after swap-scratch when hardware flash encryption
  is enabled. When hardware flash encryption is enabled, force expected
  erased value (0xFF) into flash when erasing a region, and also always
  do a real erase before writing data into flash.
- Move the Virtual eFuse offset in flash configuration from hardcoded value to .conf file.
- Fixed issue in boot_scramble_regions, where incorrect boundary
  check would cause function to attempt to write pass a designated
  flash area.
- Fixed issue in image_validate when `MCUBOOT_HASH_STORAGE_DIRECTLY` is enabled
  for platforms with NVM memory that does not start at 0x00.
- Fixed issue in image_validate when `BOOT_SIGNATURE_TYPE_PURE` is enabled
  for platforms with NVM memory that does not start at 0x00.
- Fixed serial recovery with progressive erase for MCUboot modes of single
  updatable slot (`MCUBOOT_SINGLE_APPLICATION_SLOT`, `MCUBOOT_FIRMWARE_LOADER`,
  `MCUBOOT_SINGLE_APPLICATION_SLOT_RAM_LOAD`) which was previously failing due
  to attempting to access non-existent image status fields.
- Fixed issue with imgtool when trying to compress images with
  no header padding requested.
- Fixed issue with swap using offset when mininmal erase was
  enabled that did not offset the erase to the second sector and
  wrongly used the (empty) first sector of the secondary slot.
- Switched to picolibc as the default C library in Zephyr.
- Fixed wrong define specifying 2 slots in single loader mode
  instead of just 1
- Fixed wrong slot ID in hook calls from serial recovery.
- Fixed issues with serial recovery not building/not
  working/faulting.
- Swap using offset now includes the size of the unprotected TLV
  area which was wrongly missing before, this requires extra space
  in the swap status as the data is not part of the image header
- Control over compilation of unprotected TLV allow list has been exposed
  using MCUBOOT_USE_TLV_ALLOW_LIST mcuboot configuration identifier.
- Fixed issue with platforms that have
  MCUBOOT_SUPPORT_DEV_WITHOUT_ERASE set that did not scramble
  (delete) data sections from the trailer that should have been
  deleted.
- Fixed issue with boot_scramble_region escaping flash area due
  to error in the range check.
- A few changes to make vscode nicer, including a default package to build at
  the top level, and ignoring some of the cache files from vscode.
- Zephyr builds are now using Kconfig CONFIG_MCUBOOT_BOOT_MAX_ALIGN
  to set the MCUBOOT_BOOT_MAX_ALIGN.
- Fixed issue with checking pin reset not checking for single
  flag in Zephyr.
- imgtool verify when using a public ed25519 key has been fixed
  to work rather than show an invalid key type not matching the
  TLV record error.
- Zephyr signature and encryption key file path handling has now
  been aligned with Zephyr, this means values can be specified in
  multiple .conf file and the one that last set it will be the set
  value. This also means that key files will no longer be found
  relative to the .conf file and will instead be found relative
  to the build system ``APPLICATION_CONFIG_DIR`` variable, though
  the key file strings are now configured which allows for using
  escaped CMake variables to locate the files, for example with
  ``\${CMAKE_CURRENT_LIST_DIR}`` to specify a file relative to
  the folder that the file is in.
- Watchdog support in Zephyr has been reworked and fixed to allow
  installing a timeout (with a configurable value) before starting
  it. The default timeout is set to 1 minute and this feature has
  been enabled by default. 3 Kconfig options have been added which
  control how the watchdog is used in MCUboot:

    * ``CONFIG_BOOT_WATCHDOG_SETUP_AT_BOOT`` controls setting up
      the watchdog in MCUboot (if not set up, it can still be set,
      if the driver supports this non-compliant behaviour).
    * ``CONFIG_BOOT_WATCHDOG_INSTALL_TIMEOUT_AT_BOOT`` controls if
      a timeout is installed at bootup or not.
    * ``CONFIG_BOOT_WATCHDOG_TIMEOUT_MS`` sets the value of the
      timeout in ms.

- In addition, Zephyr modules can now over-ride the default
  watchdog functionality by replacing the weakly defined functions
  ``mcuboot_watchdog_setup`` and/or ``mcuboot_watchdog_feed``,
  these functions take no arguments.
- correct esp32c6 overlay

## Version 2.2.0

- Added support for retrieving HW embedded private keys for image encryption
  (The private key can be retrieved from trusted sources like OTP, TPM.).
- Changed bootutil's order of events to verify the image header
  before checking the image.
- Added the bootloader state object to the bootutil
  boot_is_header_valid() function
- Added optional write block size checking to ensure expected
  sizes match what is available on the hardware.
- Added optional erase size checking to ensure expected sizes
  match what is available on the hardware.
- Added debug logs for zephyr to output discrepencies in erase
  and write block sizes in dts vs actual hardware configuration
  at run-time.
- When using swap with scratch, the image is now decrypted when copying from
  the scratch partition to the primary slot. Therefore, the scratch partition
  doesn't contain plaintext firmware data anymore.
- Added verification for supported IDF-based HAL version.
- Fixed missing macro for XMC flash devices on ESP32-S3
- Extended image loader header to include RTC/LP RAM, DROM and IROM segments.
- Fixed errors when building for `thingy52`, `thingy53` and
  `nrf9160dk` boards.
- Fixed chain load address output log message for RAM load
  mode in Zephyr
- Fixed issue for swap using move whereby a device could get
  stuck in a revert loop despite there being no image in the
  secondary slot
- Fixed clash when using sysbuild with other
  applications (i.e. tests) using the name mcuboot
- imgtool: added initial sanity tests for imgtool commands,
- imgtool: added and enabled unittests in GitGub workflow,
- Fixed wrong maximum application size calculation when
  operating in swap using move mode
- Added additional images max size support to shared data
  function
- Fixed issue with serial recovery image list wrongly using
  number of images as the number of slots and not returning
  complete information for 1 updateable image
- Added slot info command support to serial recovery mode
- Fixed issue with serial recovery variables not being
  correctly initialised to default values which could cause
  some commands to do unexpected operations
- Added a new swap using offset algorithm which is set with
  `MCUBOOT_SWAP_USING_OFFSET`. This algorithm is similar to swap
  using move but avoids moving the sectors in the primary slot
  up by having the update image written in the second sector in
  the update slot, which offers a faster update process and
  requires a smaller swap status area
- Added support for automatically calculating the maximum number
  of sectors that are needed for a build by checking the erase
  sizes of the partitions using CMake for Zephyr. This behaviour
  can be reverted to the old manual behaviour by disabling
  `CONFIG_BOOT_MAX_IMG_SECTORS_AUTO`
- Added protected TLV size to image size check in bootutil
- Added Kconfig for decompression support in Zephyr
- Added compressed image flags and TLV to bootutil
- Added support for removing images with conflicting flags in
  bootutil
- Added support for removing encrypted/compressed images when
  MCUboot is compiled without support for them
- Added support for devices that do not require erase prior to write operation.
- Add corrections to the max app size calculations.
- Fixed issue with swap using scratch mode that would cause the
  primary image to be corrupt and unbootable after an update if the
  device was rebooted whilst the scratch area was being erased.
- Fixed issue with serial recovery if canonical CBOR mode was
  enabled.
- Fixed issue with serial recovery set image state not checking
  primary slot images.
- Fixed issue with watchdog not being fed during flash erase
  operations, which could cause the watchdog to time out on long
  erase operations and prevent firmware updates from being possible.
- Fix issues related to calculating the maximum image size for a given
  configuration.
- Fix an issue with sha hash calculations in a loop.
- Fix an issue with the security counter being updated before an image is
  confirmed.
- Added a contributing guideline.
- Fixed an issue related to referencing the arm-vector table of the
  application, which caused a jump to the incorrect address instead of the
  application reset vector for some Zephyr builds when LTO (link time
  optimization) was enabled.
- Fixed issue with trailer and swap status sizes wrongly being
  included in single slot/firmware loaded modes which wrongly
  reduced the maximum allowable firmware sizes.
- Fixes for Zephyr 4.1 and MPU/SYSMPU renaming
- Fix support for MCX-N9XX with Zephyr.

## Version 2.1.0

- Boot serial: Add response to echo command if support is not
  enabled, previously the command would have been accepted but no
  response indicating that the command is not supported would have
  been sent.
- Added support for using builtin keys for image validation
  (available with the PSA Crypto API based crypto backend for ECDSA signatures).
- Enforce that TLV entries that should be protected are.
  This can be disabled by defining `ALLOW_ROGUE_TLVS`
- bootutil: Fixed issue with comparing sector sizes for
  compatibility, this now also checks against the number of usable
  sectors (which is the slot size minus the swap status and moved
  up by one sector).
- bootutil: Added debug logging to show write location of swap status
  and details on sectors including if slot sizes are not optimal for
  a given board.
- Update ptest to support test selection. Ptest can now be invoked with `list`
  to show the available tests and `run` to run them. The `-t` argument will
  select specific tests to run.
- Allow sim tests to skip slow tests.  By setting `MCUBOOT_SKIP_SLOW_TESTS` in
  the environment, the sim will skip two tests that are very slow.  In one
  instance this reduces the test time from 2 hours to about 5 minutes.  These
  slow tests are useful, in that they test bad powerdown recovery, but are
  inconvenient when testing other areas.
- Zephyr: Fixes support for disabling instruction/data caches prior
  to chain-loading an application, this will be automatically
  enabled if one or both of these caches are present. This feature
  can be disabled by setting `CONFIG_BOOT_DISABLE_CACHES` to `n`.
- Zephyr: Fix issue with single application slot mode, serial
  recovery and encryption whereby an encrypted image is loaded
  and being wrongly treated as encrypted after decryption.
- Zephyr: Add estimated image footer size to cache in sysbuild.
- Added firmware loader configuration type support for Zephyr, this
  allows for a single application slot and firmware loader image in
  the secondary slot which is used to update the primary image
  (loading it in any way it sees fit e.g. via Bluetooth).
- Zephyr: Remove deprecated ZEPHYR_TRY_MASS_ERASE Kconfig option.
- Zephyr: Prevent MBEDTLS Kconfig selection when tinycrypt is used.
- Zephyr: Add USB CDC serial recovery check that now causes a build
  failure if console is enabled and device is the same as the USB
  CDC device.
- Zephyr: Add USB CDC serial recovery check that now causes a build
  failure if the main thread priority is below 0 (cooperative
  thread), this would prevent USB CDC from working as the driver
  would not have been able to fire callbacks.
- Use general flash operations to determine the flash reset vector. This
  improves support a bit for some configurations of external flash.
- fix a memory leak in the HKDF implementation.
- Zephyr: Added a MCUboot banner which displays the version of
  MCUboot being used and the version of zephyr. This can be
  disabled by setting ``CONFIG_MCUBOOT_BOOT_BANNER=n`` which
  will revert back to the default zephyr boot banner.

## Version 2.0.0

Note that this release, 2.0.0 is a new major number, and contains a small API
change in the interface between mcuboot and the platform.  All platforms
contained within the MCUboot tree have been updated, but any external platforms
will have to be adjusted.  The following commit makes the API change, in the
function `boot_save_shared_data`.

    commit 3016d00cd765e7c09a14af55fb4dcad945e4b982
    Author: Jamie McCrae <jamie.mccrae@nordicsemi.no>
    Date:   Tue Mar 14 12:35:51 2023 +0000

        bootutil: Add active slot number and max app size to shared data

### About this release

- Add error when flash device fails to open.
- Panic bootloader when flash device fails to open.
- Fixed issue with serial recovery not showing image details for
  decrypted images.
- Fixes issue with serial recovery in single slot mode wrongly
  iterating over 2 image slots.
- CDDL auto-generated function code has been replaced with zcbor function
  calls, this now allows the parameters to be supplied in any order.
- Added currently running slot ID and maximum application size to
  shared data function definition.
- Make the ECDSA256 TLV curve agnostic and rename it to ECDSA_SIG.
- imgtool: add P384 support along with SHA384.
- espressif: refactor after removing IDF submodule
- espressif: add ESP32-C6, ESP32-C2 and ESP32-H2 new chips support
- espressif: adjustments after IDF v5.1 compatibility, secure boot build and memory map organization
- Serial recovery image state and image set state optional commands added
- imgtool: add 'dumpinfo' command for signed image parsing.
- imgtool: add 'getpubhash' command to dump the sha256 hash of the public key
- imgtool's getpub can print the output to a file
- imgtool can dump the raw versions of the public keys
- Drop ECDSA P224 support
- Fixed an issue with boot_serial repeats not being processed when
  output was sent, this would lead to a divergence of commands
  whereby later commands being sent would have the previous command
  output sent instead.
- Fixed an issue with the boot_serial zcbor setup encoder function
  wrongly including the buffer address in the size which caused
  serial recovery to fail on some platforms.
- zcbor library files have been updated to version 0.7.0
- Reworked boot serial extensions so that they can be used by modules
  or from user repositories by switching to iterable sections.
- Removed Zephyr custom img list boot serial extension support.
- (Zephyr) Adds support for sharing boot information with
  application via retention subsystem
- Zephyr no longer builds in optimize for debug mode, this saves a
  significant amount of flash space.
- Reworked image encryption support for Zephyr, static dummy key files
  are no longer in the code, a pem file must be supplied to extract
  the private and public keys. The Kconfig menu has changed to only
  show a single option for enabling encryption and selecting the key
  file.
- Serial recovery can now read and handle encrypted seondary slot
  partitions.
- Serial recovery with MBEDTLS no longer has undefined operations which
  led to usage faults when the secondary slot image was encrypted.
- espressif: allow the use of a different toolchain for building

## Version 1.10.0

The 1.10.0 release of MCUboot contains...

### About this release

- Various fixes to boot serial.
- Various fixes to the mbed target.
- Various fixes to the Espressif native target.
- Various fixes to the Zephyr target.
- Workflow improvements with Zephyr CI.
- Add multi image support to the espressif esp32 target.
- Improvements and corrections to the simulator.
- Improve imgtool, including adding 3rd party signing support.
- Various fixes to the mynewt target.
- Various fixes to the nuttx target.
- Dates to dependencies for doc generation.
- Add downgrade prevention for modes using swap.
- Various general fixes to the boot code.
- Prefer swap move on zephyr if the scratch partition is not enabled.
- Upgrade fault-injection hardening, improving cases injections are detected.
- Add a new flash api `flash_area_get_sector`, along with support for each
  target, that replaces `flash_area_sector_from_off`. This is a step in cleaning
  up the flash API used by MCUboot.

### Security fixes

There are no security vulnerabilities reported on the MCUboot code for this
release. There have been several updates to the dependencies in the Ruby code
used to generate the documentation. This should only affect users that generate
their own documentation.

## Version 1.9.0

The 1.9.0 release of MCUboot contains various bug fixes, improves
support on some recent targets, and adds support for devices with a
write alignment larger than 8.

This change introduces a potentially incompatible change to the format
of the image trailer.  If `BOOT_MAX_ALIGN` is kept at 8, the trailer
format does not change.  However, to support larger write alignments,
this value can be increased, which will result in a different magic
number value.  These targets were previously unsupported in MCUboot,
so this change should not affect any existing targets.  The change has
been tested with a `BOOT_MAX_ALIGN` up to 32 bytes.

### About this release

- Add native flash encryption to Espressif targets
- Numerous documentation improvements
- Increase coverage of large images in the simulator
- Add stm32 watchdog support
- Add support for the `mimxrt685_evk` board
- Add support for "partial multi-image booting"
- Add support for clear image generation with encryption capability to
  imgtool
- Fix Zephyr when `CONFIG_BOOT_ENCRYPTION_KEY_FILE` is not defined
- Remove zephyr example test running in shell.  The Go version is
  primary and much more featureful.
- imgtool: make `--max-align` default reasonable in most cases.
- Implement the mcumgr echo command in serial boot mode

### Security fixes

## Version 1.8.0

The 1.8.0 release of MCUboot contains numerous fixes, and adds support
for the NuttX RTOS, and the Espressif ESP32 SDK.

### About this release

- Add support for the NuttX RTOS.
- Add support for the Espressif ESP32 SDK.
- `boot_serial` changed to use cddl-gen, which removes the dependency
  on tinycbor.
- Add various hooks to be able to change how image data is accessed.
- Cypress supports Mbed TLS for encryption.
- Support using Mbed TLS for ECDSA.  This can be useful if Mbed TLS is
  brought in for another reason.
- Add simulator support for testing direct-XIP and ramload.
- Support Mbed TLS 3.0.  Updates the submodule for Mbed TLS to 3.0.
- Enable direct-xip mode in Mbed-OS port.
- extract `bootutil_public` library, a common interface for MCUboot
  and the application.
- Allow to boot primary image if secondary one is unreachable.
- Add AES256 image encryption support.
- Add Multiimage boot for direct-xip and ram-load mode.
- Cargo files moved to top level, now `cargo test` can be run from the
  top level directory.
- Fault injection tests use updated TF-M.
- Thingy:53 now supports multi-image DFU.
- ram load and image encryption can be used together, allowing the
  entire contents of flash to always remain encrypted.

### Security fixes

- [GHSA-gcxh-546h-phg4](https://github.com/mcu-tools/mcuboot/security/advisories/GHSA-gcxh-546h-phg4)
  has been published.  There is not a fix at this time, but a caution
  to be sure to follow the instructions carefully, and make sure that
  the development keys in the repo are never used in a production
  system.

## Version 1.7.0

The 1.7.0 release of MCUboot adds support for the Mbed-OS platform,
Equal slots (direct-xip) upgrade mode, RAM loading upgrade mode,
hardening against hardware level fault injection and timing attacks
and single image mode.
There are bug fixes, and associated imgtool updates as well.

### About this release

- Initial support for the Mbed-OS platform.
- Added possibility to enter deep sleep mode after MCUboot app execution
  for cypress platform.
- Added hardening against hardware level fault injection and timing attacks.
- Introduced Abstract crypto primitives to simplify porting.
- Added RAM-load upgrade mode.
- Renamed single-image mode to single-slot mode.
- Allow larger primary slot in swap-move
- Fixed boostrapping in swap-move mode.
- Fixed issue causing that interrupted swap-move operation might brick device
  if the primary image was padded.
- Abstracting MCUboot crypto functions for cleaner porting
- Droped flash_area_read_is_empty() porting API.
- boot/zephyr: Added watchdog feed on nRF devices.
  See `CONFIG_BOOT_WATCHDOG_FEED` option.
- boot/zephyr: Added patch for turning off cache for Cortex M7 before
  chain-loading.
- boot/zephyr: added option to relocate interrupts to application
- boot/zephyr: clean ARM core configuration only when selected by user
- boot/boot_serial: allow nonaligned last image data chunk
- imgtool: added custom TLV support.
- imgtool: added possibility to set confirm flag for hex files as well.
- imgtool: Print image digest during verify.

### Zephyr-RTOS compatibility

This release of MCUboot works with the Zephyr "main" at the time of the
release. It was tested as of has 7a3b253ce. This version of MCUboot also
works with the Zephyr v2.4.0, however it is recommended to enable
`CONFIG_MCUBOOT_CLEANUP_ARM_CORE` while using that version.

## Version 1.6.0

The 1.6.0 release of MCUboot adds support for the PSOC6 platform,
X25519 encrypted images, rollback protection, hardware keys, and a
shared boot record to communicate boot attestation information to
later boot stages.  There are bug fixes, and associated imgtool
updates as well.

### About this release

- Initial support for the Cypress PSOC6 plaformt.  This platform
  builds using the Cypress SDK, which has been added as submodules.
- CBOR decoding in serial recovery replaced by code generated from a
  CDDL description.
- Add support for X25519 encrypted images.
- Add rollback protection.  There is support for a HW rollback counter
  (which must be provided as part of the platform), as well as a SW
  solution that protects against some types of rollback.
- Add an optional boot record in shared memory to communicate boot
  attributes to later-run code.
- Add support for hardware keys.
- Various fixes to work with the latest Zephyr version.

### Security issues addressed

- CVE-2020-7595 "xmlStringLenDecodeEntities in parser.c in libxml2
  2.9.10 has an infinite loop in a certain end-of-file situation." Fix
  by updating a dependency in documentation generation.

### Zephyr-RTOS compatibility

This release of MCUboot works the Zephyr "main" at the time of the
release.  It was tested as of has 1a89ca1238.  When Zephyr v2.3.0 is
released, there will be a possible 1.6.1 or similar release of Zephyr
if needed to address any issues.  There also may be branch releases of
MCUboot specifically for the current version of Zephyr, e.g.
v1.6.0-zephyr-2.2.1.

## Version 1.5.0

The 1.5.0 release of MCUboot adds support for encrypted images using
ECIES with secp256r1 as an Elliptic Curve alternative to RSA-OAEP. A
new swap method was added which allows for upgrades without using a
scratch partition. There are also lots of bug fixes, extra simulator
testing coverage and some imgtool updates.

### About this release

- TLVs were updated to use 16-bit lengths (from previous 8). This
  should work with no changes for little-endian targets, but will
  break compatibility with big-endian targets.
- A benchmark framework was added to Zephyr
- ed25519 signature validation can now build without using Mbed TLS
  by relying on a bundled tinycrypt based sha-512 implementation.
- imgtool was updated to correctly detect trailer overruns by image.
- Encrypted image TLVs can be saved in swap metadata during a swap
  upgrade instead of the plain AES key.
- imgtool can dump private keys in C format (getpriv command), which
  can be added as decryption keys. Optionally can remove superfluous
  fields from the ASN1 by passing it `--minimal`.
- Lots of other smaller bugs fixes.
- Added downgrade prevention feature (available when the overwrite-based
  image update strategy is used)

### Known issues

- TLV size change breaks compatibility with big-endian targets.

## Version 1.4.0

The 1.4.0 release of MCUboot primarily adds support for multi-image
booting.  With this release, MCUboot can manage two images that can be
updated independently.  With this, it also supports additions to the
TLV that allow these dependencies to be specified.

Multi-image support adds backward-incompatible changes to the format
of the images: specifically adding support for protected TLV entries.
If multiple images and dependencies are not used, the images will be
compatible with previous releases of MCUboot.

### About this release

- Fixed CVE-2019-5477, and CVE-2019-16892.  These fix issue with
  dependencies used in the generation of the documentation on github.
- Numerous code cleanups and refactorings
- Documentation updates for multi-image features
- Update imgtool.py to support the new features
- Updated the Mbed TLS submodule to current stable version 2.16.3
- Moved the Mbed TLS submodule from within sim/mcuboot-sys to ext.
  This will make it easier for other board supports to use this code.
- Added some additional overflow and bound checks to data in the image
  header, and TLV data.
- Add a `-x` (or `--hex_addr`) flag to imgtool to set the base address
  written to a hex-format image.  This allows the image to be flashed
  at an offset, without having to use additional tools to modify the
  image.

## Version 1.3.1

The 1.3.1 release of MCUboot consists mostly of small bug fixes and updates.
There are no breaking changes in functionality. This release should work with
Mynewt 1.6.0 and up, and any Zephyr `main` after sha
f51e3c296040f73bca0e8fe1051d5ee63ce18e0d.

### About this release

- Fixed a revert interruption bug
- Added ed25519 signing support
- Added RSA-3072 signing support
- Allow ec256 to run on CC310 interface
- Some preparation work was done to allow for multi image support, which
  should land in 1.4.0. This includes a simulator update for testing
  multi-images, and a new name for slot0/slot1 which are now called
  "primary slot" and "secondary slot".
- Other minor bugfixes and improvements

## Version 1.3.0

The 1.3.0 release of MCUboot brings in many fixes and updates.  There
are no breaking changes in functionality.  Many of the changes are
refactorings that will make the code easier to maintain going forward.
In addition, support has been added for encrypted images.  See [the
docs](encrypted_images.md) for more information.

### About this release

- Modernize the Zephyr build scripts.
- Add a `ptest` utility to help run the simulator in different
  configurations.
- Migrate the simulator to Rust 2018 edition.  The sim now requires at
  least Rust 1.32 to build.
- Simulator cleanups.  The simulator code is now built the same way
  for every configuration, and queries the MCUboot code for how it was
  compiled.
- Abstract logging in MCUboot.  This was needed to support the new
  logging system used in Zephyr.
- Add multiple flash support.  Allows slot1/scratch to be stored in an
  external flash device.
- Add support for [encrypted images](encrypted_images.md).
- Add support for flash devices that read as '0' when erased.
- Add support to Zephyr for the `nrf52840_pca10059`.  This board
  supports serial recovery over USB with CDC ACM.
- imgtool is now also available as a python package on pypi.org.
- Add an option to erase flash pages progressively during recovery to
  avoid possible timeouts (required especially by serial recovery
  using USB with CDC ACM).
- imgtool: big-endian support
- imgtool: saves in intel-hex format when output filename has `.hex`
  extension; otherwise saves in binary format.

## Version 1.2.0

The 1.2.0 release of MCUboot brings a lot of fixes/updates, where much of the
changes were on the boot serial functionality and imgtool utility. There are
no breaking changes in MCUboot functionality, but some of the CLI parameters
in imgtool were changed (either removed or added or updated).

### About this release

- imgtool accepts .hex formatted input
- Logging system is now configurable
- Most Zephyr configuration has been switched to Kconfig
- Build system accepts .pem files in build system to autogenerate required
  key arrays used internally
- Zephyr build switched to using built-in flash_map and TinyCBOR modules
- Serial boot has substantially decreased in space usage after refactorings
- Serial boot build doesn't require newlib-c anymore on Zephyr
- imgtool updates:
  + "create" subcommand can be used as an alias for "sign"
  + To allow imgtool to always perform the check that firmware does not
    overflow the status area, `--slot-size` was added and `--pad` was updated
    to act as a flag parameter.
  + `--overwrite-only` can be passed if not using swap upgrades
  + `--max-sectors` can be used to adjust the maximum amount of sectors that
    a swap can handle; this value must also be configured for the bootloader
  + `--pad-header` substitutes `--included-header` with reverted semantics,
    so it's not required for firmware built by Zephyr build system

### Known issues

None

## Version 1.1.0

The 1.1.0 release of MCUboot brings a lot of fixes/updates to its
inner workings, specially to its testing infrastructure which now
enables a more thorough quality assurance of many of the available
options. As expected of the 1.x.x release cycle, no breaking changes
were made. From the tooling perpective the main addition is
newt/imgtool support for password protected keys.

### About this release

- serial recovery functionality support under Zephyr
- simulator: lots of refactors were applied, which result in the
  simulator now leveraging the Rust testing infrastructure; testing
  of ecdsa (secp256r1) was added
- imgtool: removed PKCS1.5 support, added support for password
  protected keys
- tinycrypt 0.2.8 and the Mbed TLS ASN1 parser are now bundled with
  MCUboot (eg secp256r1 is now free of external dependencies!)
- Overwrite-only mode was updated to erase/copy only sectors that
  actually store firmware
- A lot of small code and documentation fixes and updates.

### Known issues

None

## Version 1.0.0

The 1.0.0 release of MCUboot introduces a format change.  It is
important to either use the `imgtool.py` also from this release, or
pass the `-2` to recent versions of the `newt` tool in order to
generate image headers with the new format.  There should be no
incompatible format changes throughout the 1.x.y release series.

### About this release

- Header format change.  This change was made to move all of the
  information about signatures out of the header and into the TLV
  block appended to the image.  This allows
  - The signature to be replaced without changing the image.
  - Multiple signatures to be applied.  This can be used, for example,
    to sign an image with two algorithms, to support different
    bootloader configurations based on these image.
  - The public key is referred to by its SHA1 hash (or a prefix of the
    hash), instead of an index that has to be maintained with the
    bootloader.
  - Allow new types of signatures in the future.
- Support for PKCS#1 v1.5 signatures has been dropped.  All RSA
  signatures should be made with PSS.  The tools have been changed to
  reflect this.
- The source for Tinycrypt has been placed in the MCUboot tree.  A
  recent version of Tinycrypt introduced breaking API changes.  To
  allow MCUboot to work across various platforms, we stop using the
  Tinycrypt bundled with the OS platform, and use our own version.  A
  future release of MCUboot will update the Tinycrypt version.
- Support for some new targets:
  - Nordic nRF51 and nRF52832 dev kits
  - Hexiwear K64
- Clearer sample applications have been added under `samples`.
- Test plans for [zephyr](testplan-zephyr.md), and
  [mynewt](testplan-mynewt.md).
- The simulator is now able to test RSA signatures.
- There is an unimplemented `load_addr` header for future support for
  RAM loading in the bootloader.
- Numerous documentation.

### Known issues

None

## Version 0.9.0

This is the first release of MCUboot, a secure bootloader for 32-bit MCUs.
It is designed to be operating system-agnostic and works over any transport -
wired or wireless. It is also hardware independent, and relies  on hardware
porting layers from the operating system it works with. For the first release,
we have support for three open source operating systems: Apache Mynewt, Zephyr
and RIOT.

### About this release

- This release supports building with and running Apache Mynewt and Zephyr
  targets.
- RIOT is supported as a running target.
- Image integrity is provided with SHA256.
- Image originator authenticity is provided supporting the following
  signature algorithms:
  - RSA 2048 and RSA PKCS#1 v1.5 or v2.1
  - Elliptic curve DSA with secp224r1 and secp256r1
- Two firmware upgrade algorithms are provided:
  - An overwrite only which upgrades slot 0 with the image in slot 1.
  - A swapping upgrade which enables image test, allowing for rollback to a
    previous known good image.
- Supports both Mbed TLS and tinycrypt as backend crypto libraries. One of them
  must be defined and the chosen signing algorithm will require a particular
  library according to this list:
  - RSA 2048 needs Mbed TLS
  - ECDSA secp224r1 needs Mbed TLS
  - ECDSA secp256r1 needs tinycrypt as well as the ASN.1 code from Mbed TLS
    (so still needs that present).

### Known issues

- The image header and TLV formats are planned to change with release 1.0:
  https://runtimeco.atlassian.net/browse/MCUB-66
