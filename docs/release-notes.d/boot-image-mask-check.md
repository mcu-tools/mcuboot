- Fault injection hardening: with multiple images and the MEDIUM or HIGH
  fault injection hardening profile, boot_go_for_image_id() now checks that
  the image mask is unchanged after the boot process, and fails if it was
  altered.
