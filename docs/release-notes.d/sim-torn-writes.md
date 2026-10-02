- Added a torn-write mode to the simulator's power-loss testing: the
  interrupted flash write can now be left partially programmed instead
  of always aborting atomically, and a new revert sweep uses it to
  cover interrupted-write states such as a partially written trailer
  magic.
