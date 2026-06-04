- bootutil: added overwrite-only delta DFU support with signed patch images.
- bootutil: delta DFU records include old bytes so interrupted delta applies
  can be restored and retried.
- imgtool: added `--delta-base` and `--delta-block-size` for generating signed
  delta images.
- imgtool: delta images carry the old bytes needed for restore and interruption
  recovery.
