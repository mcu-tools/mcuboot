- Moved the fault injection hardening code to its own directory.
  Note that fault_injection_hardening.h is now included as "fault_injection_hardening.h"
  so out-of-tree ports and custom hooks need to add boot/fault_injection_hardening/include
  to their include path.
