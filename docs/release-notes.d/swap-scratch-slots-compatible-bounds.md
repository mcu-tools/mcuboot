- Fixed ``boot_slots_compatible()`` and ``app_max_size()`` in swap using scratch
  reading past the end of the sector table when the primary and secondary slots
  have a different number of sectors. Such slots are now reported as incompatible.
