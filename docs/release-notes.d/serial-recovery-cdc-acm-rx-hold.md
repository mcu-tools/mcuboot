- Serial recovery over USB CDC ACM no longer drops received bytes when
  every line buffer is in use. Bytes that cannot be stored yet are held,
  and reception is paused until a line buffer is freed, so a client can
  send a request without pausing between lines.
