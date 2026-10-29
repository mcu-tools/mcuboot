- Fixed `boot_copy_image()` (overwrite-only fast and bootstrap modes)
  writing one write unit past the end of an image that ends exactly on
  a sector boundary, into a sector that was not erased. The copy is now
  aborted before erasing anything if the image size cannot be read.
