- Fixed serial recovery with `MCUBOOT_ERASE_PROGRESSIVELY` erasing the end of
  an uploaded image: when the image reached the last sector of the slot, that
  sector was erased again after the last chunk (to clear the image trailer),
  although the upload had already erased it before writing there. The upload
  reported success, the image failed validation.
