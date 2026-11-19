- Serial recovery: the image list command no longer runs a full image
  verify on each slot, and lists any slot whose header has a valid magic,
  as img_mgmt in the application does. An image with a bad signature now
  appears in the list, and the boot path still refuses it. The image set
  command verifies only the slot whose hash matches. Direct-XIP and
  RAM-load keep the verifies that choose the active slot.
