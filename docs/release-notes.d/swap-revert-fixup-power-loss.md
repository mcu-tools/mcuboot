- Fixed reverts with swap-using-move and swap-using-offset that could
  be lost after two power losses, when the first one tore the trailer
  magic written while starting the revert.  With swap-using-offset, a
  revert resumed after its first step could also leave the device
  unbootable after a second power loss.
