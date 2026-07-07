- Fixed various issues with bootstrap in swap modes whereby it
  could read from beyond the flash area, erase (or not) erase
  sectors, and whereby it would pointlessly copy the entire
  slot instead of just copying the image data.
