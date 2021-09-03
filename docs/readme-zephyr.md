# Building and running MCUboot with Zephyr

This page describes Zephyr-specific instructions on how to build and run MCUboot with Zephyr.

For documentation on the design and operation of the bootloader itself, see the [design document](design.md).
The functionalities described there are the same on all supported RTOSes.

## Preliminary notes

First, make sure that your board has the following flash memory partitions defined in its devicetree:

- `boot_partition`: for MCUboot itself
- `image_0_primary_partition`: the primary slot of Image 0
- `image_0_secondary_partition`: the secondary slot of Image 0
- `scratch_partition`: the scratch slot

Currently, the two image slots must be contiguous.
If you are running MCUboot as your stage 1 bootloader, `boot_partition` must be configured so your SoC runs it out of reset.
If there are multiple updateable images, the corresponding primary and secondary partitions must be defined for the rest of the images too (for example `image_1_primary_partition` and `image_1_secondary_partition` for Image 1).

The flash partitions are typically defined in the Zephyr boards folder, in the  `boards/<arch>/<board>/<board>.dts` file.
An example `.dts` file with flash memory partitions defined is the one for the frdm_k64f board, located in `boards/arm/frdm_k64f/frdm_k64f.dts`.
Make sure that the labels in your board's `.dts` file match the ones used there.

## Installing requirements and dependencies

Install the additional packages required for development with MCUboot:

```
  cd ~/mcuboot  # or to your directory where MCUboot is cloned
  pip3 install --user -r scripts/requirements.txt
```

## Building the bootloader itself

From Zephyr's point of view, the bootloader is an ordinary Zephyr application.
Before building the bootloader, you must configure MCUboot as documented in the `CMakeLists.txt` file in the `boot/zephyr` folder.

You must select a signature algorithm, and decide if the primary slot should be validated on every boot.

To build MCUboot, create a `/build` directory in `boot/zephyr`, and then build it:

```
  cd boot/zephyr
  mkdir build && cd build
  cmake -GNinja -DBOARD=<board> ..
  ninja
```

In addition to the partitions defined in DTS, MCUboot requires also additional information about the flash memory layout.
The needed configuration is collected in `boot/zephyr/include/target.h`.
Depending on the board, this information may come from board-specific headers, Devicetree, or be configured by MCUboot on a per-SoC family basis.

After building the bootloader, the binaries will reside in `build/zephyr/zephyr.{bin,hex,elf}`, where `build` is the build directory you chose when running `cmake`.
Use the Zephyr build system `flash` target to program these binaries.
To do so, you can run `make flash` (or `ninja flash`, for example) from the build directory.
Depending on the target and flash tool used, this might erase the entirety of the flash memory (mass erase) or only the sectors where the bootloader resides before programming the bootloader image itself.

## Building applications for the bootloader

In addition to flash partitions in DTS, additional configuration is required to build applications for MCUboot.

This is handled internally by the Zephyr configuration system and is wrapped in the `CONFIG_BOOTLOADER_MCUBOOT` Kconfig variable.
You must enable this variable in the application's `prj.conf` file.

The directory `samples/zephyr/hello-world` in the MCUboot tree contains a sample application.
Try it on your board and make a copy of it to get started on your own application.
See `samples/zephyr/README.md` for a tutorial.

The documentation for the Zephyr [CONFIG_BOOTLOADER_MCUBOOT](http://docs.zephyrproject.org/reference/kconfig/CONFIG_BOOTLOADER_MCUBOOT.html) configuration option provides additional details regarding the changes it makes to the image placement and generation in order for an application to be bootable by MCUboot.

With this, build the application as you normally would.

### Signing the application

In order to upgrade to an image (or even boot it, if `MCUBOOT_VALIDATE_PRIMARY_SLOT` is enabled), the images must be signed.
To make development easier, MCUboot is distributed with some example keys.

You can sign images with your own signatures using the `scripts/imgtool.py` script.
See `samples/zephyr/Makefile` for examples of how to use this script.

---
**Caution**

Never use the example keys for production, since the private key is publicly available in this repository.

---

### Programming the application

You can program the application with regular programming tools.
However, you must program the application at the offset of the primary slot for this particular target.
Depending on the platform and programming tool used, you might need to manually specify a flash memory offset corresponding to the primary slot starting address.
This is usually not relevant for programming tools that use Intel Hex images (.hex) instead of raw binary images (.bin) since the former includes destination address information.

Additionally, you must ensure that the programming tool does not perform a mass erase, erasing the entire flash memory, as a mass erase will delete MCUboot.

These images can also be marked for an upgrade, and loaded into the secondary slot, at which point the bootloader should perform an upgrade.
The image must then mark the primary slot as "image ok" before the next reboot, or the bootloader will revert the application.

## Managing signing keys

The signing keys used by MCUboot are represented in standard formats and can be generated and processed using conventional tools.
However, `scripts/imgtool.py` can generate key pairs in all the supported formats.

See the [imgtool documentation](imgtool.md) for more details.

### Generating a new keypair

To generate a keypair with imgtool, run the tool with the keygen subcommand:

```
    ./scripts/imgtool.py keygen -k mykey.pem -t rsa-2048
```

The argument to `-t` sets the desired key type.

See the [imgtool documentation](imgtool.md) for more details.

### Extracting the public key

The generated keypair contains both the public and the private keys.
You must then extract the public key and insert it into the bootloader.
The keys are located in `boot/zephyr/keys.c`, and can be extracted using imgtool as follows:

```
    ./scripts/imgtool.py getpub -k mykey.pem
```

This will output the public key as a C array that can be dropped directly into the `keys.c` file.

Once done, you can use the new keypair file (`mykey.pem` in this example) to sign images.
