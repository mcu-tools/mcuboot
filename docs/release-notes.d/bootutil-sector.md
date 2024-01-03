- bootutil: Fixed issue with comparing sector sizes for
  compatibility, this now also checks against the number of usable
  sectors (which is the slot size minus the swap status and moved
  up by one sector).
- bootutil: Added debug logging to show write location of swap status
  and details on sectors including if slot sizes are not optimal for
  a given board.
