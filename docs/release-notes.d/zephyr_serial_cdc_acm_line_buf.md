- Fixed data loss in the Zephyr serial recovery CDC ACM receive path:
  when a read batch contained more newline-terminated lines than free
  line buffers, the unconsumed tail was dropped. The tail is now kept
  pending and backpressure is applied until a buffer is recycled.
