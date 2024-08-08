# Non-flash storage sample application

This sample demonstrates how to use a non flash storage to retrieve the image
being booted. It was tested on an MEC17 EVB. The image is provided via I2C and
an Aardvark I2C/SPI Host Adapter. A script, using Aardvark API, is used to
provide the image.

## Set up

Set up the MEC17 EVB. The instructions here expect that MEC17 serial, dediprog
and Aardvark are connected to the same host, in which things are built. Adjust
accordingly to different topologies. Refer to
https://docs.zephyrproject.org/latest/boards/microchip/mec172xevb_assy6906/doc/index.html#programming-and-debugging to have the board set up, with serial output.

Connect the Aardvark to the MEC17 EVB I2C pins. The Aardvark should be
connected to the host machine.

To use the Aardvark script, you need to have the Aardvark API installed. Refer
to https://www.totalphase.com/products/aardvark-i2cspi/ for details. The library
(aardvark.so) and Python bindings (aardvark.py) should be on the available (usually,
copied to the scripts directory, alongside the aardvark_i2c_image.py).

## Build

Build mcuboot. First, ensure ZEPHYR_SDK_INSTALL_DIR is defined, as well that
MEC17 image generator is on path. Refer to https://docs.zephyrproject.org/latest/boards/microchip/mec172xevb_assy6906/doc/index.html#setup for details. From the mcuboot
directory, run the following commands:

```
  source <path-to-zephyr>/zephyr-env.sh

  west build -p -b mec172xevb_assy6906 boot/zephyr/ \
         -- -DEXTRA_DTC_OVERLAY_FILE=../../samples/non-flash-source/zephyr/boards/mec172xevb_assy6906.overlay \
         -DEXTRA_CONF_FILE="../../samples/non-flash-source/zephyr/boards/mec172xevb_assy6906.conf;../../samples/non-flash-source/zephyr/sample.conf"

  west build -t flash
```

Then, build the sample application to be loaded via I2C. From the sample
app directory (mcuboot/samples/non-flash-source/zephyr/app), run:

```
  west build -p -b mec172xevb_assy6906 .
```

## Run

Run the script to provide the image. From the sample app directory, run:

```
  python3 scripts/aardvark_i2c_image.py 0x20 build/zephyr/zephyr.signed.bin
```

Now, when you reset the MEC17 EVB, the image will be loaded via I2C. You shall
see something like the following in the serial output:

```
  *** Booting MCUboot v2.1.0-67-g9c2b470ca027 ***
  *** Using Zephyr OS build v3.7.0-715-geeedb29f7e1a ***
  I: Starting bootloader
  I: Image 0 RAM loading to 0xd0000 is succeeded.
  I: Bootloader chainload address offset: 0xd0000
  I: Jumping to the first image slot
  *** Booting Zephyr OS build v3.7.0-715-geeedb29f7e1a ***
  Hello World from Zephyr on mec172xevb_assy6906!
```
