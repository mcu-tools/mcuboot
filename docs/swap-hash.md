<!--
  - SPDX-License-Identifier: Apache-2.0

  - Copyright (c) 2022 Linaro LTD
-->

# Swap Hash

## Introduction

The prior swap algorithms used in MCUboot have relied on a few
assumptions about how flash memory works:
1.  The erase operation sets a large region of flash to a given value
2.  The write operation can be used to set a smaller region of flash to
    a specific value.
3.  Operations that are partially completed leave the flash in a
    undetermined state, with subsequent reads giving potentially
    arbitrary data.
4.  The erase size is large compared to the write size.

Because of 3, these algorithms record every step of their progress in
the status area.  For a swap covering `N` sectors (erase-sized units),
this results in `N*3` writes to the status area.  Each write uses the
minimum write size bytes of the status area.  As the minimum write
size of the flash device becomes larger, the amount of space taken by
this status update increases greatly, becoming prohibitive on devices
with very large writes (for example, a device with 512-byte writes and
512-byte erases would require 3x as much flash for status as is
available for the image, with 512-byte writes and 4096-byte erases,
the space would be nearly as large as the image itself.  Both of these
are sizes used by currently available devices).

One solution to this is to change how status is written.  Instead of a
single status area that can be partially written to, we could use two
status areas, each in its own erasable section, and ping-pong between
them.  This would allow the status values to be packed better, to
individual bytes, or even individual bits (only 2 bits is needed for
each, as it is not 3 individual status values, but actually a
counter).  However, we have now traded space for increased wear in the
status area.  Our `N*3` writes to the status area now become `N*3/2`
erases and writes to the status area.  Especially with the smaller
erase sizes, this can result in a significant amount of wear (with
possibly thousands of erases for each upgrade), bringing the number of
possible upgrades before wearing out the flash into the single digits
on some devices.

To improve this even further, we need some additional insight into
point 3 above.  Although it isn't possible to tell if a flash
operation has completed on a previous run, it is possible to
definitively see that a flash operation has started, at least past a
certain point.  Since the flash operations are performed in a known
sequence, we know definitively that all previous operations have been
completed.  Restart is simply a matter of backing up to the last
operation that can be known to have completed and resuming after that
point.

In addition, we only need to track state for erase and write as a
pair.  A partially completed write still has to be erased again in
order to be written successfully.  From now on, this document will
consider a single erase and write as a "step".

The key insight in how we can make this work is to realize that it is
important that each step change the contents of flash.  If a step were
to consist of erasing and then writing the same data that was already
there, it no longer becomes possible to tell if the operation has
started, or if it has completed.

With this improvement, and the modifications to swap-move described in
the rest of this document, we can reduce the number of status area
writes to 3, allowing us to use the ping-pong approach.

## Terminology

We will use "primary slot" to refer to the first code slot; the one
the code is expected to execute from.

We will use "upgrade slot" to refer to the second code slot; where the
image to be upgraded will be placed.

There are various terms used in flash memory that have some
potentially confusing overlap with how these terms are used in other
devices (notably disks).  Conventionally, a "sector" is used to
describe the erasable size, however there doesn't seem to be a
consistent term used for the minimum write size.

Because the swap operation always works in units at least as large as
the erase size of the device, we will refer to all operations
performed on units of a "page".  The size of the page will be at least
as large as the erase size of the device.  A given configuration may
use a page size as large as 128K, where the primary slot contains as
few as 2 pages, and the upgrade slot a single page.  The smallest
page size in a known device will be 512, for a device that both erases
and writes in 512-byte units.

## Using Hashes

In order to be able to recover using this new algorithm, we need to be
able to tell from the contents of a given page whether a step has been
started on it.  Since, during recovery, the contents won't be known,
we will compute a hash of each page.  These hashes are stored in the
status area (instead of the large block of status data).  The size of
the hash is a tradeoff between the size needed to be stored, and the
robustness of the verification.  A 32-bit hash would give a 2 in
`2^32` chance of an intermediate operation appearing to either be the
data before the step or after it.  This would suggest an upgrade
failure occurring roughly every 2 billion interrupted upgrades.  Given
that interruptions of upgrades themselves are infrequent, this is
likely to be sufficient.  The value is parameterized, however, and can
be increased if deemed necessary.

A 32-bit hash is insufficient to guarantee no collissions across the
two images.  In addition, we have to ensure that none of the
non-erased-appearing pages in the images has a hash collision with the
all-erased page.  This is handled by using a keyed hash function.  The
key is initiall set to a known value, and if during the hash
computation, we detect a collision, the key is changed, and the hash
computations are changed.

Collisions need to be checked between adjacent pages of the image in
slot 0 (the first phase of the swap slides slot 0 down), and then
between the pages in these positions and in the old image.  All-in
all, there are `3*N` possible collisions to check, giving a chance of
`2^32/(3*N)` of there being a collision.  This is rare enough that it
probably won't occur during testing, so some effort will need to go
into the simulator to ensure that collisions are properly detected and
handled.

## New Status Format

Swap-hash uses a new format for the image status area (also referred
to as the image trailer).  There are two variants of this status area,
depending on whether the write size of the device is small or not.

### Small write

For small write devices, where we can write in smaller units, the
trailer is formatted as follows:

```
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ~                                                               ~
    ~    Page hashes (4 * (image-0-size-in-pages +                  ~
    ~                      image-1-size-in-pages))                  ~
    ~                                                               ~
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                 Encryption key 0 (16 octets) [*]              |
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                    0xff padding as needed                     |
    |  (BOOT_MAX_ALIGN minus 16 octets from Encryption key 0) [*]   |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                 Encryption key 1 (16 octets) [*]              |
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                    0xff padding as needed                     |
    |  (BOOT_MAX_ALIGN minus 16 octets from Encryption key 1) [*]   |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                   Image 0 size (4 octets)                     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                   Image 1 size (4 octets)                     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                   Hash key (4 octets)                         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                    0xff padding as needed                     |
    |        (BOOT_MAX_ALIGN minus 4 octets from Swap size)         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   Sliding     |  0xff padding (BOOT_MAX_ALIGN minus 1 octet)  |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   Swapping    |  0xff padding (BOOT_MAX_ALIGN minus 1 octet)  |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   Copy done   |  0xff padding (BOOT_MAX_ALIGN minus 1 octet)  |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   Image OK    |  0xff padding (BOOT_MAX_ALIGN minus 1 octet)  |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                    0xff padding as needed                     |
    |         (BOOT_MAX_ALIGN minus 16 octets from MAGIC)           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                       MAGIC (16 octets)                       |
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

The differences from the status area used in swap-move are:
- Swap-info is replaced with two status value "sliding" and
  "swapping", that indicate the phase we are in.
- There is a field for the hash key, determined as above to eliminate
  collisions in the hash function.
- The size of both images is stored.  This is needed for the algorithm
  described below.
- Instead of a large status area, we store page hashes.

This trailer is placed in the last page of each slot, and can share
that slot with image data.

TODO: Describe where the trailer is written at a given phase.

### Large Write (ping pong)

For devices with large writes (and smaller erases), the status area is
written in two sections:

```
      +--------------------------
      ~   Hash overflow pages   ~
      +--------------------------
      | penultimate status page |
      +--------------------------
      |  ultimate status page   |
      +--------------------------
```

The ultimate and penultimate status pages are of the same format, and
contain a sequence number.  They are written in a ping-pong format,
increasing the sequence number for each write.  Upon recovery, the
page with the *lowest* sequence number, and valid hash, is used (see
below).  This page has the following format.

```
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ~                                                               ~
    ~    Page hashes (4 * (image-0-size-in-pages +                  ~
    ~                      image-1-size-in-pages))                  ~
    ~    maximum to fit in page                                     ~
    ~                                                               ~
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                 Encryption key 0 (16 octets) [*]              |
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                 Encryption key 1 (16 octets) [*]              |
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                   Image 0 size (4 octets)                     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                   Image 1 size (4 octets)                     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                   Hash key (4 octets)                         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                   Sequence number (4 octets)                  |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   Phase       |  0xff padding (3 octets)                      |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                   protection hash (4 octets)                  |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                       MAGIC (16 octets)                       |
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

Differences from the small-write status area:
- The various state flags are reduced to a single enum "Phase"
  - `IMAGE_PHASE_SLIDE`: We have started sliding.
  - `IMAGE_PHASE_SWAP`: We have started swapping.
  - `IMAGE_PHASE_DONE`: The copy has been completed.
  - `IMAGE_PHASE_OK`: The image has also be verified as OK.
- There is a hash before the magic, this is the first 4 bytes of a
  keyed hash of the contents of the page, up until this point), and is
  used to determine if the page is valid.
- The sequence number is increased each time a new page is written.
- The page hashes are stored, starting with image 0, followed by image
  1, until the page is full.  Note that if there are fewer hashes than
  the available space, the hashes start at the *beginning* of the
  status page, and the padding is between those hash and the first
  encryption key.

Status pages in this mode never overlap the image.

Preceeding these two pages are 0 or more additional pages needed to
hold the hashes of the images.  These are encoded as follows:

```
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ~                                                               ~
    ~    Page hashes (4 * (image-0-size-in-pages +                  ~
    ~                      image-1-size-in-pages))                  ~
    ~    maximum to fit in page                                     ~
    ~                                                               ~
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                   protection hash (4 octets)                  |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

The hashes start with image 0 followed by image 1, and are placed in
the last page(s) until it fills, and then in previous pages as needed
(with the earliest overflow page having the last image hashes).

### Updating the large-write status

To initiate an upgrade, the MAGIC value is written to the last page of
the upgrade slot.  From then on, the status area is always written to
the primary slot.

The overflow pages are written to flash first, followed by the
ultimate page, with a phase of `IMAGE_PHASE_SLIDE` to indicate the
start of the slide operation.  The overflow pages are assumed to be
written correctly when the first status page is written with a valid
hash an starting sequence number).

To update the status, assuming `A` is the current status page and `B`
is the alternate one:
- Erase `B`
- Write `B` with new sequence number
- Erase `A`

Because it isn't possible to determine if the `B` status page was
written correctly or just happens to appear that way, we don't use it
until the first page has been erased.  The erase of the first isn't
necessary if power has remained on, and we know that the operation has
completed previously.

## The Work List

For this hash-based recovery to work, we need to build a work list of
the steps needed to perform the upgrade.  This work list is divided
into two phases: the slide phase, and the swap phase.  These have to
be separated by a write to the status area because it is not possible
to distinguish between them, as the operate on the same pages.

The work list starts out as a list of the operations needed to perform
either the slide or the swap.  Each step has a source and a
destination and logically describes the following two steps:
- Erase the destination page
- Copy the data from the source to the destination page

However, any steps that would result in no change to the flash will
need to be eliminated.  This occurs either with adjacent pages in the
primary slot that are identical, or pages between the two images
(either the same, or shifted in the primary slot) that are identical.
These will result in no operation being performed in flash.

There is some additional complexity in regards to handling erased
pages, and how this is handled depends on whether the device
distinguished between erased data, and data that is written as all
bytes of the erased value.

- If the device does not distinguish between erased data and data
  written as all 0xFF (or the appropriate erase value), there are two
  cases:
  - If the previous contents of destination are the erased value, then
    the entire work step is eliminated.
  - Otherwise, the erase is performed, but the write is not performed.
- If the device does require erased data to be written, the erased
  page is not treated differently from any other data.  It is
  important that ecc checking be enabled when performing recovery.

## Performing the swap

The swap is performed through the following steps

- Set the hash key to 1.
- retry: Compute the hashes of all of the pages in both images
- Compute the two work lists.  If during this computation, we find a
  collision (same hash, but different page data), increment the hash
  key, and goto retry.
- Write any hash overflow pages to the primary slot status area
- Write the status area data (one page, or the last page if a
  large-write device).  Set the status to indicate sliding
- Perform the steps of the "slide" operation, in order
- Update the status to indicate swapping
- Perform the steps of the "swap" operation, in order
- Update the status to indicate done

If, on startup, we discover that the status area is written to either
sliding or swapping, we have to perform a recovery:

- Read the hash from the status area
- Compute the work list, using the same algorithm as before to
  eliminte pages (however, don't perform collision detection, assume
  the hash key hash been properly computed, and that hash collision do
  indicate identical data)
- Scan the appropriate worklist computing page hashes and comparing
  with those retrieved.  Stop at the first page that doesn't appear to
  have been started.
- Unless we are on the first work item, back up one step.
- Perform this step and the remaining items on the work list.
- If sliding, write "swapping" to status, and perform the swap steps,
  as above
- Otherwise, if swapping, write the done status
