# Hpmicro MCU: MCUboot-based basic bootloader

[MCUboot](https://github.com/mcu-tools/mcuboot) is an open-source library enabling the development of secure bootloader applications for 32-bit MCUs. MCUboot is the primary bootloader in popular IoT operating systems such as Zephyr and Apache Mynewt. This example demonstrates using MCUboot with HPMicro MCUs. This example bundles two applications:

- **Bootloader app:** Implements an MCUboot-based basic bootloader application running at the beginning. The bootloader handles image authentication and upgrades. When the image is valid, the bootloader load the image and switch to it.

- **hello_world app:** Implements a simple application. The application toggles the user LED and send different message to uart console. You can build this application in one of the following ways.

   - **BOOT mode:** The application image is built to be programmed into the primary slot. The bootloader will simply boot the application on the next reset.

   - **UPGRADE mode:** The application image is built to be programmed into the secondary slot. Based on user input bootloader will copy the image into the primary slot and boot it on the next reset.
## Hardware setup
This example uses the board's default configuration, you can change the config to fit your own board.

## Software setup
1. See `hpm_sdk`'s `README` for environment setup
2. Setup mcuboot's `README`

## Using the code example
This document expects you to be familiar with MCUboot and its concepts. See [MCUboot documentation](https://github.com/mcu-tools/mcuboot) to learn more.

This example bundles two applications - the bootloader and the hello_world app. You need to build and program the applications in the following order. Do not start building the applications yet: follow the [Step-by-step instructions](#step-by-step-instructions).

1. *Build and program the bootloader* - On the next reset, SOC runs the bootloader and prints a message that no valid image has been found.

2. *Build and program the hello_world app in BOOT mode by setting the cmake variable `build_type` to `bootmode`* - On the next reset, the bootloader will run the hello_world app from the primary slot. This application toggles the user LED and print message `hpmicro hello world app for mcuboot(BOOT MODE)`.

3. *Build and program the hello_world app in UPGRADE mode by setting the cmake variable `build_type` to `upgrade`* - On the next reset, the bootloader will copy this new image from the secondary slot into the primary slot and run the image from the primary slot. The application toggles the user LED and print message `hpmicro hello world app for mcuboot(UPGRADE MODE)`.

### how to download app to flash
It has many ways to download app to flash. Ex: openocd for ft2232, jlink, mcu app.
For board hpm6750evk, it is easy to use onboard ft2232, for more detail, See hpm_sdk's `README`:
1. openocd -f ${path_soc_config} -f ${path_ft2232_config} -f ${path_board_config}
2. start `cmd` console
3. telnet localhost 4444
4. type `reset halt` in telnet
5. type `flash erase_address ${ADDRESS} ${SIZE}` in telnet
6. type `flash write_bank 0 ${PATH_TO_IN_FILE} ${OFFSET}` in telnet
7. exit telnet

a concrete example:
```
- openocd -f D:\hpm_sdk\boards\openocd\probes\ft2232.cfg -f D:\hpm_sdk\boards\openocd\soc\hpm6750-single-core.cfg -f D:\hpm_sdk\boards\openocd\boards\hpm6750evk.cfg
- telnet localhost 4444
- reset
- halt
- flash erase_address 0x80043000 0x180000
- flash write_bank 0 C:/Users/zihan.xu/zephyrproject/bootloader/mcuboot/samples/zephyr/hello-world/signed-hello1-hpm.bin 0x43000
```
### build bootloader
1. setup build env, see hpm_sdk's `README` for detail
2. cd boot/hpmicro/bootloader
3. md build&cd build
4. cmake -GNinja -DBOARD=hpm6750evk -DCMAKE_BUILD_TYPE=flash_xip ..
5. ninja

### build hello_world in bootmode
1. setup build env, see hpm_sdk's `README` for detail
2. cd boot/hpmicro/hello_world
3. md build&cd build
4. cmake -GNinja -DBOARD=hpm6750evk -DCMAKE_BUILD_TYPE=bootmode ..
5. ninja
6. start `cmd` console
7. python ${path to imgtool.py} sign --header-size 0x200 --align 8 --version 1.0 --slot-size 0x180000 ${path to orign bin file} ${path to output file}
8. Flash app to flash. (`ADDRESS=0x80043000`, `SIZE=0x180000`, `OFFSET=0x43000`). See [how to download app to flash].

### build hello_world in upgrade mode
1. setup build env, see hpm_sdk's `README` for detail
2. cd boot/hpmicro/hello_world
3. md build&cd build
4. cmake -GNinja -DBOARD=hpm6750evk -DCMAKE_BUILD_TYPE=upgrade ..
5. ninja
6. start `cmd` console
7. python ${path to imgtool.py} sign --header-size 0x200 --align 8 --version 1.1 --slot-size 0x180000 --pad ${path to orign bin file} ${path to output file}
8. Flash app to flash. (`ADDRESS=0x801C3000`, `SIZE=0x180000`, `OFFSET=0x1C3000`). See [how to download app to flash].