/*******************************************************************************
* File Name: cycfg_system.c
*
* Description:
* System configuration
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

#include "cycfg_system.h"

#define CY_CFG_SYSCLK_ECO_ERROR 1
#define CY_CFG_SYSCLK_ALTHF_ERROR 2
#define CY_CFG_SYSCLK_FLL_ERROR 4
#define CY_CFG_SYSCLK_WCO_ERROR 5
#define CY_CFG_SYSCLK_CLKBAK_ENABLED 1
#define CY_CFG_SYSCLK_CLKBAK_SOURCE CY_SYSCLK_BAK_IN_CLKLF
#define CY_CFG_SYSCLK_FLL_ENABLED 1
#define CY_CFG_SYSCLK_FLL_MULT 500U
#define CY_CFG_SYSCLK_FLL_REFDIV 125U
#define CY_CFG_SYSCLK_FLL_CCO_RANGE CY_SYSCLK_FLL_CCO_RANGE4
#define CY_CFG_SYSCLK_FLL_ENABLE_OUTDIV true
#define CY_CFG_SYSCLK_FLL_LOCK_TOLERANCE 10U
#define CY_CFG_SYSCLK_FLL_IGAIN 9U
#define CY_CFG_SYSCLK_FLL_PGAIN 4U
#define CY_CFG_SYSCLK_FLL_SETTLING_COUNT 48U
#define CY_CFG_SYSCLK_FLL_OUTPUT_MODE CY_SYSCLK_FLLPLL_OUTPUT_OUTPUT
#define CY_CFG_SYSCLK_FLL_CCO_FREQ 320U
#define CY_CFG_SYSCLK_FLL_OUT_FREQ 96000000
#define CY_CFG_SYSCLK_CLKHF0_ENABLED 1
#define CY_CFG_SYSCLK_CLKHF0_DIVIDER CY_SYSCLK_CLKHF_NO_DIVIDE
#define CY_CFG_SYSCLK_CLKHF0_FREQ_MHZ 96UL
#define CY_CFG_SYSCLK_CLKHF0_CLKPATH CY_SYSCLK_CLKHF_IN_CLKPATH0
#define CY_CFG_SYSCLK_CLKHF1_ENABLED 1
#define CY_CFG_SYSCLK_CLKHF1_DIVIDER CY_SYSCLK_CLKHF_NO_DIVIDE
#define CY_CFG_SYSCLK_CLKHF1_FREQ_MHZ 96UL
#define CY_CFG_SYSCLK_CLKHF1_CLKPATH CY_SYSCLK_CLKHF_IN_CLKPATH0
#define CY_CFG_SYSCLK_CLKHF2_ENABLED 1
#define CY_CFG_SYSCLK_CLKHF2_DIVIDER CY_SYSCLK_CLKHF_NO_DIVIDE
#define CY_CFG_SYSCLK_CLKHF2_FREQ_MHZ 48UL
#define CY_CFG_SYSCLK_CLKHF2_CLKPATH CY_SYSCLK_CLKHF_IN_CLKPATH2
#define CY_CFG_SYSCLK_CLKHF3_ENABLED 1
#define CY_CFG_SYSCLK_CLKHF3_DIVIDER CY_SYSCLK_CLKHF_DIVIDE_BY_2
#define CY_CFG_SYSCLK_CLKHF3_FREQ_MHZ 24UL
#define CY_CFG_SYSCLK_CLKHF3_CLKPATH CY_SYSCLK_CLKHF_IN_CLKPATH1
#define CY_CFG_SYSCLK_IHO_ENABLED 1
#define CY_CFG_SYSCLK_IMO_ENABLED 1
#define CY_CFG_SYSCLK_CLKLF_ENABLED 1
#define CY_CFG_SYSCLK_MFO_ENABLED 1
#define CY_CFG_SYSCLK_MFO_DEEPSLEEP_ENABLED false
#define CY_CFG_SYSCLK_CLKPATH0_ENABLED 1
#define CY_CFG_SYSCLK_CLKPATH0_SOURCE CY_SYSCLK_CLKPATH_IN_IHO
#define CY_CFG_SYSCLK_CLKPATH0_SOURCE_NUM 7UL
#define CY_CFG_SYSCLK_CLKPATH1_ENABLED 1
#define CY_CFG_SYSCLK_CLKPATH1_SOURCE CY_SYSCLK_CLKPATH_IN_IHO
#define CY_CFG_SYSCLK_CLKPATH1_SOURCE_NUM 7UL
#define CY_CFG_SYSCLK_CLKPATH2_ENABLED 1
#define CY_CFG_SYSCLK_CLKPATH2_SOURCE CY_SYSCLK_CLKPATH_IN_IHO
#define CY_CFG_SYSCLK_CLKPATH2_SOURCE_NUM 7UL
#define CY_CFG_SYSCLK_CLKPATH3_ENABLED 1
#define CY_CFG_SYSCLK_CLKPATH3_SOURCE CY_SYSCLK_CLKPATH_IN_IMO
#define CY_CFG_SYSCLK_CLKPATH3_SOURCE_NUM 0UL
#define CY_CFG_SYSCLK_PILO_ENABLED 1
#define CY_CFG_PWR_ENABLED 1
#define CY_CFG_PWR_INIT 1
#define CY_CFG_PWR_USING_PMIC 0
#define CY_CFG_PWR_VBACKUP_USING_VDDD 1
#define CY_CFG_PWR_REGULATOR_MODE_MIN false
#define CY_CFG_PWR_USING_ULP 0

#if (!defined(CY_DEVICE_SECURE))
    static const cy_stc_fll_manual_config_t srss_0_clock_0_fll_0_fllConfig = 
    {
        .fllMult = 500U,
        .refDiv = 125U,
        .ccoRange = CY_SYSCLK_FLL_CCO_RANGE4,
        .enableOutputDiv = true,
        .lockTolerance = 10U,
        .igain = 9U,
        .pgain = 4U,
        .settlingCount = 48U,
        .outputMode = CY_SYSCLK_FLLPLL_OUTPUT_OUTPUT,
        .cco_Freq = 320U,
    };
#endif //(!defined(CY_DEVICE_SECURE))
#if defined (CY_USING_HAL)
    const cyhal_resource_inst_t srss_0_clock_0_pathmux_0_obj = 
    {
        .type = CYHAL_RSC_CLKPATH,
        .block_num = 0U,
        .channel_num = 0U,
    };
    const cyhal_resource_inst_t srss_0_clock_0_pathmux_1_obj = 
    {
        .type = CYHAL_RSC_CLKPATH,
        .block_num = 1U,
        .channel_num = 0U,
    };
    const cyhal_resource_inst_t srss_0_clock_0_pathmux_2_obj = 
    {
        .type = CYHAL_RSC_CLKPATH,
        .block_num = 2U,
        .channel_num = 0U,
    };
    const cyhal_resource_inst_t srss_0_clock_0_pathmux_3_obj = 
    {
        .type = CYHAL_RSC_CLKPATH,
        .block_num = 3U,
        .channel_num = 0U,
    };
#endif //defined (CY_USING_HAL)
static cy_stc_syspm_core_buck_params_t coreBuckConfigParam = 
{
    .voltageSel = CY_CFG_PWR_CBUCK_VOLT,
    .mode = CY_CFG_PWR_CBUCK_MODE,
    .override = false,
    .copySettings = false,
    .useSettings = false,
    .inRushLimitSel = 0,
};
static cy_stc_syspm_sdr_params_t sdr0ConfigParam = 
{
    .coreBuckVoltSel = CY_CFG_PWR_CBUCK_VOLT,
    .coreBuckMode = CY_CFG_PWR_CBUCK_MODE,
    .coreBuckDpSlpVoltSel = CY_SYSPM_CORE_BUCK_VOLTAGE_0_90V,
    .coreBuckDpSlpMode = CY_SYSPM_CORE_BUCK_MODE_LP,
    .sdr0DpSlpVoltSel = CY_SYSPM_SDR_VOLTAGE_0_900V,
    .sdrVoltSel = CY_CFG_PWR_SDR0_VOLT,
    .sdr0Allowbypass = CY_CFG_PWR_SDR0_MODE_BYPASS,
};
static cy_stc_syspm_sdr_params_t sdr1ConfigParam = 
{
    .coreBuckVoltSel = CY_CFG_PWR_CBUCK_VOLT,
    .coreBuckMode = CY_CFG_PWR_CBUCK_MODE,
    .sdrVoltSel = CY_CFG_PWR_SDR1_VOLT,
    .sdr1HwControl = true,
    .sdr1Enable = true,
};

__WEAK void cycfg_ClockStartupError(uint32_t error)
{
    (void)error; /* Suppress the compiler warning */
    while(1);
}
#if (!defined(CY_DEVICE_SECURE))
    __STATIC_INLINE void Cy_SysClk_FllDeInit()
    {
        Cy_SysClk_FllDisable();
    }
    __STATIC_INLINE void Cy_SysClk_ClkBakInit()
    {
        Cy_SysClk_ClkBakSetSource(CY_SYSCLK_BAK_IN_CLKLF);
    }
    __STATIC_INLINE void Cy_SysClk_FllInit()
    {
        if (CY_SYSCLK_SUCCESS != Cy_SysClk_FllManualConfigure(&srss_0_clock_0_fll_0_fllConfig))
        {
            cycfg_ClockStartupError(CY_CFG_SYSCLK_FLL_ERROR);
        }
        if (CY_SYSCLK_SUCCESS != Cy_SysClk_FllEnable(200000UL))
        {
            cycfg_ClockStartupError(CY_CFG_SYSCLK_FLL_ERROR);
        }
    }
    __STATIC_INLINE void Cy_SysClk_ClkHf0Init()
    {
        Cy_SysClk_ClkHfSetSource(0U, CY_CFG_SYSCLK_CLKHF0_CLKPATH);
        Cy_SysClk_ClkHfSetDivider(0U, CY_SYSCLK_CLKHF_NO_DIVIDE);
    }
    __STATIC_INLINE void Cy_SysClk_ClkHf1Init()
    {
        Cy_SysClk_ClkHfSetSource(CY_CFG_SYSCLK_CLKHF1, CY_CFG_SYSCLK_CLKHF1_CLKPATH);
        Cy_SysClk_ClkHfSetDivider(CY_CFG_SYSCLK_CLKHF1, CY_SYSCLK_CLKHF_NO_DIVIDE);
        Cy_SysClk_ClkHfEnable(CY_CFG_SYSCLK_CLKHF1);
    }
    __STATIC_INLINE void Cy_SysClk_ClkHf2Init()
    {
        Cy_SysClk_ClkHfSetSource(CY_CFG_SYSCLK_CLKHF2, CY_CFG_SYSCLK_CLKHF2_CLKPATH);
        Cy_SysClk_ClkHfSetDivider(CY_CFG_SYSCLK_CLKHF2, CY_SYSCLK_CLKHF_NO_DIVIDE);
        Cy_SysClk_ClkHfEnable(CY_CFG_SYSCLK_CLKHF2);
    }
    __STATIC_INLINE void Cy_SysClk_ClkHf3Init()
    {
        Cy_SysClk_ClkHfSetSource(CY_CFG_SYSCLK_CLKHF3, CY_CFG_SYSCLK_CLKHF3_CLKPATH);
        Cy_SysClk_ClkHfSetDivider(CY_CFG_SYSCLK_CLKHF3, CY_SYSCLK_CLKHF_DIVIDE_BY_2);
        Cy_SysClk_ClkHfEnable(CY_CFG_SYSCLK_CLKHF3);
    }
#endif //(!defined(CY_DEVICE_SECURE))
__STATIC_INLINE void Cy_SysClk_IhoInit()
{
    Cy_SysClk_IhoEnable();
}
#if (!defined(CY_DEVICE_SECURE))
    __STATIC_INLINE void Cy_SysClk_ClkLfInit()
    {
        /* The WDT is unlocked in the default startup code */
        Cy_SysClk_ClkLfSetSource(CY_SYSCLK_CLKLF_IN_PILO);
    }
#endif //(!defined(CY_DEVICE_SECURE))
__STATIC_INLINE void Cy_SysClk_MfoInit()
{
    Cy_SysClk_MfoEnable(CY_CFG_SYSCLK_MFO_DEEPSLEEP_ENABLED);
}
#if (!defined(CY_DEVICE_SECURE))
    __STATIC_INLINE void Cy_SysClk_ClkPath0Init()
    {
        Cy_SysClk_ClkPathSetSource(0U, CY_CFG_SYSCLK_CLKPATH0_SOURCE);
    }
    __STATIC_INLINE void Cy_SysClk_ClkPath1Init()
    {
        Cy_SysClk_ClkPathSetSource(1U, CY_CFG_SYSCLK_CLKPATH1_SOURCE);
    }
    __STATIC_INLINE void Cy_SysClk_ClkPath2Init()
    {
        Cy_SysClk_ClkPathSetSource(2U, CY_CFG_SYSCLK_CLKPATH2_SOURCE);
    }
    __STATIC_INLINE void Cy_SysClk_ClkPath3Init()
    {
        Cy_SysClk_ClkPathSetSource(3U, CY_CFG_SYSCLK_CLKPATH3_SOURCE);
    }
#endif //(!defined(CY_DEVICE_SECURE))
__STATIC_INLINE void Cy_SysClk_PiloInit()
{
    Cy_SysClk_PiloEnable();

    if(!Cy_SysClk_PiloOkay())
    {
        Cy_SysPm_TriggerXRes();
    }
}
__STATIC_INLINE void init_cycfg_power(void)
{
    CY_UNUSED_PARAMETER(sdr1ConfigParam); /* Suppress a compiler warning about unused variables */

    Cy_SysPm_Init();
    /* **Reset the Backup domain on POR, XRES, BOD only if Backup domain is supplied by VDDD** */
    #if (CY_CFG_PWR_VBACKUP_USING_VDDD)
        #ifdef CY_CFG_SYSCLK_ILO_ENABLED
            if (0u == Cy_SysLib_GetResetReason() /* POR, XRES, or BOD */)
            {
                Cy_SysLib_ResetBackupDomain();
                Cy_SysClk_IloDisable();
                Cy_SysClk_IloInit();
            }
        #endif /* CY_CFG_SYSCLK_ILO_ENABLED */
    #endif /* CY_CFG_PWR_VBACKUP_USING_VDDD */

    /* **System Active Power Mode Profile Configuration** */
    /* Core Buck Regulator Configuration */
    Cy_SysPm_CoreBuckConfig(&coreBuckConfigParam);

    /* SDR0 Regulator Configuration */
    Cy_SysPm_SdrConfigure(CY_SYSPM_SDR_0, &sdr0ConfigParam);

    /* SDR1 Regulator Configuration */
    #if (CY_CFG_PWR_SDR1_ENABLE)
        Cy_SysPm_SdrConfigure(CY_SYSPM_SDR_1, &sdr1ConfigParam);
    #endif /* CY_CFG_PWR_SDR1_VOLT */

    /* **System Active Low Power Profile(LPACTIVE/LPSLEEP) Configuration** */
    #if (CY_CFG_PWR_SYS_LP_PROFILE_MODE)
        Cy_SysPm_SystemLpActiveEnter();
    #endif /* CY_CFG_PWR_SYS_ACTIVE_MODE */

    /* **System Regulators Low Current Configuration** */
    #if (CY_CFG_PWR_REGULATOR_MODE_MIN)
        Cy_SysPm_SystemSetMinRegulatorCurrent();
    #endif /* CY_CFG_PWR_REGULATOR_MODE_MIN */

    /* **System Idle Power Mode Configuration** */
    #if (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP)
        Cy_SysPm_SetDeepSleepMode(CY_SYSPM_MODE_DEEPSLEEP);
    #elif (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP_RAM)
        Cy_SysPm_SetDeepSleepMode(CY_SYSPM_MODE_DEEPSLEEP_RAM);
    #elif (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP_OFF)
        Cy_SysPm_SetDeepSleepMode(CY_SYSPM_MODE_DEEPSLEEP_OFF);
    #endif /* CY_CFG_PWR_SYS_IDLE_MODE */
}


void init_cycfg_system(void)
{
    #ifdef CY_CFG_PWR_ENABLED
        #ifdef CY_CFG_PWR_INIT
            init_cycfg_power();
        #else
            #warning Power system will not be configured. Update power personality to v1.20 or later.
        #endif /* CY_CFG_PWR_INIT */
    #endif /* CY_CFG_PWR_ENABLED */
    
        /* Disable FLL */
        Cy_SysClk_FllDeInit();
    
    /* Enable all source clocks */
    #ifdef CY_CFG_SYSCLK_PILO_ENABLED
        Cy_SysClk_PiloInit();
    #endif
    
    #ifdef CY_CFG_SYSCLK_WCO_ENABLED
        Cy_SysClk_WcoInit();
    #endif
    
    #ifdef CY_CFG_SYSCLK_CLKLF_ENABLED
        Cy_SysClk_ClkLfInit();
    #endif
    
    #ifdef CY_CFG_SYSCLK_EXTCLK_ENABLED
        Cy_SysClk_ExtClkInit();
    #endif
    
    #ifdef CY_CFG_SYSCLK_IHO_ENABLED
        Cy_SysClk_IhoInit();
    #endif
    
    #ifdef CY_CFG_SYSCLK_ALTHF_ENABLED
        Cy_SysClk_AltHfInit();
    #endif
    
    /* Configure Path Clocks */
    #ifdef CY_CFG_SYSCLK_CLKPATH1_ENABLED
        Cy_SysClk_ClkPath1Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKPATH2_ENABLED
        Cy_SysClk_ClkPath2Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKPATH3_ENABLED
        Cy_SysClk_ClkPath3Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKPATH4_ENABLED
        Cy_SysClk_ClkPath4Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKPATH5_ENABLED
        Cy_SysClk_ClkPath5Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKPATH6_ENABLED
        Cy_SysClk_ClkPath6Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKPATH7_ENABLED
        Cy_SysClk_ClkPath7Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKPATH8_ENABLED
        Cy_SysClk_ClkPath8Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKPATH9_ENABLED
        Cy_SysClk_ClkPath9Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKPATH10_ENABLED
        Cy_SysClk_ClkPath10Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKPATH11_ENABLED
        Cy_SysClk_ClkPath11Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKPATH12_ENABLED
        Cy_SysClk_ClkPath12Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKPATH13_ENABLED
        Cy_SysClk_ClkPath13Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKPATH14_ENABLED
        Cy_SysClk_ClkPath14Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKPATH15_ENABLED
        Cy_SysClk_ClkPath15Init();
    #endif
    
    /* Configure HF clocks */
    #ifdef CY_CFG_SYSCLK_CLKHF1_ENABLED
        Cy_SysClk_ClkHf1Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKHF2_ENABLED
        Cy_SysClk_ClkHf2Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKHF3_ENABLED
        Cy_SysClk_ClkHf3Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKHF4_ENABLED
        Cy_SysClk_ClkHf4Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKHF5_ENABLED
        Cy_SysClk_ClkHf5Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKHF6_ENABLED
        Cy_SysClk_ClkHf6Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKHF7_ENABLED
        Cy_SysClk_ClkHf7Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKHF8_ENABLED
        Cy_SysClk_ClkHf8Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKHF9_ENABLED
        Cy_SysClk_ClkHf9Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKHF10_ENABLED
        Cy_SysClk_ClkHf10Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKHF11_ENABLED
        Cy_SysClk_ClkHf11Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKHF12_ENABLED
        Cy_SysClk_ClkHf12Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKHF13_ENABLED
        Cy_SysClk_ClkHf13Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKHF14_ENABLED
        Cy_SysClk_ClkHf14Init();
    #endif
    #ifdef CY_CFG_SYSCLK_CLKHF15_ENABLED
        Cy_SysClk_ClkHf15Init();
    #endif
    
    /* Configure miscellaneous clocks */
    #ifdef CY_CFG_SYSCLK_CLKALTSYSTICK_ENABLED
        Cy_SysClk_ClkAltSysTickInit();
    #endif
    
    #ifdef CY_CFG_SYSCLK_CLKPUMP_ENABLED
        Cy_SysClk_ClkPumpInit();
    #endif
    
    #ifdef CY_CFG_SYSCLK_CLKBAK_ENABLED
        Cy_SysClk_ClkBakInit();
    #endif
    
    /* Configure default enabled clocks */
    #ifdef CY_CFG_SYSCLK_ILO_ENABLED
        Cy_SysClk_IloInit();
    #endif
    
    #ifndef CY_CFG_SYSCLK_IMO_ENABLED
        #error the IMO must be enabled for proper chip operation
    #endif
    
    #ifdef CY_CFG_SYSCLK_MFO_ENABLED
        Cy_SysClk_MfoInit();
    #endif
    
    #ifdef CY_CFG_SYSCLK_CLKMF_ENABLED
        Cy_SysClk_ClkMfInit();
    #endif
    
    #ifdef CY_CFG_SYSCLK_CLKPATH0_ENABLED
        Cy_SysClk_ClkPath0Init();
    #endif
    /* Configure and enable FLL */
    #ifdef CY_CFG_SYSCLK_FLL_ENABLED
        Cy_SysClk_FllInit();
    #endif
    
    Cy_SysClk_ClkHf0Init();
    
    /* Disable Unused Clock Sources */
    #ifndef CY_CFG_SYSCLK_IHO_ENABLED
        Cy_SysClk_IhoDisable();
    #endif
#if defined(CY_CFG_SYSCLK_ECO_PRESCALER_ENABLED)
        Cy_SysClk_EcoPrescalerInit();
#endif //defined(CY_CFG_SYSCLK_ECO_PRESCALER_ENABLED)
    /* Update System Core Clock values for correct Cy_SysLib_Delay functioning */
    SystemCoreClockUpdate();
#if (!defined(CY_DEVICE_SECURE))
    CY_UNUSED_PARAM(srss_0_clock_0_fll_0_fllConfig);
#endif //(!defined(CY_DEVICE_SECURE))
}

void reserve_cycfg_system(void)
{
#if defined (CY_USING_HAL)
    cyhal_hwmgr_reserve(&srss_0_clock_0_pathmux_0_obj);
    cyhal_hwmgr_reserve(&srss_0_clock_0_pathmux_1_obj);
    cyhal_hwmgr_reserve(&srss_0_clock_0_pathmux_2_obj);
    cyhal_hwmgr_reserve(&srss_0_clock_0_pathmux_3_obj);
#endif //defined (CY_USING_HAL)
}
