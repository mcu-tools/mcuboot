- The Cypress security counter example backend now fails closed. It
  previously reported a fixed counter value and discarded updates while
  reporting success, so `MCUBOOT_HW_ROLLBACK_PROT` builds using it ran with
  a rollback floor that never advanced.
