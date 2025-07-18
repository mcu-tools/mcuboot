# Image tool

The Python program `scripts/imgtool.py` can be used to perform the
operations that are necessary to manage keys and sign images.  Using
this script should be preferred to the manual steps described in
`doc/signed_images.md`.

This program is written for Python3, and has several dependencies on
Python libraries.  These can be installed using 'pip3':

    pip3 install --user -r scripts/requirements.txt

## [Managing keys](#managing-keys)

This tool currently supports rsa-2048, rsa-3072, ecdsa-p256 and ed25519 keys.
You can generate a keypair for one of these types using the 'keygen' command:

    ./scripts/imgtool.py keygen -k filename.pem -t rsa-2048

or use rsa-3072, ecdsa-p256, or ed25519 for the type.  The key type used
should match what MCUboot is configured to verify.

This key file is what is used to sign images, this file should be
protected, and not widely distributed.

You can add the `-p` argument to `keygen`, which will cause it to
prompt for a password.  You will need to enter this password in every
time you use the private key.

## [Incorporating the public key into the code](#incorporating-the-public-key-into-the-code)

There is a development key distributed with MCUboot that can be used
for testing.  Since this private key is widely distributed, it should
never be used for production.  Once you have generated a production
key, as described above, you should replace the public key in the
bootloader with the generated one.

For Zephyr, the keys live in the file `boot/zephyr/keys.c`.  For
mynewt, follow the instructions in `doc/signed_images.md` to generate
the key file.

    ./scripts/imgtool.py getpub -k filename.pem

will extract the public key from the given private key file, and
output it as a C data structure.  You can replace or insert this code
into the key file. However, when the `MCUBOOT_HW_KEY` config option is
enabled, this last step is unnecessary and can be skipped.

## [Signing images](#signing-images)

Image signing takes an image in binary or Intel Hex format intended for the
primary slot and adds a header and trailer that the bootloader is expecting:

    Usage: imgtool sign [OPTIONS] INFILE OUTFILE

      Create a signed or unsigned image

      INFILE and OUTFILE are parsed as Intel HEX if the params have .hex
      extension, otherwise binary format is used

    Options:
      --vector-to-sign [payload|digest]
                                      send to OUTFILE the payload or payloads
                                      digest instead of complied image. These data
                                      can be used for external image signing
      --sha [auto|256|384|512]        selected sha algorithm to use; defaults to
                                      "auto" which is 256 if no cryptographic
                                      signature is used, or default for signature
                                      type
      --sig-out filename              Path to the file to which signature will be
                                      written. The image signature will be encoded
                                      as base64 formatted string
      --pure                          Expected Pure variant of signature; the Pure
                                      variant is expected to be signature done
                                      over an image rather than hash of that
                                      image.
      --fix-sig-pubkey filename       public key relevant to fixed signature
      --fix-sig filename              fixed signature for the image. It will be
                                      used instead of the signature calculated
                                      using the public key
      -k, --key filename
      --public-key-format [hash|full]
                                      In what format to add the public key to the
                                      image manifest: full key or hash of the key.
      --max-align [8|16|32]           Maximum flash alignment. Set if flash
                                      alignment of the primary and secondary slot
                                      differ and any of them is larger than 8.
      --align [1|2|4|8|16|32]         Alignment used by swap update modes.
      -v, --version TEXT              [required]
      -s, --security-counter TEXT     Specify the value of security counter. Use
                                      the `auto` keyword to automatically generate
                                      it from the image version.
      -d, --dependencies TEXT         Add dependence on another image, format:
                                      "(<image_ID>,[<slot:active|primary|secondary>,]
                                      <image_version>), ... "
      --pad-sig                       Add 0-2 bytes of padding to ECDSA signature
                                      (for mcuboot <1.5)
      -H, --header-size INTEGER       [required]
      --pad-header                    Add --header-size zeroed bytes at the
                                      beginning of the image
      -S, --slot-size INTEGER         Size of the slot. If the slots have
                                      different sizes, use the size of the
                                      secondary slot.  [required]
      --pad                           Pad image to --slot-size bytes, adding
                                      trailer magic
      --confirm                       When padding the image, mark it as confirmed
                                      (implies --pad)
      -M, --max-sectors INTEGER       When padding allow for this amount of
                                      sectors (defaults to 128)
      --boot-record sw_type           Create CBOR encoded boot record TLV. The
                                      sw_type represents the role of the software
                                      component (e.g. CoFM for coprocessor
                                      firmware). [max. 12 characters]
      --overwrite-only                Use overwrite-only instead of swap upgrades
      -e, --endian [little|big]       Select little or big endian
      -c, --clear                     Output a non-encrypted image with encryption
                                      capabilities,so it can be installed in the
                                      primary slot, and encrypted when swapped to
                                      the secondary.
      --skip-encryption               Set encryption flags and TLV's without
                                      applying encryption.
      --compression [disabled|lzma2|lzma2armthumb]
                                      Enable image compression using specified
                                      type. Will fall back without image
                                      compression automatically if the compression
                                      increases the image size.
      --encrypt-keylen [128|256]      When encrypting the image using AES, select
                                      a 128 bit or 256 bit key len.
      -E, --encrypt filename          Encrypt image using the provided public key.
                                      (Not supported in direct-xip or ram-load
                                      mode.)
      --save-enctlv                   When upgrading, save encrypted key TLVs
                                      instead of plain keys. Enable when
                                      BOOT_SWAP_SAVE_ENCTLV config option was set.
      -F, --rom-fixed INTEGER         Set flash address the image is built for.
      -L, --load-addr INTEGER         Load address for image when it should run
                                      from RAM.
      -x, --hex-addr INTEGER          Adjust address in hex output file.
      -R, --erased-val [0|0xff]       The value that is read back from erased
                                      flash.
      --custom-tlv [tag] [value]      Custom TLV that will be placed into
                                      protected area. Add "0x" prefix if the value
                                      should be interpreted as an integer,
                                      otherwise it will be interpreted as a
                                      string. Specify the option multiple times to
                                      add multiple TLVs.
      --non-bootable                  Mark the image as non-bootable.
      -h, --help                      Show this message and exit.

The main arguments given are the key file generated above, a version
field to place in the header (1.2.3 for example), the alignment of the
flash device in question, and the header size.

The header size depends on the operating system and the particular
flash device.  For Zephyr, it will be configured as part of the build,
and will be a small power of two.  By default, the Zephyr build system will
already prepended a zeroed header to the image.  If another build system is
in use that does not automatically add this zeroed header, `--pad-header` can
be passed and the `--header-size` will be added by imgtool. If `--pad-header`
is used with an Intel Hex file, `--header-size` bytes will be subtracted from
the load address (in Intel Hex terms, the Extended Linear Address record) to
adjust for the new bytes prepended to the file. The load address of all data
existing in the file should not change.

The `--compression` option enables LZMA compression over payload. Details
about internals of image generated with this option can be found here
[here](./compression_format.md)
This isn't fully supported on the embedded side but can be utilised when
project is built on top of the mcuboot.

The `--slot-size` argument is required and used to check that the firmware
does not overflow into the swap status area (metadata). If swap upgrades are
not being used, `--overwrite-only` can be passed to avoid adding the swap
status area size when calculating overflow.

The optional `--pad` argument will place a trailer on the image that
indicates that the image should be considered an upgrade.  Writing this image
in the secondary slot will then cause the bootloader to upgrade to it.

A dependency can be specified in the following way:
`-d "(image_id, image_version)"`. The `image_id` is the number of the image
which the current image depends on. The `image_version` is the minimum version
of that image to satisfy compliance. For example `-d "(1, 1.2.3+0)"` means this
image depends on Image 1 which version has to be at least 1.2.3+0.

In addition, a dependency can specify the slot as follows:
`-d "(image_id, slot, image_version)"`. The `image_id` is the number of the
image on which the current image depends.
The slot specifies which slots of the image are to be taken into account
(`active`: primary or secondary, `primary`: only primary `secondary`: only
secondary slot). The `image_version` is the minimum version of that image to
fulfill the requirements.
For example `-d "(1, primary, 1.2.3+0)"` means that this image depends on the
primary slot of the Image 1, whose version must be at least 1.2.3+0.

The `--public-key-format` argument can be used to distinguish where the public
key is stored for image authentication. The `hash` option is used by default, in
which case only the hash of the public key is added to the TLV area (the full
public key is incorporated into the bootloader). When the `full` option is used
instead, the TLV area will contain the whole public key and thus the bootloader
can be independent from the key(s). For more information on the additional
requirements of this option, see the [design](design.md) document.
