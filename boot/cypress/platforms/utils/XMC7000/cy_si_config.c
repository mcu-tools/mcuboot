/********************************************************************************
* Copyright 2023 Infineon Technologies AG
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

#include "cmsis_compiler.h"
#include "cy_crypto_common.h"
#include "cy_si_config.h"
#include "cy_si_keystorage.h"
#include "cy_syslib.h"

/** Linker script symbols */
extern const char __app_header_vtable_offset[];
extern const char __secure_object_size[];

#define CY_M0PLUS_SI_VECTOR_OFFSET ((uint32_t)__app_header_vtable_offset)
#define CY_M0PLUS_SI_SIZE ((uint32_t)__secure_object_size)
#define CY_SI_VT_OFFSET (CY_M0PLUS_SI_VECTOR_OFFSET) - offsetof(cy_stc_si_appheader_t, core0Vt) /**< CM0+ VT Offset */
#define CY_SI_CPUID (0xC6000000UL)                                                              /**< CM0+ ARM CPUID[15:4] Reg shifted to [31:20] */
#define CY_SI_CORE_IDX (0UL)                                                                    /**< CM0+ core ID */

/** Flashboot parameters */
#define CY_SI_FLASHBOOT_FLAGS                                              \
    ((CY_SI_FLASHBOOT_CLK_100MHZ << CY_SI_TOC_FLAGS_CLOCKS_POS) |          \
     (CY_SI_FLASHBOOT_WAIT_20MS << CY_SI_TOC_FLAGS_DELAY_POS) |            \
     (CY_SI_FLASHBOOT_SWJ_ENABLE << CY_SI_TOC_FLAGS_SWJEN_POS) |           \
     (CY_SI_FLASHBOOT_VALIDATE_ENABLE << CY_SI_TOC_FLAGS_APP_VERIFY_POS) | \
     (CY_SI_FLASHBOOT_FBLOADER_DISABLE << CY_SI_TOC_FLAGS_FBLOADER_ENABLE_POS))

extern const cy_stc_si_toc_t cy_toc2;
/** TOC2 in SFlash */
CY_SECTION(".cy_toc_part2")
__USED const cy_stc_si_toc_t cy_toc2 = {
    .objSize        = CY_SI_TOC2_OBJECTSIZE,         /* Offset+0x00: Object Size (Bytes) excluding CRC */
    .magicNum       = CY_SI_TOC2_MAGICNUMBER,        /* Offset+0x04: TOC2 ID (magic number) */
    .smifCfgAddr    = 0UL,                           /* Offset+0x08: SMIF config list pointer */
    .cm0pappAddr1   = CY_SI_SECURE_FLASH_BEGIN,      /* Offset+0x0C: App1 (CM0+ First User App Object) addr */
    .cm0pappFormat1 = CY_SI_APP_FORMAT_CYPRESS,      /* Offset+0x10: App1 Format */
    .cm0pappAddr2   = CY_SI_USERAPP_FLASH_BEGIN,     /* Offset+0x14: App2 (CM0+ Second User App Object) addr */
    .cm0pappFormat2 = CY_SI_APP_FORMAT_BASIC,        /* Offset+0x18: App2 Format */
    .cm71appAddr1   = CY_SI_CM71_1stAPP_FLASH_BEGIN, /* Offset+0x1C: App3 (CM7_1 1st User App Object) addr */
    .cm71appAddr2   = CY_SI_CM71_2ndAPP_FLASH_BEGIN, /* Offset+0x20: App4 (CM7_1 2nd User App Object) addr */
    .cm72appAddr1   = CY_SI_CM72_1stAPP_FLASH_BEGIN, /* Offset+0x24: App5 (CM7_2 1st User App Object) addr */
    .cm72appAddr2   = CY_SI_CM72_2ndAPP_FLASH_BEGIN, /* Offset+0x28: App6 (CM7_2 1st User App Object) addr */
    .reserved1      = {0UL},                         /* Offset+0x2C-0xFB: Reserved area 212Bytes */
    .securityMarker = CY_SECURITY_NOT_ENHANCED,      /* Offset+0xFC Security Enhance Marker */
    .shashObj       = 3UL,                           /* Offset+0x100: Number of verified additional objects (S-HASH)*/
    .sigKeyAddr     = CY_SI_PUBLIC_KEY,              /* Offset+0x104: Addr of signature verification key */
    .swpuAddr       = CY_SI_SWPU_BEGIN,              /* Offset+0x108: Addr of SWPU Objects */
    .toc2Addr       = (uint32_t)&cy_toc2,            /* Offset+0x10C: TOC2_OBJECT_ADDR */
    .addObj         = {0UL},                         /* Offset+0x110-0x1F4: Reserved area 232Bytes */
    .tocFlags       = CY_SI_FLASHBOOT_FLAGS,         /* Flashboot flags stored in TOC2 */
    .crc            = 0UL,                           /* Offset+0x1FC: Reserved area 1Byte */
};

/** Secure Application header */
CY_SECTION(".cy_app_header")
__USED const cy_stc_si_appheader_t cy_si_appHeader = {
    .objSize       = CY_M0PLUS_SI_SIZE,
    .appId         = (CY_SI_APP_VERSION | CY_SI_APP_ID_SECUREIMG),
    .appAttributes = 0UL,                          /* Reserved */
    .numCores      = 1UL,                          /* Only CM0+ */
    .core0Vt       = CY_SI_VT_OFFSET,              /* CM0+ VT offset */
    .core0Id       = CY_SI_CPUID | CY_SI_CORE_IDX, /* CM0+ core ID */
};
/** Secure Image Digital signature (Populated by cymcuelftool) */
CY_SECTION(".cy_app_signature")
__USED CY_ALIGN(4) const uint8_t cy_si_appSignature[CY_SI_SECURE_DIGSIG_SIZE] = {0u};
