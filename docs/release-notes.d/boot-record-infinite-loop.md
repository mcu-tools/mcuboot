- Fixed a possible infinite loop in ``boot_save_shared_data()`` (``boot_record.c``) when
  ``MCUBOOT_DATA_SHARING_BOOTINFO`` and hardware rollback protection are enabled: iterating
  boot-info security counters could spin forever if ``boot_nv_security_counter_get`` failed for
  an image index (for example, backends that expose only one NV counter while
  ``BOOT_IMAGE_NUMBER`` is greater than one).
