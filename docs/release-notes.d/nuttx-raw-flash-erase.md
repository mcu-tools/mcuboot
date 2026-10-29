- NuttX port: the flash map backend now gives bootutil raw flash semantics.
  It opens the slots with `O_DIRECT | O_SYNC`, so writes program without the
  FTL erasing first, and `flash_area_erase()` erases with
  `MTDIOC_ERASESECTORS` instead of writing the erased value. Erasing and
  writing a slot is about 20 times faster and erases each sector once
  instead of on every write. The erase range must be whole sectors of the
  area, as bootutil passes it.
