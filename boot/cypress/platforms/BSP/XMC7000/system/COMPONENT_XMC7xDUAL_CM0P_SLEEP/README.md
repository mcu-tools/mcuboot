# XMC7xDUAL Cortex M0+ DeepSleep prebuilt image (XMC7xDUAL_CM0P_SLEEP)

### Overview
DeepSleep prebuilt application image is executed on the Cortex M0+ core of the XMC dual CM7-core MCU (CM0+, CM7_0 and CM7_1).
The image is provided as C array ready to be compiled as part of the Cortex M7_0 application.
The Cortex M0+ application code is placed to internal flash by the Cortex M7_0 linker script.

DeepSleep prebuilt image executes the following steps:
- starts CM7_0 core at CY_CORTEX_M7_0_APPL_ADDR. (check the address in partition.h in pdl repo)
- starts CM7_1 core at CY_CORTEX_M7_1_APPL_ADDR. (check the address in partition.h in pdl repo)
- puts the CM0+ core into Deep Sleep.

Note: After CM7_0 boots up, a delay of 1 second is added before CM7_1 boots up. This is to take care of race condition if both the cores try to configure the same clock.

### Usage

This image is used by default by all Infineon BSPs that target XMC Dual-Core MCU.

To use this image in the custom BSP, adjust the BSP target makefile to
add the COMPONENT_XMC7xDUAL_CM0P_SLEEP directory to the list of components
discovered by ModusToolbox build system:

```
COMPONENTS+=XMC7xDUAL_CM0P_SLEEP
```

Make sure there is a single XMC7xDUAL_CM0P_* component included in the COMPONENTS list.

---
Copyright (c) (2020-2022), Cypress Semiconductor Corporation (an Infineon company) or an affiliate of Cypress Semiconductor Corporation.
