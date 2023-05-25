- Fixed an issue with the boot_serial zcbor setup encoder function
  wrongly including the buffer address in the size which caused
  serial recovery to fail on some platforms.
