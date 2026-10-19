- Fixed the serial recovery in-place decryption writing past its key data on
  the stack when it cleaned up, and decrypting an image without its key TLV
  with an unkeyed context; such an image is now left as it was uploaded.
