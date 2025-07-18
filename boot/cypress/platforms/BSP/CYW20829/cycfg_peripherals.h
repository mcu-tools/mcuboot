/*******************************************************************************
* File Name: cycfg_peripherals.h
*
* Description:
* Peripheral Hardware Block configuration
* This file was automatically generated and should not be modified.
* Configurator Backend 3.10.0
* device-db 4.100.0.4783
* mtb-pdl-cat1 3.9.0.29592
*
********************************************************************************
* Copyright 2025 Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
********************************************************************************/

#if !defined(CYCFG_PERIPHERALS_H)
#define CYCFG_PERIPHERALS_H

#include "cycfg_notices.h"
#include "cy_adcmic.h"
#if defined (CY_USING_HAL)
    #include "cyhal_hwmgr.h"
#endif //defined (CY_USING_HAL)

#if defined(__cplusplus)
extern "C" {
#endif

#define adcmic_0_ENABLED 1U
#define adcmic_0_HW MXS40ADCMIC0
#define adcmic_0_IRQ adcmic_interrupt_adcmic_IRQn
#define adcmic_0_FIFO_DATA_REG_PTR CY_ADCMIC_FIFO_DATA_REG_PTR(MXS40ADCMIC0)
#define adcmic_0_TRIGGER_CLR_REG_PTR CY_ADCMIC_TRIGGER_CLR_REG_PTR(MXS40ADCMIC0)

extern cy_stc_adcmic_context_t adcmic_0_context;
extern const cy_stc_adcmic_dc_config_t adcmic_0_dc_config;
extern const cy_stc_adcmic_config_t adcmic_0_config;
#if defined (CY_USING_HAL)
    extern const cyhal_resource_inst_t adcmic_0_obj;
#endif //defined (CY_USING_HAL)

void reserve_cycfg_peripherals(void);

#if defined(__cplusplus)
}
#endif


#endif /* CYCFG_PERIPHERALS_H */
