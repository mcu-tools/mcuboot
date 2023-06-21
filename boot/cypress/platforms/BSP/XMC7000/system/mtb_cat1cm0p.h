/***************************************************************************//**
* \file mtb_cat1cm0p.h
* \version 1.0
*
* \brief
* Provides a macro for BSP to indicate whether a prebuilt CM0P image is in use.
*
********************************************************************************
* \copyright
* Copyright (c) (2022), Cypress Semiconductor Corporation (an Infineon company) or
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
*******************************************************************************/

#if defined(COMPONENT_CAT1A)
#if defined(COMPONENT_CM0P_BLESS) || defined(COMPONENT_CM0P_CRYPTO) || defined(COMPONENT_CM0P_SECURE) || defined(COMPONENT_CM0P_SLEEP)
    #define CY_USING_PREBUILT_CM0P_IMAGE
#endif /* defined(COMPONENT_CM0P_BLESS) || defined(COMPONENT_CM0P_CRYPTO) || defined(COMPONENT_CM0P_SECURE) || defined(COMPONENT_CM0P_SLEEP) */
#elif defined(COMPONENT_CAT1C)
#if defined(COMPONENT_XMC7x_CM0P_SLEEP) || defined(COMPONENT_XMC7xDUAL_CM0P_SLEEP)
    #define CY_USING_PREBUILT_CM0P_IMAGE
#endif /* defined(COMPONENT_XMC7x_CM0P_SLEEP) || defined(COMPONENT_XMC7xDUAL_CM0P_SLEEP) */
#endif /* COMPONENT_CAT1A, COMPONENT_CAT1C */