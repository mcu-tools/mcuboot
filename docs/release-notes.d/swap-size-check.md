- When an interrupted swap is resumed, the swap size stored in the
  image trailer is now checked against the slot layout before it is
  used. A size larger than the swap algorithm allows made the
  bootloader index past the end of its sector tables. Such a size is
  now rejected, and the swap is not resumed.
