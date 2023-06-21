# ECDSA signature format

When the ECDSA SECP256R1 (EC256) signature support was added to MCUboot, a
shortcut was taken, and these signatures were padded to make them
always a fixed length. Unfortunately, this padding was done in a way
that is not easily reversible. Some crypto libraries (specifically, Mbed
TLS) are fairly strict about the formatting of the ECDSA signature.
This currently means that the ECDSA SECP224R1 (EC) signature
checking code will fail to boot about 1 out of every 256 images,
because the signature itself will end in a 0x00 byte, and the code
will remove too much data, invalidating the signature.

There are two ways to fix this:

  - Use a reversible padding scheme. This solution requires
    at least one pad byte to always be added (to set the length). This
    padding would be somewhat incompatible across versions (older
    EC256 would work, while newer MCUboot code would reject old
    signatures. The EC code would work reliably only in the new
    combination).

  - Remove the padding entirely. Depending on the tool used, this solution
    requires some rethinking of how TLV generation is implemented so
    that the length does not need to be known until the signature is
    generated. These tools are usually written in higher-level
    languages, so this change should not be difficult.

    However, this will also break compatibility with older versions,
    because images generated with newer tools will not
    work with older versions of MCUboot.

This document proposes a multi-stage approach to give a transition
period:

  1. Add a `--no-pad-sig` argument to the sign command in
     `imgtool.py`.

     Without this argument, the images are padded with the
     existing scheme. With this argument, the ECDSA is encoded
     without any padding. The `--pad-sig` argument is also
     accepted, but it is already the default.

  2. MCUboot will be modified to allow unpadded signatures right away.
     The existing EC256 implementations will still work (with or
     without padding), and the existing EC implementation will be able
     to accept padded and unpadded signatures.

  3. An Mbed TLS implementation of EC256 can be added, but it will require
     the `--no-pad-sig` signature to be able to boot all generated
     images. Without the argument, 3 out of 4 images generated will have
     padding and will be considered invalid.

After one or more MCUboot release cycles and announcements in the
relevant channels, the arguments to `imgtool.py` will change:

  - `--no-pad-sig` will still be accepted but will have no effect.

  - `--pad-sig` will now bring back the old padding behavior.

This will require an update to any scripts that will rely on the default
behavior, but will not specify a specific version of imgtool.

The signature generation in the simulator can be changed at the same
time the boot code begins to accept unpadded signatures. The simulator is
always run out of the same tree as the MCUboot code, so there should
not be any compatibility issues.

## Background

ECDSA signatures are encoded as ASN.1, notably with the signature
itself encoded as follows:

```
    ECDSA-Sig-Value ::= SEQUENCE {
      r  INTEGER,
      s  INTEGER
    }
```

Both `r` and `s` are 256-bit numbers. Because these are
unsigned numbers that are being encoded in ASN.1 as signed values, if
the high bit of the number is set, the DER-encoded representation will
require 33 bytes instead of 32. This means that the length of the
signature will vary by a couple of bytes, depending on whether one or
both of these numbers have the high bit set.

Originally, MCUboot added padding to the entire signature and just
removed any trailing 0 bytes from the data block. This turned out to be fine 255 out of 256
times, each time the last byte of the signature was non-zero, but if the
signature ended in a zero, MCUboot would remove too many bytes and render the
signature invalid.

The correct approach here is to accept that ECDSA signatures are of
variable length, and to make sure that we can handle them as such.
