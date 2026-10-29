- Fixed the Espressif port with QIO or QOUT flash
  (`CONFIG_ESPTOOLPY_FLASHMODE_QIO`): the bootloader did not link
  (`bootloader_enable_qio_mode` undefined), and its image header asked the ROM
  to load it in quad mode, which the flash only accepts once the bootloader has
  enabled it. The image header now says DIO for QIO and QOUT, as in ESP-IDF.
