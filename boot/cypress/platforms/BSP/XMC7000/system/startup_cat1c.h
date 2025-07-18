/*******************************************************************************
* \copyright
* Copyright 2025 Cypress Semiconductor Corporation
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

#ifndef __STARTUP_CAT1C_H__
#define __STARTUP_CAT1C_H__

#define FIXED_EXP_NR            (15u)
#define VECTORTABLE_SIZE        (16u + FIXED_EXP_NR + 1u) /* +1 is for Stack pointer */
#define VECTORTABLE_ALIGN       (128) /* alignment for 85 entries (32x4=128) is 2^7=128 bytes */


#endif /* __STARTUP_CAT1C_H__ */
