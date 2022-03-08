# [Building and using MCUboot with Espressif's chips](#building-and-using-mcuboot-with-espressifs-chips)

The Espressif port is build on top of ESP-IDF HAL, therefore it is required in order to build MCUboot for Espressif SoCs.

Documentation about the MCUboot bootloader design, operation and features can be found in the [design document](design.md).

## [SoC support availability](#soc-support-availability)

The current port is available for use in the following SoCs within the OSes:

| | ESP32 | ESP32-S2 | ESP32-C3 | ESP32-S3 |
| :-----: | :-----: | :-----: | :-----: | :-----: |
| Zephyr | Supported | Supported | Supported | WIP |
| NuttX | Supported | Supported | Supported | WIP |

## [Installing requirements and dependencies](#installing-requirements-and-dependencies)

1. Install additional packages required for development with MCUboot:

```
  cd ~/mcuboot  # or to your directory where MCUboot is cloned
  pip3 install --user -r scripts/requirements.txt
```

2. Update the submodules needed by the Espressif port. This may take a while.

```
git submodule update --init --recursive --checkout boot/espressif/hal/esp-idf
```

3. Next, get the Mbed TLS submodule required by MCUboot.
```
git submodule update --init --recursive ext/mbedtls
```

4. Now we need to install IDF dependencies and set environment variables. This step may take some time:
```
cd boot/espressif/hal/esp-idf
./install.sh
. ./export.sh
cd ../..
```

## [Building the bootloader itself](#building-the-bootloader-itself)

The MCUboot Espressif port bootloader is built using the toolchain and tools provided by ESP-IDF. Additional configuration related to MCUboot features and slot partitioning may be made using the `bootloader.conf`.

---
***Note***

*Replace `<TARGET>` with the target ESP32 family (like `esp32`, `esp32s2` and others).*

---

1. Compile and generate the ELF:

```
cmake -DCMAKE_TOOLCHAIN_FILE=tools/toolchain-<TARGET>.cmake -DMCUBOOT_TARGET=<TARGET> -B build -GNinja
cmake --build build/
```

2. Convert the ELF to the final bootloader image, ready to be flashed:

```
esptool.py --chip <TARGET> elf2image --flash_mode dio --flash_freq 40m --flash_size <FLASH_SIZE> -o build/mcuboot_<TARGET>.bin build/mcuboot_<TARGET>.elf
```

3. Flash MCUboot in your device:

```
esptool.py -p <PORT> -b <BAUD> --before default_reset --after hard_reset --chip <TARGET> write_flash --flash_mode dio --flash_size <FLASH_SIZE> --flash_freq 40m <BOOTLOADER_FLASH_OFFSET> build/mcuboot_<TARGET>.bin
```
---
***Note***
You may adjust the port `<PORT>` (like `/dev/ttyUSB0`) and baud rate `<BAUD>` (like `2000000`) according to the connection to your board.
You can also skip `<PORT>` and `<BAUD>` parameters so that esptool tries to automatically detect it.

*`<FLASH_SIZE>` can be found using the command below:*
```
esptool.py -p <PORT> -b <BAUD> flash_id
```
The output contains device information and its flash size:
```
Detected flash size: 4MB
```


*`<BOOTLOADER_FLASH_OFFSET>` value must follow one of the addresses below:*

| ESP32 | ESP32-S2 | ESP32-C3 | ESP32-S3 |
| :-----: | :-----: | :-----: | :-----: |
| 0x1000 | 0x1000 | 0x0000 | 0x0000 |

---

## [Signing and flashing an application](#signing-and-flashing-an-application)

1. Images can be regularly signed with the `scripts/imgtool.py` script:

```
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

```
esptool.py -p <PORT> -b <BAUD> --before default_reset --after hard_reset --chip <TARGET>  write_flash --flash_mode dio --flash_size <FLASH_SIZE> --flash_freq 40m <SLOT_OFFSET> <SIGNED_BIN>
```

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

```
imgtool.py sign -k <YOUR_SIGNING_KEY.pem> --pad --pad-sig --align 4 -v 0 -H 32 --pad-header -S 0x00100000 <BIN_IN> <BIN_OUT>
```
If signing a Zephyr image, the `--pad-header` is not needed, as it already have the padding for MCUboot header.


## [Secure Boot](#secure-boot)

The Secure Boot implementation is based on [IDF's Secure Boot V2](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/secure-boot-v2.html), is hardware-assisted and RSA based, and has the role for ensuring that only authorized code will be executed on the device. This is done through bootloader signature checking by the ROM bootloader. \
***Note***: ROM bootloader is the First Stage Bootloader, while the Espressif MCUboot port is the Second Stage Bootloader.

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
```
espsecure.py generate_signing_key --version 2 <BOOTLOADER_SIGNING_KEY.pem>
```

Then sign the bootloader image:
```
espsecure.py sign_data --version 2 --keyfile <BOOTLOADER_SIGNING_KEY.pem> -o <BOOTLOADER_BIN_OUT> <BOOTLOADER_BIN_IN>
```

---
:warning: ***ATTENTION***

*Once the bootloader is flashed and the device resets, the **first boot will enable Secure Boot** and the bootloader and key **no longer can be modified**. So **ENSURE** that both bootloader and key are correct and you did not forget anything before flashing.*

---

Flash the bootloader as following, with `--after no_reset` flag, so you can reset the device only when assured:
```
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
```
imgtool.py sign -k <YOUR_SIGNING_KEY.pem> --pad --pad-sig --align 32 --max-align 32 -v 0 -H 32 --pad-header -S <SLOT_SIZE> <BIN_IN> <BIN_OUT>
```

### [Device generated key](#device-generated-key)

First ensure that the application image is able to perform encrypted read and write operations to the SPI Flash.
Flash the bootloader and application normally:

```
esptool.py -p <PORT> -b 2000000 --after no_reset --chip <ESP_CHIP> write_flash --flash_mode dio --flash_size <FLASH_SIZE> --flash_freq 40m <BOOTLOADER_FLASH_OFFSET> <BOOTLOADER_BIN>
```
```
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
```
espsecure.py generate_flash_encryption_key <FLASH_ENCRYPTION_KEY.bin>
```

Burn the key into the device's eFuse (keep a copy on the host), this action can be done **only once**:

---
:warning: ***ATTENTION***

*eFuse emulation in Flash configuration options do not have any effect, so if the key burning command below is used, it will actually burn the physical eFuse.*

---

- ESP32
```
espefuse.py --port PORT burn_key flash_encryption <FLASH_ENCRYPTION_KEY.bin>
```

- ESP32S2, ESP32C3 and ESP32S3
```
espefuse.py --port PORT burn_key BLOCK <FLASH_ENCRYPTION_KEY.bin> <KEYPURPOSE>
```

BLOCK is a free keyblock between BLOCK_KEY0 and BLOCK_KEY5. And KEYPURPOSE is either XTS_AES_128_KEY, XTS_AES_256_KEY_1, XTS_AES_256_KEY_2 (AES XTS 256 is available only in ESP32S2).

Now, similar as the Device generated key, the bootloader and application can be flashed plaintext. The **first boot** will encrypt the flash content using the host key burned in the eFuse instead of generate a new one.

Flashing the bootloader and application:

```
esptool.py -p <PORT> -b 2000000 --after no_reset --chip <ESP_CHIP> write_flash --flash_mode dio --flash_size <FLASH_SIZE> --flash_freq 40m <BOOTLOADER_FLASH_OFFSET> <BOOTLOADER_BIN>
```
```
esptool.py -p <PORT> -b 2000000 --after no_reset --chip <ESP_CHIP> write_flash --flash_mode dio --flash_size <FLASH_SIZE> --flash_freq 40m <PRIMARY_SLOT_FLASH_OFFSET> <APPLICATION_BIN>
```

On the **first boot**, the bootloader will:
1. Encrypt flash in-place including bootloader, image primary/secondary slot and scratch using the written key.
2. Burn eFuse to enable Flash Encryption.
3. Reset system to ensure Flash Encryption cache resets properly.

Encrypting data on the host:
- ESP32
```
espsecure.py encrypt_flash_data --keyfile <FLASH_ENCRYPTION_KEY.bin> --address <FLASH_OFFSET> --output <OUTPUT_DATA> <INPUT_DATA>
```

- ESP32-S2, ESP32-C3 and ESP32-S3
```
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