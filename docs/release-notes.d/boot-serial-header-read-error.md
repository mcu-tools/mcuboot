- Serial recovery: the image list and image set commands now skip a slot
  whose image header cannot be read, instead of acting on a header left
  over from the previous slot or uninitialised.
