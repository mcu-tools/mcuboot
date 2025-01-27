- Added a new swap using offset algorithm which is set with
  `MCUBOOT_SWAP_USING_OFFSET`. This algorithm is similar to swap
  using move but avoids moving the sectors in the primary slot
  up by having the update image written in the second sector in
  the update slot, which offers a faster update process and
  requires a smaller swap status area
