# [Building and using MCUboot with Espressif's chips](#building-and-using-mcuboot-with-espressifs-chips)

The MCUBoot Espressif's port depends on HAL (Hardware Abstraction Layer) sources based on ESP-IDF or 3rd party frameworks as such as Zephyr-RTOS (`zephyrproject-rtos/hal_espressif/`) or NuttX RTOS (`espressif/esp-hal-3rdparty`). Building the MCUboot Espressif's port and its features is platform dependent, therefore, the system environment including toolchains, must be set accordingly. A standalone build version means that ESP-IDF and its toolchain are used as source. For 3rd parties framework, HAL path and toolchain must be set.

Documentation about the MCUboot bootloader design, operation and features can be found in the [design document](design.md).

## [SoC support availability](#soc-support-availability)

The current port is available for use in the following SoCs within the OSes:

| | ESP32 | ESP32-S2 | ESP32-C3 | ESP32-S3 | ESP32-C2 | ESP32-C6 | ESP32-H2 |
| :-----: | :-----: | :-----: | :-----: | :-----: | :-----: | :-----: | :-----: |
| Zephyr | Supported | Supported | Supported | Supported | In progress | In progress | In progress |
| NuttX | Supported | Supported | Supported | Supported | In progress | In progress | In progress |

Notice that any customization in the memory layout from the OS application must be done aware of the bootloader own memory layout to avoid overlapping. More information on the section [Memory map organization for OS compatibility](#memory-map-organization-for-os-compatibility).

## [Installing requirements and dependencies](#installing-requirements-and-dependencies)

The following instructions considers a MCUboot Espressif port standalone build.

1. Install additional packages required for development with MCUboot:
```bash
cd ~/mcuboot  # or to your directory where MCUboot is cloned
```
```bash
pip3 install --user -r scripts/requirements.txt
```

2. Update the Mbed TLS submodule required by MCUboot:
```bash
git submodule update --init --recursive ext/mbedtls
```

3. If ESP-IDF is the chosen option for use as HAL layer and the system already have ESP-IDF installed, ensure that the environment is set:
```bash
<IDF_PATH>/install.sh
```
```bash
. <IDF_PATH>/export.sh
```

---
***Note***

*If desirable, instructions for ESP-IDF installation can be found [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#manual-installation)*

---

---
***Note***

*The other HALs mentioned above like `hal_espressif` from Zephyr RTOS or `esp-hal-3rdparty` from NuttX RTOS environments also can be used for the bootloader standalone build, however as eventually code revision may differ from what is currently expected, it is recommended using them only within their RTOS build system.*

---

4. If ESP-IDF is not installed and will not be used, install `esptool`:
```bash
pip3 install esptool
```

## [Building the bootloader itself](#building-the-bootloader-itself)

The MCUboot Espressif port bootloader is built using the toolchain and tools provided by Espressif. Additional configuration related to MCUboot features and slot partitioning may be made using the `port/<TARGET>/bootloader.conf` file or passing a custom config file using the `-DMCUBOOT_CONFIG_FILE` argument on the first step below.

---
***Note***

*Replace `<TARGET>` with the target ESP32 family (like `esp32`, `esp32s2` and others).*

---

1. Compile and generate the BIN:
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=tools/toolchain-<TARGET>.cmake -DMCUBOOT_TARGET=<TARGET> -DESP_HAL_PATH=<ESP_HAL_PATH> -DMCUBOOT_FLASH_PORT=<PORT> -B build -GNinja
```
```bash
ninja -C build/
```

---
***Note***

*If using ESP-IDF as HAL layer source, `ESP_HAL_PATH` can be ommited.*

---

2. Flash MCUboot in your device:
```bash
ninja -C build/ flash
```

If `MCUBOOT_FLASH_PORT` arg was not passed to `cmake`, the default `PORT` for flashing will be `/dev/ttyUSB0`.

Alternatively:
```bash
esptool.py -p <PORT> -b <BAUD> --before default_reset --after no_reset --chip <TARGET> write_flash --flash_mode dio --flash_size <FLASH_SIZE> --flash_freq 40m <BOOTLOADER_FLASH_OFFSET> build/mcuboot_<TARGET>.bin
```
---
***Note***

You may adjust the port `<PORT>` (like `/dev/ttyUSB0`) and baud rate `<BAUD>` (like `2000000`) according to the connection with your board.
You can also skip `<PORT>` and `<BAUD>` parameters so that esptool tries to automatically detect it.

*`<FLASH_SIZE>` can be found using the command below:*
```bash
esptool.py -p <PORT> -b <BAUD> flash_id
```
The output contains device information and its flash size:
```
Detected flash size: 4MB
```


*`<BOOTLOADER_FLASH_OFFSET>` value must follow one of the addresses below:*

| ESP32 | ESP32-S2 | ESP32-C3 | ESP32-S3 | ESP32-C2 | ESP32-C6 | ESP32-H2 |
| :-----: | :-----: | :-----: | :-----: | :-----: | :-----: | :-----: |
| 0x1000 | 0x1000 | 0x0000 | 0x0000 | 0x0000 | 0x0000 | 0x0000 |

---

3. Reset your device

## [Signing and flashing an application](#signing-and-flashing-an-application)

1. Images can be regularly signed with the `scripts/imgtool.py` script:
```bash
imgtool.py sign --align 4 -v 0 -H 32 --pad-header -S <SLOT_SIZE> <BIN_IN> <SIGNED_BIN>
```

---

***Note***

`<SLOT_SIZE>` is the size of the slot to be used.
Default slot0 size is `0x100000`, but it can change as per application flash partitions.

For Zephyr images, `--pad-header` is not needed as it already has the padding for MCUboot header.

---

:warning: ***ATTENTION***

*This is the basic signing needed for adding MCUboot headers and trailers.
For signing with a crypto key and guarantee the authenticity of the image being booted, see the section [MCUboot image signature verification](#mcuboot-image-signature-verification) below.*

---

2. Flash the signed application:
```bash
esptool.py -p <PORT> -b <BAUD> --before default_reset --after hard_reset --chip <TARGET>  write_flash --flash_mode dio --flash_size <FLASH_SIZE> --flash_freq 40m <SLOT_OFFSET> <SIGNED_BIN>
```

# [Downgrade prevention](#downgrade-prevention)

Downgrade prevention (avoid updating of images to an older version) can be enabled using the following configuration:

```
CONFIG_ESP_DOWNGRADE_PREVENTION=y
```

MCUboot will then verify and compare the new image version number with the current one before perform an update swap.

Version number is added to the image when signing it with `imgtool` (`-v` parameter, e.g. `-v 1.0.0`).

### [Downgrade prevention with security counter](#downgrade-prevention-with-security-counter)

It is also possible to rely on a security counter, also added to the image when signing with `imgtool` (`-s` parameter), apart from version number. This allows image downgrade at some extent, since any update must have greater or equal security counter value. Enable using the following configuration:

```
CONFIG_ESP_DOWNGRADE_PREVENTION_SECURITY_COUNTER=y
```

E.g.: if the current image was signed using `-s 1` parameter, an eventual update image must have been signed using security counter `-s 1` or greater.

# [Security Chain on Espressif port](#security-chain-on-espressif-port)

[MCUboot encrypted images](encrypted_images.md) do not provide full code confidentiality when only external storage is available (see [Threat model](encrypted_images.md#threat-model)) since by MCUboot design the image in Primary Slot, from where the image is executed, is stored plaintext.
Espressif chips have off-chip flash memory, so to ensure a security chain along with MCUboot image signature verification, the hardware-assisted Secure Boot and Flash Encryption were made available on the MCUboot Espressif port.

## [MCUboot image signature verification](#mcuboot-image-signature-verification)

The image that MCUboot is booting can be signed with 4 types of keys: RSA-2048, RSA-3072, EC256 and ED25519. In order to enable the feature, the **bootloader** must be compiled with the following configurations:

---
***Note***

*It is strongly recommended to generate a new signing key using `imgtool` instead of use the existent samples.*

---

#### For EC256 algorithm use
```
CONFIG_ESP_SIGN_EC256=y

# Use Tinycrypt lib for EC256 or ED25519 signing
CONFIG_ESP_USE_TINYCRYPT=y

CONFIG_ESP_SIGN_KEY_FILE=<YOUR_SIGNING_KEY.pem>
```

#### For ED25519 algorithm use
```
CONFIG_ESP_SIGN_ED25519=y

# Use Tinycrypt lib for EC256 or ED25519 signing
CONFIG_ESP_USE_TINYCRYPT=y

CONFIG_ESP_SIGN_KEY_FILE=<YOUR_SIGNING_KEY.pem>
```

#### For RSA (2048 or 3072) algorithm use
```
CONFIG_ESP_SIGN_RSA=y
# RSA_LEN is 2048 or 3072
CONFIG_ESP_SIGN_RSA_LEN=<RSA_LEN>

# Use Mbed TLS lib for RSA image signing
CONFIG_ESP_USE_MBEDTLS=y

CONFIG_ESP_SIGN_KEY_FILE=<YOUR_SIGNING_KEY.pem>
```

Notice that the public key will be embedded in the bootloader code, since the hardware key storage is not supported by Espressif port.

### [Signing the image](#signing-the-image)

Now you need to sign the **image binary**, use the `imgtool` with `-k` parameter:
```bash
imgtool.py sign -k <YOUR_SIGNING_KEY.pem> --pad --pad-sig --align 4 -v 0 -H 32 --pad-header -S 0x00100000 <BIN_IN> <BIN_OUT>
```
If signing a Zephyr image, the `--pad-header` is not needed, as it already have the padding for MCUboot header.


## [Secure Boot](#secure-boot)

The Secure Boot implementation is based on [IDF's Secure Boot V2](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/secure-boot-v2.html), is hardware-assisted and RSA based, and has the role for ensuring that only authorized code will be executed on the device. This is done through bootloader signature checking by the ROM bootloader. \
***Note***: ROM bootloader is the First Stage Bootloader, while the Espressif MCUboot port is the Second Stage Bootloader.

***Note***: Currently on MCUboot Espressif Port, the Secure Boot V2 for ESP32-C2 is not supported yet.

### [Building bootloader with Secure Boot](#building-bootloader-with-secure-boot)

In order to build the bootloader with the feature on, the following configurations must be enabled:
```
CONFIG_SECURE_BOOT=1
CONFIG_SECURE_BOOT_V2_ENABLED=1
CONFIG_SECURE_SIGNED_ON_BOOT=1
CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME=1
CONFIG_SECURE_BOOT_SUPPORTS_RSA=1
```

---
:warning: ***ATTENTION***

*On development phase is recommended add the following configuration in order to keep the debugging enabled and also to avoid any unrecoverable/permanent state change:*
```
CONFIG_SECURE_BOOT_ALLOW_JTAG=1
CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_CACHE=1

# Options for enabling eFuse emulation in Flash
CONFIG_EFUSE_VIRTUAL=1
CONFIG_EFUSE_VIRTUAL_KEEP_IN_FLASH=1
```

---

---
:warning: ***ATTENTION***

*You can disable UART Download Mode by adding the following configuration:*
```
CONFIG_SECURE_DISABLE_ROM_DL_MODE=1
```

*This may be suitable for **production** builds. **After disabling UART Download Mode you will not be able to flash other images through UART.***

*Otherwise, you can switch the UART ROM Download Mode to the Secure Download Mode. It will limit the use of Download Mode functions to simple flash read, write and erase operations.*
```
CONFIG_SECURE_ENABLE_SECURE_ROM_DL_MODE=1
```

*Once the device makes its first full boot, these configurations cannot be reverted*

---

Once the **bootloader image** is built, the resulting binary file is required to be signed with `espsecure.py` tool.

First create a signing key:
```bash
espsecure.py generate_signing_key --version 2 <BOOTLOADER_SIGNING_KEY.pem>
```

Then sign the bootloader image:
```bash
espsecure.py sign_data --version 2 --keyfile <BOOTLOADER_SIGNING_KEY.pem> -o <BOOTLOADER_BIN_OUT> <BOOTLOADER_BIN_IN>
```

---
:warning: ***ATTENTION***

*Once the bootloader is flashed and the device resets, the **first boot will enable Secure Boot** and the bootloader and key **no longer can be modified**. So **ENSURE** that both bootloader and key are correct and you did not forget anything before flashing.*

---

Flash the bootloader as following, with `--after no_reset` flag, so you can reset the device only when assured:
```bash
esptool.py -p <PORT> -b 2000000 --after no_reset --chip <ESP_CHIP> write_flash --flash_mode dio --flash_size <FLASH_SIZE> --flash_freq 40m <BOOTLOADER_FLASH_OFFSET> <SIGNED_BOOTLOADER_BIN>
```

### [Secure Boot Process](#secure-boot-process)

Secure boot uses a signature block appended to the bootloader image in order to verify the authenticity. The signature block contains the RSA-3072 signature of that image and the RSA-3072 public key.

On its **first boot** the Secure Boot is not enabled on the device eFuses yet, neither the key nor digests. So the first boot will have the following process:

1. On startup, since it is the first boot, the ROM bootloader will not verify the bootloader image (the Secure Boot bit in the eFuse is disabled) yet, so it proceeds to execute it (our MCUboot bootloader port).
2. Bootloader calculates the SHA-256 hash digest of the public key and writes the result to eFuse.
3. Bootloader validates the application images and prepare the booting process (MCUboot phase).
4. Bootloader burns eFuse to enable Secure Boot V2.
5. Bootloader proceeds to load the Primary image.

After that the Secure Boot feature is permanently enabled and on every next boot the ROM bootloader will verify the MCUboot bootloader image.
The process of an usual boot:

1. On startup, the ROM bootloader checks the Secure Boot enable bit in the eFuse. If it is enabled, the boot will proceed as following.
2. ROM bootloader verifies the bootloader's signature block integrity (magic number and CRC). Interrupt boot if it fails.
3. ROM bootloader verifies the bootloader image, interrupt boot if any step fails.: \
3.1. Compare the SHA-256 hash digest of the public key embedded in the bootloader’s signature block with the digest saved in the eFuses. \
3.2. Generate the application image digest and match it with the image digest in the signature block. \
3.3. Use the public key to verify the signature of the bootloader image, using RSA-PSS with the image digest calculated from previous step for comparison.
4. ROM bootloader executes the bootloader image.
5. Bootloader does the usual verification (MCUboot phase).
6. Proceeds to boot the Primary image.

## [Flash Encryption](#flash-encryption)

The Espressif Flash Encryption is hardware-assisted, transparent to the MCUboot process and is an additional security measure beyond MCUboot existent features.
The Flash Encryption implementation is also based on [IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/flash-encryption.html) and is intended for encrypting off-chip flash memory contents, so it is protected against physical reading.

When enabling the Flash Encryption, the user can encrypt the content either using a **device generated key** (remains unknown and unreadable) or a **host generated key** (owner is responsible for keeping the key private and safe). After the flash encryption gets enabled through eFuse burning on the device, all read and write operations are decrypted/encrypted in runtime.

### [Building bootloader with Flash Encryption](#building-bootloader-with-flash-encryption)

In order to build the bootloader with the feature on, the following configurations must be enabled:

For **release mode**:
```
CONFIG_SECURE_FLASH_ENC_ENABLED=1
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=1
```

For **development mode**:
```
CONFIG_SECURE_FLASH_ENC_ENABLED=1
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT=1
```

---
:warning: ***ATTENTION***

*On development phase is strongly recommended adding the following configuration in order to keep the debugging enabled and also to avoid any unrecoverable/permanent state change:*
```
CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_ENC=1
CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_DEC=1
CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_CACHE=1
CONFIG_SECURE_BOOT_ALLOW_JTAG=1

# Options for enabling eFuse emulation in Flash
CONFIG_EFUSE_VIRTUAL=1
CONFIG_EFUSE_VIRTUAL_KEEP_IN_FLASH=1
```
---

---
:warning: ***ATTENTION***

*Unless the recommended flags for **DEVELOPMENT MODE** were enabled, the actions made by Flash Encryption process are **PERMANENT**.* \
*Once the bootloader is flashed and the device resets, the **first boot will enable Flash Encryption, encrypt the flash content including bootloader and image slots, burn the eFuses that no longer can be modified** and if device generated the key **it will not be recoverable**.* \
*When on **RELEASE MODE**, **ENSURE** that the application with an update agent is flashed before reset the device.*

*In the same way as Secure Boot feature, you can disable UART Download Mode by adding the following configuration:*
```
CONFIG_SECURE_DISABLE_ROM_DL_MODE=1
```

*This may be suitable for **production** builds. **After disabling UART Download Mode you will not be able to flash other images through UART.***

*Otherwise, you can switch the UART Download Mode to the Secure Download Mode. It will limit the use of Download Mode functions to simple flash read, write and erase operations.*
```
CONFIG_SECURE_ENABLE_SECURE_ROM_DL_MODE=1
```

*These configurations cannot be reverted after the device's first boot*

---

### [Signing the image when working with Flash Encryption](#signing-the-image-when-working-with-flash-encryption)

When enabling flash encryption, it is required to signed the image using 32-byte alignment: `--align 32 --max-align 32`.

Command example:
```bash
imgtool.py sign -k <YOUR_SIGNING_KEY.pem> --pad --pad-sig --align 32 --max-align 32 -v 0 -H 32 --pad-header -S <SLOT_SIZE> <BIN_IN> <BIN_OUT>
```

### [Device generated key](#device-generated-key)

First ensure that the application image is able to perform encrypted read and write operations to the SPI Flash.
Flash the bootloader and application normally:
```bash
esptool.py -p <PORT> -b 2000000 --after no_reset --chip <ESP_CHIP> write_flash --flash_mode dio --flash_size <FLASH_SIZE> --flash_freq 40m <BOOTLOADER_FLASH_OFFSET> <BOOTLOADER_BIN>
```
```bash
esptool.py -p <PORT> -b 2000000 --after no_reset --chip <ESP_CHIP> write_flash --flash_mode dio --flash_size <FLASH_SIZE> --flash_freq 40m <PRIMARY_SLOT_FLASH_OFFSET> <APPLICATION_BIN>
```

On the **first boot**, the bootloader will:
1. Generate Flash Encryption key and write to eFuse.
2. Encrypt flash in-place including bootloader, image primary/secondary slot and scratch.
3. Burn eFuse to enable Flash Encryption.
4. Reset system to ensure Flash Encryption cache resets properly.

### [Host generated key](#host-generated-key)

First ensure that the application image is able to perform encrypted read and write operations to the SPI Flash. Also ensure that the **UART ROM Download Mode is not disabled** - or that the **Secure Download Mode is enabled**.
Before flashing, generate the encryption key using `espsecure.py` tool:
```bash
espsecure.py generate_flash_encryption_key <FLASH_ENCRYPTION_KEY.bin>
```

Burn the key into the device's eFuse (keep a copy on the host), this action can be done **only once**:

---
:warning: ***ATTENTION***

*eFuse emulation in Flash configuration options do not have any effect, so if the key burning command below is used, it will actually burn the physical eFuse.*

---

- ESP32
```bash
espefuse.py --port PORT burn_key flash_encryption <FLASH_ENCRYPTION_KEY.bin>
```

- ESP32S2, ESP32C3 and ESP32S3
```bash
espefuse.py --port PORT burn_key BLOCK <FLASH_ENCRYPTION_KEY.bin> <KEYPURPOSE>
```

BLOCK is a free keyblock between BLOCK_KEY0 and BLOCK_KEY5. And KEYPURPOSE is either XTS_AES_128_KEY, XTS_AES_256_KEY_1, XTS_AES_256_KEY_2 (AES XTS 256 is available only in ESP32S2).

Now, similar as the Device generated key, the bootloader and application can be flashed plaintext. The **first boot** will encrypt the flash content using the host key burned in the eFuse instead of generate a new one.

Flashing the bootloader and application:
```bash
esptool.py -p <PORT> -b 2000000 --after no_reset --chip <ESP_CHIP> write_flash --flash_mode dio --flash_size <FLASH_SIZE> --flash_freq 40m <BOOTLOADER_FLASH_OFFSET> <BOOTLOADER_BIN>
```
```bash
esptool.py -p <PORT> -b 2000000 --after no_reset --chip <ESP_CHIP> write_flash --flash_mode dio --flash_size <FLASH_SIZE> --flash_freq 40m <PRIMARY_SLOT_FLASH_OFFSET> <APPLICATION_BIN>
```

On the **first boot**, the bootloader will:
1. Encrypt flash in-place including bootloader, image primary/secondary slot and scratch using the written key.
2. Burn eFuse to enable Flash Encryption.
3. Reset system to ensure Flash Encryption cache resets properly.

Encrypting data on the host:
- ESP32
```bash
espsecure.py encrypt_flash_data --keyfile <FLASH_ENCRYPTION_KEY.bin> --address <FLASH_OFFSET> --output <OUTPUT_DATA> <INPUT_DATA>
```

- ESP32-S2, ESP32-C3 and ESP32-S3
```bash
espsecure.py encrypt_flash_data --aes_xts --keyfile <FLASH_ENCRYPTION_KEY.bin> --address <FLASH_OFFSET> --output <OUTPUT_DATA> <INPUT_DATA>
```

---
***Note***

OTA updates are required to be sent plaintext. The reason is that, as said before, after the Flash Encryption is enabled all read/write operations are decrypted/encrypted in runtime, so as e.g. if pre-encrypted data is sent for an OTA update, it would be wrongly double-encrypted when the update agent writes to the flash.

For updating with an image encrypted on the host, flash it through serial using `esptool.py` as above. **UART ROM Download Mode must not be disabled**.

---

## [Security Chain scheme](#security-chain-scheme)

Using the 3 features, Secure Boot, Image signature verification and Flash Encryption, a Security Chain can be established so only trusted code is executed, and also the code and content residing in the off-chip flash are protected against undesirable reading.

The overall final process when all features are enabled:
1. ROM bootloader validates the MCUboot bootloader using RSA signature verification.
2. MCUboot bootloader validates the image using the chosen algorithm EC256/RSA/ED25519. It also validates an upcoming image when updating.
3. Flash Encryption guarantees that code and data are not exposed.

### [Size Limitation](#size-limitation)

When all 3 features are enable at same time, the bootloader size may exceed the fixed limit for the ROM bootloader checking on the Espressif chips **depending on which algorithm** was chosen for MCUboot image signing. The issue https://github.com/mcu-tools/mcuboot/issues/1262 was created to track this limitation.

## [Multi image](#multi-image)

The multi image feature (currently limited to 2 images) allows the images to be updated separately (each one has its own primary and secondary slot) by MCUboot.

The Espressif port bootloader handles the boot in two different approaches:

### [Host OS boots second image](#host-os-boots-second-image)

Host OS from the *first image* is responsible for booting the *second image*, therefore the bootloader is aware of the second image regions and can update it, however it does not load neither boots it.

Configuration example (`bootloader.conf`):
```
CONFIG_ESP_BOOTLOADER_SIZE=0xF000
CONFIG_ESP_MCUBOOT_WDT_ENABLE=y

# Enables multi image, if it is not defined, its assumed
# only one updatable image
CONFIG_ESP_IMAGE_NUMBER=2

# Example of values to be used when multi image is enabled
# Notice that the OS layer and update agent must be aware
# of these regions
CONFIG_ESP_APPLICATION_SIZE=0x50000
CONFIG_ESP_IMAGE0_PRIMARY_START_ADDRESS=0x10000
CONFIG_ESP_IMAGE0_SECONDARY_START_ADDRESS=0x60000
CONFIG_ESP_IMAGE1_PRIMARY_START_ADDRESS=0xB0000
CONFIG_ESP_IMAGE1_SECONDARY_START_ADDRESS=0x100000
CONFIG_ESP_SCRATCH_OFFSET=0x150000
CONFIG_ESP_SCRATCH_SIZE=0x40000
```

### [Multi boot](#multi-boot)

In the multi boot approach the bootloader is responsible for booting two different images in two different CPUs, firstly the *second image* on the APP CPU and then the *first image* on the PRO CPU (current CPU), it is also responsible for update both images as well. Thus multi boot will be only supported by Espressif multi core chips - currently only ESP32 is implemented.

---
***Note***

*The host OSes in each CPU must handle how the resources are divided/controlled between then.*

---

Configuration example:
```
CONFIG_ESP_BOOTLOADER_SIZE=0xF000
CONFIG_ESP_MCUBOOT_WDT_ENABLE=y

# Enables multi image, if it is not defined, its assumed
# only one updatable image
CONFIG_ESP_IMAGE_NUMBER=2

# Enables multi image boot on independent processors
# (main host OS is not responsible for booting the second image)
# Use only with CONFIG_ESP_IMAGE_NUMBER=2
CONFIG_ESP_MULTI_PROCESSOR_BOOT=y

# Example of values to be used when multi image is enabled
# Notice that the OS layer and update agent must be aware
# of these regions
CONFIG_ESP_APPLICATION_SIZE=0x50000
CONFIG_ESP_IMAGE0_PRIMARY_START_ADDRESS=0x10000
CONFIG_ESP_IMAGE0_SECONDARY_START_ADDRESS=0x60000
CONFIG_ESP_IMAGE1_PRIMARY_START_ADDRESS=0xB0000
CONFIG_ESP_IMAGE1_SECONDARY_START_ADDRESS=0x100000
CONFIG_ESP_SCRATCH_OFFSET=0x150000
CONFIG_ESP_SCRATCH_SIZE=0x40000
```

### [Image version dependency](#image-version-dependency)

MCUboot allows version dependency check between the images when updating them. As `imgtool.py` allows a version assigment when signing an image, it is also possible to add the version dependency constraint:
```bash
imgtool.py sign --align 4 -v <VERSION> -d "(<IMAGE_INDEX>, <VERSION_DEPENDENCY>)" -H 32 --pad-header -S <SLOT_SIZE> <BIN_IN> <SIGNED_BIN>
```

- `<VERSION>` defines the version of the image being signed.
- `"(<IMAGE_INDEX>, <VERSION_DEPENDENCY>)"` defines the minimum version and from which image is needed to satisfy the dependency.

---
Example:
```bash
imgtool.py sign --align 4 -v 1.0.0 -d "(1, 0.0.1+0)" -H 32 --pad-header -S 0x100000 image0.bin image0-signed.bin
```

Supposing that the image 0 is being signed, its version is 1.0.0 and it depends on image 1 with version at least 0.0.1+0.

---

## [Serial recovery mode](#serial-recovery-mode)

Serial recovery mode allows management through MCUMGR (more information and how to install it: https://github.com/apache/mynewt-mcumgr-cli) for communicating and uploading a firmware to the device.

Configuration example:
```
# Enables the MCUboot Serial Recovery, that allows the use of
# MCUMGR to upload a firmware through the serial port
CONFIG_ESP_MCUBOOT_SERIAL=y
# GPIO used to boot on Serial Recovery
CONFIG_ESP_SERIAL_BOOT_GPIO_DETECT=32
# GPIO input type (0 for Pull-down, 1 for Pull-up)
CONFIG_ESP_SERIAL_BOOT_GPIO_INPUT_TYPE=0
# GPIO signal value
CONFIG_ESP_SERIAL_BOOT_GPIO_DETECT_VAL=1
# Delay time for identify the GPIO signal
CONFIG_ESP_SERIAL_BOOT_DETECT_DELAY_S=5
# UART port used for serial communication
CONFIG_ESP_SERIAL_BOOT_UART_NUM=1
# GPIO for Serial RX signal
CONFIG_ESP_SERIAL_BOOT_GPIO_RX=25
# GPIO for Serial TX signal
CONFIG_ESP_SERIAL_BOOT_GPIO_TX=26
```

When enabled, the bootloader checks the if the GPIO `<CONFIG_ESP_SERIAL_BOOT_GPIO_DETECT>` configured has the signal value `<CONFIG_ESP_SERIAL_BOOT_GPIO_DETECT_VAL>` for approximately `<CONFIG_ESP_SERIAL_BOOT_DETECT_DELAY_S>` seconds for entering the Serial recovery mode. Example: a button configured on GPIO 32 pressed for 5 seconds.

Serial mode then uses the UART port configured for communication (`<CONFIG_ESP_SERIAL_BOOT_UART_NUM>`, pins `<CONFIG_ESP_SERIAL_BOOT_GPIO_RX>`, `<CONFIG_ESP_SERIAL_BOOT_GPIO_RX>`).

### [Serial Recovery through USB JTAG Serial port](#serial-recovery-through-usb-jtag-serial-port)

Some chips, like ESP32-C3 and ESP32-S3 have an integrated USB JTAG Serial Controller that implements a serial port (CDC) that can also be used for handling MCUboot Serial Recovery.
More information about the USB pins and hardware configuration:
- ESP32-C3: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/usb-serial-jtag-console.html
- ESP32-S3: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/usb-serial-jtag-console.html.
- ESP32-C6: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-guides/usb-serial-jtag-console.html
- ESP32-H2: https://docs.espressif.com/projects/esp-idf/en/latest/esp32h2/api-guides/usb-serial-jtag-console.html

Configuration example:
```
# Use Serial through USB JTAG Serial port for Serial Recovery
CONFIG_ESP_MCUBOOT_SERIAL_USB_SERIAL_JTAG=y
# Use sector erasing (recommended) instead of entire image size
# erasing when uploading through Serial Recovery
CONFIG_ESP_MCUBOOT_ERASE_PROGRESSIVELY=y
# GPIO used to boot on Serial Recovery
CONFIG_ESP_SERIAL_BOOT_GPIO_DETECT=5
# GPIO input type (0 for Pull-down, 1 for Pull-up)
CONFIG_ESP_SERIAL_BOOT_GPIO_INPUT_TYPE=0
# GPIO signal value
CONFIG_ESP_SERIAL_BOOT_GPIO_DETECT_VAL=1
# Delay time for identify the GPIO signal
CONFIG_ESP_SERIAL_BOOT_DETECT_DELAY_S=5
```

---
:warning: ***ATTENTION***

*When working with Flash Encryption enabled, `CONFIG_ESP_MCUBOOT_ERASE_PROGRESSIVELY` must be ***disabled***, although it is recommended for common Serial Recovery usage*

---

### [MCUMGR image upload example](#mcumgr-image-upload-example)

After entering the Serial recovery mode on the device, MCUMGR can be used as following:

Configure the connection:
```bash
mcumgr conn add esp type="serial" connstring="dev=<PORT>,baud=115200,mtu=256"
```

Upload the image (the process may take some time):
```bash
mcumgr -c esp image upload <IMAGE_BIN>
```

Reset the device:
```bash
mcumgr -c esp reset
```

---
:warning: ***ATTENTION***

*Serial recovery mode uploads the image to the PRIMARY_SLOT, therefore if the upload process gets interrupted the image may be corrupted and unable to boot*

---

## [Memory map organization for OS compatibility](#memory-map-organization-for-os-compatibility)

When adding support for this MCUboot port to an OS or even customizing an already supported application memory layout, it is mandatory for the OS linker script to avoid overlaping on `iram_loader_seg` and `dram_seg` bootloader RAM regions. Although part of the RAM becomes initially unavailable, it is reclaimable by the OS after boot as heap.

Therefore, the application must be designed aware of the bootloader memory usage.

---
***Note***

*Mostly of the Espressif chips have a separation on the address space for the same physical memory ammount: IRAM (accessed by the instruction bus) and DRAM (accessed by the data bus), which means that they need to be accessed by different addresses ranges depending on type, but refer to the same region. More information on the [Espressif TRMs](https://www.espressif.com/en/support/documents/technical-documents?keys=&field_download_document_type_tid%5B%5D=963).*

---

The following diagrams illustrate a memory organization from the bootloader point of view (notice that the addresses and sizes may vary depending on the chip), they reflect the linker script `boot/espressif/port/<TARGET>/ld/bootloader.ld`:

### ESP32

#### ESP32 standard
```
  SRAM0
                                     IRAM ADDR  / DRAM ADDR
 *  +--------+--------------+------+ 0x40070000 / --------- - SRAM0 START
 *  |        ^                    |
 *  |        | PRO CPU Cache      |  *NOT CLAIMABLE BY OS RAM
 *  |        v                    |
 *  +--------+--------------+------+ 0x40078000 / ----------
 *  |        ^                    |
 *  |        |                    |  *NOT CLAIMABLE BY OS RAM
 *  |        | iram_loader_seg    |  *Region usable as iram_loader_seg during boot
 *  |        | (APP CPU Cache)    |   as APP CPU is not initialized yet
 *  |        |                    |
 *  |        v                    |
 *  +--------+--------------+------+ 0x40080000 / ----------
 *  |        ^                    |
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  |        v                    |
 *  +------------------------------+ 0x40090000 / ----------
 *  |        ^                    |
 *  |        | iram_seg           |  *CLAIMABLE BY OS RAM
 *  |        |                    |
 *  |        v                    |
 *  +--------+--------------+------+ 0x40099000 / ----------
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  +------------------------------+ 0x4009FFFF / ---------- - SRAM0 END

  SRAM1
                                     IRAM ADDR  / DRAM ADDR
 *  +------------------------------+ 0x400A0000 / 0x3FFFFFFF - SRAM1 START
 *  |        ^                    |
 *  |        |                    |  *** SHOULD NOT BE OVERLAPPED ***
 *  |        | dram_seg           |  *** OS CAN RECLAIM IT AFTER BOOT LATER AS HEAP ***
 *  |        v                    |
 *  +--------+--------------+------+ 0x400AB900 / 0x3FFF4700
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  |        |                    |
 *  |        |                    |
 *  |        v                    |
 *  +--------+--------------+------+ 0x400BFFFF / 0x3FFE0000 - SRAM1 END
 Note: On ESP32 the SRAM1 addresses are accessed in reverse order comparing Instruction bus (IRAM) and Data bus (DRAM), but refer to the same location. See the TRM for more information.

  SRAM2
                                     IRAM ADDR  / DRAM ADDR
 *  +--------+--------------+------+ ---------- / 0x3FFAE000 - SRAM2 START
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  +--------+--------------+------+ ---------- / 0x3FFDFFFF - SRAM2 END
```

#### ESP32 Multi Processor Boot

This is the linker script mapping when the `CONFIG_ESP_MULTI_PROCESSOR_BOOT` is enabled ([Multi boot](#multi-boot)) since APP CPU Cache region cannot be used for `iram_loader_seg` region as there would be conflict when the bootloader starts the APP CPU before jump to the main application.

```
  SRAM0
                                     IRAM ADDR  / DRAM ADDR
 *  +--------+--------------+------+ 0x40070000 / --------- - SRAM0 START
 *  |        ^                    |
 *  |        |                    |
 *  |        | Cache              |  *Used by PRO CPU and APP CPU as Cache
 *  |        |                    |
 *  |        v                    |
 *  +--------+--------------+------+ 0x40080000 / ----------
 *  |        ^                    |
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  |        v                    |
 *  +------------------------------+ 0x40090000 / ----------
 *  |        ^                    |
 *  |        | iram_seg           |  *CLAIMABLE BY OS RAM
 *  |        |                    |
 *  |        v                    |
 *  +--------+--------------+------+ 0x40099000 / ----------
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  +------------------------------+ 0x4009FFFF / ---------- - SRAM0 END

  SRAM1
                                     IRAM ADDR  / DRAM ADDR
 *  +------------------------------+ 0x400A0000 / 0x3FFFFFFF - SRAM1 START
 *  |        ^                    |
 *  |        |                    |  *** SHOULD NOT BE OVERLAPPED ***
 *  |        | dram_seg           |  *** OS CAN RECLAIM IT AFTER BOOT LATER AS HEAP ***
 *  |        v                    |
 *  +--------+--------------+------+ 0x400AB900 / 0x3FFF4700
 *  |        ^                    |
 *  |        |                    |  *** SHOULD NOT BE OVERLAPPED ***
 *  |        | iram_loader_seg    |  *** OS CAN RECLAIM IT AFTER BOOT LATER AS HEAP ***
 *  |        |                    |
 *  |        v                    |
 *  +------------------------------+ 0x400B1E00 / 0x3FFEE200
 *  |        ^                    |
 *  |        |                    |
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  |        |                    |
 *  |        v                    |
 *  +--------+--------------+------+ 0x400BFFFF / 0x3FFE0000 - SRAM1 END
 Note: On ESP32 the SRAM1 addresses are accessed in reverse order comparing Instruction bus (IRAM) and Data bus (DRAM), but refer to the same location. See the TRM for more information.

  SRAM2
                                     IRAM ADDR  / DRAM ADDR
 *  +--------+--------------+------+ ---------- / 0x3FFAE000 - SRAM2 START
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  +--------+--------------+------+ ---------- / 0x3FFDFFFF - SRAM2 END
```

### ESP32-S2

```
  SRAM0
                                     IRAM ADDR  / DRAM ADDR
 *  +--------+--------------+------+ 0x40020000 / 0x3FFB0000 - SRAM0 START
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  +--------+--------------+------+ 0x40027FFF / 0x3FFB7FFF - SRAM0 END

  SRAM1
                                     IRAM ADDR  / DRAM ADDR
 *  +--------+--------------+------+ 0x40028000 / 0x3FFB8000 - SRAM1 START
 *  |        ^                    |
 *  |        |                    |
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  |        |                    |
 *  |        v                    |
 *  +--------+--------------+------+ 0x40047000 / 0x3FFD7000
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | iram_seg           |  *CLAIMABLE BY OS RAM
 *  |        |                    |
 *  |        |                    |
 *  |        v                    |
 *  +------------------------------+ 0x40050000 / 0x3FFE0000
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | iram_loader_seg    |  *** SHOULD NOT BE OVERLAPPED ***
 *  |        |                    |  *** OS CAN RECLAIM IT AFTER BOOT LATER AS HEAP ***
 *  |        |                    |
 *  |        v                    |
 *  +------------------------------+ 0x40056000 / 0x3FFE6000
 *  |        ^                    |
 *  |        |                    |
 *  |        | dram_seg           |  *** SHOULD NOT BE OVERLAPPED ***
 *  |        |                    |  *** OS CAN RECLAIM IT AFTER BOOT LATER AS HEAP ***
 *  |        v                    |
 *  +--------+--------------+------+ 0x4006FFFF / 0x3FFFFFFF - SRAM1 END
```

### ESP32-S3

```
  SRAM0
                                     IRAM ADDR  / DRAM ADDR
 *  +--------+--------------+------+ 0x40370000 / ---------- - SRAM0 START
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  +--------+--------------+------+ 0x40377FFF / ---------- - SRAM0 END

  SRAM1
                                     IRAM ADDR  / DRAM ADDR
 *  +--------+--------------+------+ 0x40378000 / 0x3FC88000 - SRAM1 START
 *  |        ^                    |
 *  |        |                    |
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  |        |                    |
 *  |        v                    |
 *  +--------+--------------+------+ 0x403B0000 / 0x3FCC0000
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | iram_seg           |  *CLAIMABLE BY OS RAM
 *  |        |                    |
 *  |        |                    |
 *  |        v                    |
 *  +------------------------------+ 0x403BA000 / 0x3FCCA000
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | iram_loader_seg    |  *** SHOULD NOT BE OVERLAPPED ***
 *  |        |                    |  *** OS CAN RECLAIM IT AFTER BOOT LATER AS HEAP ***
 *  |        |                    |
 *  |        v                    |
 *  +------------------------------+ 0x403C0000 / 0x3FCD0000
 *  |        ^                    |
 *  |        |                    |
 *  |        | dram_seg           |  *** SHOULD NOT BE OVERLAPPED ***
 *  |        |                    |  *** OS CAN RECLAIM IT AFTER BOOT LATER AS HEAP ***
 *  |        v                    |
 *  +--------+--------------+------+ 0x403DFFFF / 0x3FCEFFFF - SRAM1 END

  SRAM2
                                     IRAM ADDR  / DRAM ADDR
 *  +--------+--------------+------+ ---------- / 0x3FCF0000 - SRAM2 START
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  +--------+--------------+------+ ---------- / 0x3FCFFFFF - SRAM2 END
```

### ESP32-C2

```
  SRAM0
                                     IRAM ADDR  / DRAM ADDR
 *  +--------+--------------+------+ 0x4037C000 / ---------- - SRAM0 START
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  +--------+--------------+------+ 0x4037FFFF / ---------- - SRAM0 END

  SRAM1
                                     IRAM ADDR  / DRAM ADDR
 *  +--------+--------------+------+ 0x40380000 / 0x3FCA0000 - SRAM1 START
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  |        |                    |
 *  |        |                    |
 *  |        v                    |
 *  +--------+--------------+------+ 0x403A1370 / 0x3FCC1370
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | iram_seg           |  *CLAIMABLE BY OS RAM
 *  |        |                    |
 *  |        |                    |
 *  |        v                    |
 *  +------------------------------+ 0x403A9B70 / 0x3FCC9B70
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | iram_loader_seg    |  *** SHOULD NOT BE OVERLAPPED ***
 *  |        |                    |  *** OS CAN RECLAIM IT AFTER BOOT LATER AS HEAP ***
 *  |        |                    |
 *  |        v                    |
 *  +------------------------------+ 0x403B0B70 / 0x3FCD0B70
 *  |        ^                    |
 *  |        |                    |
 *  |        | dram_seg           |  *** SHOULD NOT BE OVERLAPPED ***
 *  |        |                    |  *** OS CAN RECLAIM IT AFTER BOOT LATER AS HEAP ***
 *  |        v                    |
 *  +--------+--------------+------+ 0x403BFFFF / 0x3FCDFFFF - SRAM1 END
```

### ESP32-C3

```
  SRAM0
                                     IRAM ADDR  / DRAM ADDR
 *  +--------+--------------+------+ 0x4037C000 / ---------- - SRAM0 START
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  +--------+--------------+------+ 0x4037FFFF / ---------- - SRAM0 END

  SRAM1
                                     IRAM ADDR  / DRAM ADDR
 *  +--------+--------------+------+ 0x40380000 / 0x3FC80000 - SRAM1 START
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  |        |                    |
 *  |        |                    |
 *  |        v                    |
 *  +--------+--------------+------+ 0x403C7000 / 0x3FCC7000
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | iram_seg           |  *CLAIMABLE BY OS RAM
 *  |        |                    |
 *  |        |                    |
 *  |        v                    |
 *  +------------------------------+ 0x403D0000 / 0x3FCD0000
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | iram_loader_seg    |  *** SHOULD NOT BE OVERLAPPED ***
 *  |        |                    |  *** OS CAN RECLAIM IT AFTER BOOT LATER AS HEAP ***
 *  |        |                    |
 *  |        v                    |
 *  +------------------------------+ 0x403D5000 / 0x3FCD5000
 *  |        ^                    |
 *  |        |                    |
 *  |        | dram_seg           |  *** SHOULD NOT BE OVERLAPPED ***
 *  |        |                    |  *** OS CAN RECLAIM IT AFTER BOOT LATER AS HEAP ***
 *  |        v                    |
 *  +--------+--------------+------+ 0x403DFFFF / 0x3FCDFFFF - SRAM1 END
```

### ESP32-C6

```
                                     IRAM ADDR  / DRAM ADDR
 *  +--------+--------------+------+ 0x40800000 / 0x40800000 - HP SRAM START
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  |        |                    |
 *  |        |                    |
 *  |        v                    |
 *  +--------+--------------+------+ 0x40860610 / 0x40860610
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | iram_seg           |  *CLAIMABLE BY OS RAM
 *  |        |                    |
 *  |        |                    |
 *  |        v                    |
 *  +------------------------------+ 0x40869610 / 0x40869610
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | iram_loader_seg    |  *** SHOULD NOT BE OVERLAPPED ***
 *  |        |                    |  *** OS CAN RECLAIM IT AFTER BOOT LATER AS HEAP ***
 *  |        |                    |
 *  |        v                    |
 *  +------------------------------+ 0x40870610 / 0x40870610
 *  |        ^                    |
 *  |        |                    |
 *  |        | dram_seg           |  *** SHOULD NOT BE OVERLAPPED ***
 *  |        |                    |  *** OS CAN RECLAIM IT AFTER BOOT LATER AS HEAP ***
 *  |        v                    |
 *  +--------+--------------+------+ 0x4087FFFF / 0x4087FFFF - HP SRAM END
```

### ESP32-H2

```
                                     IRAM ADDR  / DRAM ADDR
 *  +--------+--------------+------+ 0x40800000 / 0x40800000 - HP SRAM START
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | FREE               |  *CLAIMABLE BY OS RAM
 *  |        |                    |
 *  |        |                    |
 *  |        v                    |
 *  +--------+--------------+------+ 0x408317D0 / 0x408317D0
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | iram_seg           |  *CLAIMABLE BY OS RAM
 *  |        |                    |
 *  |        |                    |
 *  |        v                    |
 *  +------------------------------+ 0x40839FD0 / 0x40839FD0
 *  |        ^                    |
 *  |        |                    |
 *  |        |                    |
 *  |        | iram_loader_seg    |  *** SHOULD NOT BE OVERLAPPED ***
 *  |        |                    |  *** OS CAN RECLAIM IT AFTER BOOT LATER AS HEAP ***
 *  |        |                    |
 *  |        v                    |
 *  +------------------------------+ 0x40840FD0 / 0x40840FD0
 *  |        ^                    |
 *  |        |                    |
 *  |        | dram_seg           |  *** SHOULD NOT BE OVERLAPPED ***
 *  |        |                    |  *** OS CAN RECLAIM IT AFTER BOOT LATER AS HEAP ***
 *  |        v                    |
 *  +--------+--------------+------+ 0x4084FFFF / 0x4084FFFF - HP SRAM END
```
