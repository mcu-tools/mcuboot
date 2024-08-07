# Runtime chosen image sample application

This sample demonstrates how to use a non flash storage to retrieve the image
being booted. It was tested on a FRDM K64F. Both slots are used to store two
different images. The image to be booted is selected based on a button press.

## Build

Build mcuboot. First, ensure ZEPHYR_SDK_INSTALL_DIR is defined. From the
mcuboot directory, run the following commands:

```
  source <path-to-zephyr>/zephyr-env.sh

  west build -p -b frdm_k64f boot/zephyr/ -- -DBUILD_RUNTIME_SOURCE_SAMPLE=1 \
        -DEXTRA_CONF_FILE="../../samples/runtime-source/zephyr/sample.conf"
        -DEXTRA_DTC_OVERLAY_FILE=../../samples/runtime-source/zephyr/boards/frdm_k64f.overlay

  west build -t flash
```

Then, build the sample application to be loaded. We need to build it twice, one
for each slot. From the sample
app directory (mcuboot/samples/non-flash-source/zephyr/app), run:

```
  west build -p -b frdm_k64f .
  west flash
```

Then change the overlay file to use the second slot. For instance, open
`boards/frdm_k64f.overlay` and change the line:

```
  zephyr,code-partition = &slot0_partition;

```

to:

```
    zephyr,code-partition = &slot1_partition;
```

And build and flash again:

```
  west build -b frdm_k64f .
  west flash
```

## Run

Open a serial terminal to see the output and reset the board. It shall boot the
image on slot0 by default. By keeping the SW2 button pressed during reset, the
bootloader will randomly select the image to be booted.
