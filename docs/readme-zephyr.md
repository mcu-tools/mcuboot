# Building and using MCUboot with Zephyr

MCUboot began its life as the bootloader for Mynewt.  It has since
acquired the ability to be used as a bootloader for Zephyr as well.
There are some pretty significant differences in how apps are built
for Zephyr, and these are documented here.

Please see the [design document]({% link design.md %}) for documentation on the
design and operation of the bootloader itself.  This functionality
should be the same on all supported RTOSs.

The first step required for Zephyr is making sure your board has flash
partitions defined in its device tree. These partitions are:

- `boot_partition`: for MCUboot itself
- `slot0_partition`: the primary image slot
- `slot1_partition`: the secondary image slot
- `scratch_partition`: the scratch slot

Currently, the two image slots must be contiguous. If you are running
MCUboot as your stage 1 bootloader, `boot_partition` must be configured
so your SoC runs it out of reset.

The flash partitions are typically defined in the Zephyr boards folder, in a
file named `boards/<arch>/<board>/<board>.dts`. An example `.dts` file with
flash partitions defined is the frdm_k64f's in
`boards/arm/frdm_k64f/frdm_k64f.dts`. Make sure the labels in your board's
`.dts` file match the ones used there.

## Building the bootloader itself

The bootloader is an ordinary Zephyr application, at least from
Zephyr's point of view.  There is a bit of configuration that needs to
be made before building it.  Most of this can be done as documented in
the `CMakeLists.txt` file in boot/zephyr.  There are comments there for
guidance.  It is important to select a signature algorithm, and decide
if slot0 should be validated on every boot.

To build MCUboot, create a build directory in boot/zephyr, and build
it as usual:

```
  cd boot/zephyr
  mkdir build && cd build
  cmake -GNinja -DBOARD=<board> ..
  ninja
```

In addition to the partitions defined in DTS, some additional
information about the flash layout is currently required to build
MCUboot itself. All the needed configuration is collected in
`boot/zephyr/include/target.h`. Depending on the board, this information
may come from board-specific headers, Device Tree, or be configured by
MCUboot on a per-SoC family basis.

After building the bootloader, the binaries should reside in
`build/zephyr/zephyr.{bin,hex,elf}`, where `build` is the build
directory you chose when running `cmake`. Use the Zephyr build
system `flash` target to flash these binaries, usually by running
`make flash` (or `ninja flash`, etc.) from the build directory.

## Building Applications for the bootloader

In addition to flash partitions in DTS, some additional configuration
is required to build applications for MCUboot.

The directory `samples/zephyr/hello-world` in the MCUboot tree contains
a simple application with everything you need. You can try it on your
board and then just make a copy of it to get started on your own
application; see samples/zephyr/README.md for a tutorial.

More details:

- `CONFIG_TEXT_SECTION_OFFSET` must be set to allow room for the
  boot image header. Typically this is set in the app's prj.conf.  It
  must also be aligned to a boundary that the particular MCU requires
  the vector table to be aligned on.

- Your board must provide a DTS `zephyr,code-partition` chosen node
  which ensures it is built and linked into the DTS `slot0_partition`. This
  is typically achieved by creating a
  [dts.overlay](https://github.com/runtimeco/mcuboot/blob/master/samples/zephyr/hello-world/dts.overlay)
  file that contains the chose node description:

```
         chosen {
                zephyr,code-partition = &slot0_partition;
         };
```

With this, build the application as your normally would.

### Signing the application

In order to upgrade to an image (or even boot it, if
`MCUBOOT_VALIDATE_SLOT0` is enabled), the images must be signed.
To make development easier, MCUboot is distributed with some example
keys.  It is important to stress that these should never be used for
production, since the private key is publicly available in this
repository.  See below on how to make your own signatures.

There is a `sign.sh` script that gives some examples of how to make
these signatures.

### Flashing the application

The application itself can flashed with regular flash tools, but will
need to be loaded at the offset of SLOT-0 for this particular target.
These images can also be marked for upgrade, and loaded into SLOT-1,
at which point the bootloader should perform an upgrade.  It is up to
the image to mark slot-0 as "image ok" before the next reboot,
otherwise the bootloader will revert the application.

## Managing signing keys

The signing keys used by MCUboot are represented in standard formats,
and can be generated and processed using conventional tools.  However,
the Mynewt project has developed some tools to make this easier, and
the `imgtool` directory contains a small program to use these tools,
as well as some additional tools for generating and extracting public
keys.  If you will be using your own keys, it is recommended to build
this tool following the directions within the directory.

### Generating a new keypair

Generating a keypair with imgtool is a matter of running the keygen
subcommand:

```
    $ imgtool keygen -k mykey.pem -t rsa-2048
```

The argument to `-t` should be the desired key type.  See the
imgtool README.rst for more details on the possible key types.

### Extracting the public key

The generated keypair above contains both the public and the private
key.  It is necessary to extract the public key and insert it into the
bootloader.  The keys live in `boot/zephyr/keys.c`, and can be
extracted using imgtool:

```
    $ imgtool getpub -k mykey.pem
```

This will output the public key as a C array that can be dropped
directly into the `keys.c` file.

Once this is done, this new keypair file (`mykey.pem` in this
example) can be used to sign images.
