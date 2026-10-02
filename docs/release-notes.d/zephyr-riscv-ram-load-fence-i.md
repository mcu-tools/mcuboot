- Zephyr: on RISC-V cores with Zifencei, RAM load now executes `fence.i`
  after the image is copied and before jumping to it, so a core with an
  instruction cache cannot run stale lines fetched before the copy.
