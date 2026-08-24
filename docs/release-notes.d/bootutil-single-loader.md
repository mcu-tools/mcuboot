- The single application slot boot path has moved from the Zephyr and Mynewt
  ports into ``boot/bootutil/src/single_loader.c``, and both ports now build the
  shared copy. The file guards its own body on ``MCUBOOT_SINGLE_APPLICATION_SLOT``
  / ``MCUBOOT_SINGLE_APPLICATION_SLOT_RAM_LOAD``, so it may be listed
  unconditionally in a port's source list. Out-of-tree ports no longer have to
  supply their own ``boot_go()`` to use single-slot mode.
- ``single_loader.c`` now provides ``context_boot_go()`` alongside ``boot_go()``,
  mirroring ``loader.c``, so a caller can supply its own ``boot_loader_state``
  instead of the one owned by the unit.
- Fixed a duplicate definition of ``boot_get_max_app_size()`` between
  ``single_loader.c`` and ``bootutil_misc.c``, which failed to link with
  ``MCUBOOT_SINGLE_APPLICATION_SLOT`` and ``MCUBOOT_SERIAL_IMG_GRP_SLOT_INFO``
  both enabled.
- Fixed an undefined reference to ``boot_get_image_max_sizes()`` with
  ``MCUBOOT_SINGLE_APPLICATION_SLOT`` and ``MCUBOOT_DATA_SHARING`` enabled:
  ``single_loader.c`` guarded the definition on
  ``MCUBOOT_SERIAL_IMG_GRP_SLOT_INFO`` alone, while its declaration and callers
  use ``MCUBOOT_SERIAL_IMG_GRP_SLOT_INFO || MCUBOOT_DATA_SHARING``.
- The single-slot ``boot_go()`` no longer returns without closing the primary
  flash area when ``boot_save_boot_status()`` or ``boot_save_shared_data()``
  fails, and now reports the failure as a FIH value rather than a raw ``int``.
- Added ``BOOTUTIL_CAP_SINGLE_APPLICATION_SLOT``. Single-slot builds previously
  fell through to reporting ``BOOTUTIL_CAP_SWAP_USING_SCRATCH`` from
  ``bootutil_get_caps()``, which claimed an upgrade strategy for a configuration
  that has no second slot.
- Anchored the Mynewt ``pkg.ign_files`` patterns in ``boot/bootutil/pkg.yml``.
  newt compiles these as unanchored regular expressions, so the bare
  ``loader.c`` entry also matched ``bootutil_loader.c`` -- silently dropping it
  from single-slot Mynewt builds -- and would have matched the relocated
  ``single_loader.c`` as well.
- The simulator has gained a ``single-slot`` feature, giving
  ``MCUBOOT_SINGLE_APPLICATION_SLOT`` its first test coverage: booting a valid
  primary slot without touching flash, and refusing to boot one that fails
  validation.
- Added a ``single_slot`` Mynewt CI target, so the single-slot configuration is
  built for at least one port as well as in the simulator.
