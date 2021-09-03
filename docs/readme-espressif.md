# Building and using MCUboot with Espressif's chips

The Espressif port is build on top of ESP-IDF HAL, therefore it is required in order to build MCUboot for Espressif SoCs.

Documentation about the MCUboot bootloader design, operation and features can be found in the [design document](design.md).

## SoC support availability

The current port is available for use in the following SoCs within the OSes:
- ESP32
    - Zephyr RTOS - _WIP_
    - NuttX
- ESP32-S2
    - Zephyr RTOS - _WIP_
    - NuttX - _WIP_

## Installing requirements and dependencies

1. Install additional packages required for development with MCUboot:

```
  cd ~/mcuboot  # or to your directory where MCUboot is cloned
  pip3 install --user -r scripts/requirements.txt
```

2. Update the submodules needed by the Espressif port. This may take a while.

```
git submodule update --init --recursive --checkout boot/espressif/hal/esp-idf
```

3. Next, get the mbedtls submodule required by MCUboot.

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

## Building the bootloader itself

The MCUboot Espressif port bootloader is built using the toolchain and tools provided by ESP-IDF. Additional configuration related to MCUboot features and slot partitioning may be made using the `bootloader.conf`.

---
***Note*** 

*Replace `<target>` with the target ESP32 family (like `esp32`, `esp32s2` and others).*

---

1. Compile and generate the ELF:

```
cmake -DCMAKE_TOOLCHAIN_FILE=tools/toolchain-<target>.cmake -DMCUBOOT_TARGET=<target> -B build -GNinja
cmake --build build/
```

2. Convert the ELF to the final bootloader image, ready to be flashed:

```
esptool.py --chip <target> elf2image --flash_mode dio --flash_freq 40m -o build/mcuboot_<target>.bin build/mcuboot_<target>.elf
```

3. Flash MCUboot in your board:

```
esptool.py -p <PORT> -b <BAUD> --before default_reset --after hard_reset --chip <target> write_flash --flash_mode dio --flash_size detect --flash_freq 40m 0x1000 build/mcuboot_<target>.bin
```

You may adjust the port `<PORT>` (like `/dev/ttyUSB0`) and baud rate `<BAUD>` (like `2000000`) according to the connection with your board.
