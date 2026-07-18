- Fixed a bug in ``decrypt_image_inplace()`` (serial recovery, encrypted
  image upload) where the return value of ``flash_area_get_sector()`` was
  not checked. On a flash driver that fails this call, the ``sector``
  struct is left unpopulated and its ``fs_size`` field is later used as a
  divisor, which could result in a division by zero. The return value is
  now checked and the function bails out on failure, matching the same
  check already performed in ``bs_upload()``.
