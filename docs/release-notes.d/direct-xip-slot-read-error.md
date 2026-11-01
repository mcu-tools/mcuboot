- Fixed direct-XIP and RAM-load modes failing to boot when reading the image
  header of one slot returns a flash error (for example an ECC error left by
  a power loss during an update). The slot is now treated as having no image
  and the other slot can still be booted.
