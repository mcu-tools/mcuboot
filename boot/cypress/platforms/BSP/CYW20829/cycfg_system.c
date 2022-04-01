/*******************************************************************************
* File Name: cycfg_system.c
*
* Description:
* System configuration
* This file was automatically generated and should not be modified.
* Tools Package 2.2.1.3040
* integration_mxs40sv2-LATEST 3.0.0.5994
* personalities 3.0.0.0
* udd 3.0.0.775
*
********************************************************************************
* Copyright 2021 Cypress Semiconductor Corporation
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
#define CY_CFG_SYSCLK_FLL_ENABLED 1
#define CY_CFG_SYSCLK_FLL_MULT 504U
#define CY_CFG_SYSCLK_FLL_REFDIV 42U
#define CY_CFG_SYSCLK_FLL_CCO_RANGE CY_SYSCLK_FLL_CCO_RANGE2
#define CY_CFG_SYSCLK_FLL_ENABLE_OUTDIV true
#define CY_CFG_SYSCLK_FLL_LOCK_TOLERANCE 10U
#define CY_CFG_SYSCLK_FLL_IGAIN 8U
#define CY_CFG_SYSCLK_FLL_PGAIN 7U
#define CY_CFG_SYSCLK_FLL_SETTLING_COUNT 8U
#define CY_CFG_SYSCLK_FLL_OUTPUT_MODE CY_SYSCLK_FLLPLL_OUTPUT_OUTPUT
#define CY_CFG_SYSCLK_FLL_CCO_FREQ 198U
#define CY_CFG_SYSCLK_FLL_OUT_FREQ 48000000
#define CY_CFG_SYSCLK_CLKHF0_ENABLED 1
#define CY_CFG_SYSCLK_CLKHF0_DIVIDER CY_SYSCLK_CLKHF_NO_DIVIDE
#define CY_CFG_SYSCLK_CLKHF0_FREQ_MHZ 48UL
#define CY_CFG_SYSCLK_CLKHF0_CLKPATH CY_SYSCLK_CLKHF_IN_CLKPATH0
#define CY_CFG_SYSCLK_CLKHF1_ENABLED 1
#define CY_CFG_SYSCLK_CLKHF1_DIVIDER CY_SYSCLK_CLKHF_NO_DIVIDE
#define CY_CFG_SYSCLK_CLKHF1_FREQ_MHZ 8UL
#define CY_CFG_SYSCLK_CLKHF1_CLKPATH CY_SYSCLK_CLKHF_IN_CLKPATH1
#define CY_CFG_SYSCLK_IMO_ENABLED 1
#define CY_CFG_SYSCLK_CLKPATH0_ENABLED 1
#define CY_CFG_SYSCLK_CLKPATH0_SOURCE CY_SYSCLK_CLKPATH_IN_IMO
#define CY_CFG_SYSCLK_CLKPATH0_SOURCE_NUM 0UL
#define CY_CFG_SYSCLK_CLKPATH1_ENABLED 1
#define CY_CFG_SYSCLK_CLKPATH1_SOURCE CY_SYSCLK_CLKPATH_IN_IMO
#define CY_CFG_SYSCLK_CLKPATH1_SOURCE_NUM 0UL
#define CY_CFG_SYSCLK_CLKPATH2_ENABLED 1
#define CY_CFG_SYSCLK_CLKPATH2_SOURCE CY_SYSCLK_CLKPATH_IN_IMO
#define CY_CFG_SYSCLK_CLKPATH2_SOURCE_NUM 0UL
#define CY_CFG_SYSCLK_CLKPATH3_ENABLED 1
#define CY_CFG_SYSCLK_CLKPATH3_SOURCE CY_SYSCLK_CLKPATH_IN_IMO
#define CY_CFG_SYSCLK_CLKPATH3_SOURCE_NUM 0UL
#define CY_CFG_SYSCLK_CLKTIMER_ENABLED 1
#define CY_CFG_SYSCLK_CLKTIMER_SOURCE CY_SYSCLK_CLKTIMER_IN_IMO
#define CY_CFG_SYSCLK_CLKTIMER_DIVIDER 0U

void cycfg_ClockStartupError(uint32_t error);

#if defined (CY_USING_HAL)
	const cyhal_resource_inst_t srss_0_clock_0_pathmux_0_obj =
	{
		.type = CYHAL_RSC_CLKPATH,
		.block_num = 0U,
		.channel_num = 0U,
	};
#endif //defined (CY_USING_HAL)
#if defined (CY_USING_HAL)
	const cyhal_resource_inst_t srss_0_clock_0_pathmux_1_obj =
	{
		.type = CYHAL_RSC_CLKPATH,
		.block_num = 1U,
		.channel_num = 0U,
	};
#endif //defined (CY_USING_HAL)
#if defined (CY_USING_HAL)
	const cyhal_resource_inst_t srss_0_clock_0_pathmux_2_obj =
	{
		.type = CYHAL_RSC_CLKPATH,
		.block_num = 2U,
		.channel_num = 0U,
	};
#endif //defined (CY_USING_HAL)
#if defined (CY_USING_HAL)
	const cyhal_resource_inst_t srss_0_clock_0_pathmux_3_obj =
	{
		.type = CYHAL_RSC_CLKPATH,
		.block_num = 3U,
		.channel_num = 0U,
	};
#endif //defined (CY_USING_HAL)

__WEAK void cycfg_ClockStartupError(uint32_t error)
{
    (void)error; /* Suppress the compiler warning */
    while (true) {}
}
__STATIC_INLINE void Cy_SysClk_FllInit(void)
{
    Cy_SysClk_FllOutputDividerEnable(false);

    if (CY_SYSCLK_SUCCESS != Cy_SysClk_FllEnable(0UL))
    {
        cycfg_ClockStartupError(CY_CFG_SYSCLK_FLL_ERROR);
    }
}
__STATIC_INLINE void Cy_SysClk_ClkHf0Init(void)
{
    (void)Cy_SysClk_ClkHfSetSource(0U, CY_CFG_SYSCLK_CLKHF0_CLKPATH);
    (void)Cy_SysClk_ClkHfSetDivider(0U, CY_SYSCLK_CLKHF_NO_DIVIDE);
}
__STATIC_INLINE void Cy_SysClk_ClkHf1Init(void)
{
    (void)Cy_SysClk_ClkHfSetSource(CY_CFG_SYSCLK_CLKHF1, CY_CFG_SYSCLK_CLKHF1_CLKPATH);
    (void)Cy_SysClk_ClkHfSetDivider(CY_CFG_SYSCLK_CLKHF1, CY_SYSCLK_CLKHF_NO_DIVIDE);
    (void)Cy_SysClk_ClkHfEnable(CY_CFG_SYSCLK_CLKHF1);
}
__STATIC_INLINE void Cy_SysClk_ClkPath0Init(void)
{
    (void)Cy_SysClk_ClkPathSetSource(0U, CY_CFG_SYSCLK_CLKPATH0_SOURCE);
}
__STATIC_INLINE void Cy_SysClk_ClkPath1Init(void)
{
    (void)Cy_SysClk_ClkPathSetSource(1U, CY_CFG_SYSCLK_CLKPATH1_SOURCE);
}
__STATIC_INLINE void Cy_SysClk_ClkPath2Init(void)
{
    (void)Cy_SysClk_ClkPathSetSource(2U, CY_CFG_SYSCLK_CLKPATH2_SOURCE);
}
__STATIC_INLINE void Cy_SysClk_ClkPath3Init(void)
{
    (void)Cy_SysClk_ClkPathSetSource(3U, CY_CFG_SYSCLK_CLKPATH3_SOURCE);
}


void init_cycfg_system(void)
{

	    /* Set worst case memory wait states (! ultra low power, 150 MHz), will update at the end */
	    Cy_SysLib_SetWaitStates(false, 150UL);
	    #ifdef CY_CFG_PWR_ENABLED
	        #ifdef CY_CFG_PWR_INIT
	            init_cycfg_power();
	        #else
	            #warning Power system will not be configured. Update power personality to v1.20 or later.
	        #endif /* CY_CFG_PWR_INIT */
	    #endif /* CY_CFG_PWR_ENABLED */

	    /* Reset the core clock path to default and disable all the FLLs*/
	    (void)Cy_SysClk_ClkHfSetDivider(0U, CY_SYSCLK_CLKHF_NO_DIVIDE);
	    (void)Cy_SysClk_ClkPathSetSource((uint32_t)CY_SYSCLK_CLKHF_IN_CLKPATH1, CY_SYSCLK_CLKPATH_IN_IMO);

	    if ((CY_SYSCLK_CLKHF_IN_CLKPATH0 == Cy_SysClk_ClkHfGetSource(0UL)) &&
	        (CY_SYSCLK_CLKPATH_IN_WCO == Cy_SysClk_ClkPathGetSource((uint32_t)CY_SYSCLK_CLKHF_IN_CLKPATH0)))
	    {
	        (void)Cy_SysClk_ClkHfSetSource(0U, CY_SYSCLK_CLKHF_IN_CLKPATH1);
	    }

	    (void)Cy_SysClk_FllDisable();
	    (void)Cy_SysClk_ClkPathSetSource((uint32_t)CY_SYSCLK_CLKHF_IN_CLKPATH0, CY_SYSCLK_CLKPATH_IN_IMO);
	    (void)Cy_SysClk_ClkHfSetSource(0UL, CY_SYSCLK_CLKHF_IN_CLKPATH0);
	    #ifdef CY_IP_MXBLESS
	        (void)Cy_BLE_EcoReset();
	    #endif


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

	    #ifdef CY_CFG_SYSCLK_ALTHF_ENABLED
	        Cy_SysClk_AltHfInit();
	    #endif

	    #ifdef CY_CFG_SYSCLK_ECO_ENABLED
	        Cy_SysClk_EcoInit();
	    #endif

	    #ifdef CY_CFG_SYSCLK_EXTCLK_ENABLED
	        Cy_SysClk_ExtClkInit();
	    #endif

	    #if ((CY_CFG_SYSCLK_CLKPATH0_SOURCE_NUM == 0x6UL) && (CY_CFG_SYSCLK_CLKHF0_CLKPATH_NUM == 0U))
	        /* Configure HFCLK0 to temporarily run from IMO to initialize other clocks */
	        (void)Cy_SysClk_ClkPathSetSource(1UL, CY_SYSCLK_CLKPATH_IN_IMO);
	        (void)Cy_SysClk_ClkHfSetSource(0UL, CY_SYSCLK_CLKHF_IN_CLKPATH1);
	    #else
	        #ifdef CY_CFG_SYSCLK_CLKPATH1_ENABLED
	            Cy_SysClk_ClkPath1Init();
	        #endif
	    #endif

	    /* Configure Path Clocks */
	    #ifdef CY_CFG_SYSCLK_CLKPATH0_ENABLED
	        Cy_SysClk_ClkPath0Init();
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

	    /* Configure and enable FLL */
	    #ifdef CY_CFG_SYSCLK_FLL_ENABLED
	        Cy_SysClk_FllInit();
	    #endif

	    Cy_SysClk_ClkHf0Init();

	    #if ((CY_CFG_SYSCLK_CLKPATH0_SOURCE_NUM == 0x6UL) && (CY_CFG_SYSCLK_CLKHF0_CLKPATH_NUM == 0U))
	        #ifdef CY_CFG_SYSCLK_CLKPATH1_ENABLED
	            /* Apply the ClkPath1 user setting */
	            Cy_SysClk_ClkPath1Init();
	        #endif
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

	/* Set accurate flash wait states */
	#if (defined (CY_CFG_PWR_ENABLED) && defined (CY_CFG_SYSCLK_CLKHF0_ENABLED))
	    Cy_SysLib_SetWaitStates(CY_CFG_PWR_USING_ULP != 0, CY_CFG_SYSCLK_CLKHF0_FREQ_MHZ);
	#endif

	/* Update System Core Clock values for correct Cy_SysLib_Delay functioning */
	SystemCoreClockUpdate();

#if defined (CY_USING_HAL)
	(void)cyhal_hwmgr_reserve(&srss_0_clock_0_pathmux_0_obj);
#endif //defined (CY_USING_HAL)

#if defined (CY_USING_HAL)
	(void)cyhal_hwmgr_reserve(&srss_0_clock_0_pathmux_1_obj);
#endif //defined (CY_USING_HAL)

#if defined (CY_USING_HAL)
	(void)cyhal_hwmgr_reserve(&srss_0_clock_0_pathmux_2_obj);
#endif //defined (CY_USING_HAL)

#if defined (CY_USING_HAL)
	(void)cyhal_hwmgr_reserve(&srss_0_clock_0_pathmux_3_obj);
#endif //defined (CY_USING_HAL)
}
