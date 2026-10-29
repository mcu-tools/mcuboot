- A flash error during a swap no longer goes unnoticed when asserts
  are compiled out. The swap stops before the failed step is recorded
  in the swap status, `boot_go()` returns a failure instead of booting
  the primary slot, and the swap is resumed on the next boot. The same
  applies when the swap state can't be read, or a trailer write fails
  while a swap is set up. Previously all of these were checked only
  with `assert()`, so a release build could record a step as done
  although its data had not been moved, and lose part of both images.
