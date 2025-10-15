- Swap using offset now includes the size of the unprotected TLV
  area which was wrongly missing before, this requires extra space
  in the swap status as the data is not part of the image header
