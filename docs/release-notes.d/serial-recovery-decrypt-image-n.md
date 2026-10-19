- Fixed serial recovery not decrypting an encrypted image uploaded into the
  primary slot of image 1 or higher (multi-image builds): it stayed
  ciphertext, so no image booted. Uploads into the primary slot of any image
  are now decrypted in place, like those into the primary slot of image 0.
