- Added `MCUBOOT_BOOT_TMPBUF_SZ`, allowing a port to override the size of the
  buffer used to read the image in chunks while its hash is computed. The
  default remains 256 bytes. Raising it reduces the number of `flash_area_read()`
  calls over an image, which is significant on flash that is not memory mapped;
  lowering it reduces the statically allocated buffer.
