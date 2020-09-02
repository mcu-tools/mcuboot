################################################################################
# \file libs.mk
# \version 1.0
#
# \brief
# Makefile to describe libraries needed for Cypress MCUBoot based applications.
#
################################################################################
# \copyright
# Copyright 2018-2019 Cypress Semiconductor Corporation
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################

################################################################################
# PDL library
################################################################################
PDL_VERSION = 121

CUR_LIBS_PATH = $(CURDIR)/libs
MBEDTLS_PATH = $(CURDIR)/../../ext

# Collect source files for PDL
SOURCES_PDL := $(wildcard $(CUR_LIBS_PATH)/pdl/psoc6pdl/drivers/source/*.c)
SOURCES_WATCHDOG := $(wildcard $(CUR_LIBS_PATH)/watchdog/*.c)

# PDL related include directories
INCLUDE_DIRS_PDL := $(CUR_LIBS_PATH)/pdl/psoc6pdl/drivers/include
INCLUDE_DIRS_PDL += $(CUR_LIBS_PATH)/pdl/psoc6pdl/devices/include/ip
INCLUDE_DIRS_PDL += $(CUR_LIBS_PATH)/pdl/psoc6pdl/devices/include
INCLUDE_DIRS_PDL += $(CUR_LIBS_PATH)/pdl/psoc6pdl/cmsis/include

# core-libs related include directories
INCLUDE_DIRS_CORE_LIB := $(CUR_LIBS_PATH)/core-lib/include
INCLUDE_DIRS_WATCHDOG := $(CUR_LIBS_PATH)/watchdog

SOURCES_HAL += $(CUR_LIBS_PATH)/psoc6hal/COMPONENT_PSOC6HAL/source/cyhal_crypto_common.c
SOURCES_HAL += $(CUR_LIBS_PATH)/psoc6hal/COMPONENT_PSOC6HAL/source/cyhal_hwmgr.c

# Collected source files for libraries
SOURCES_LIBS := $(SOURCES_PDL)
SOURCES_LIBS += $(SOURCES_WATCHDOG)
SOURCES_LIBS += $(SOURCES_PLATFORM)
SOURCES_LIBS += $(SOURCES_HAL)

# needed for Crypto HW Acceleration and headers inclusion, do not use for peripherals
# peripherals should be accessed
INCLUDE_DIRS_HAL := $(CUR_LIBS_PATH)/psoc6hal/COMPONENT_PSOC6HAL/include
INCLUDE_DIRS_HAL += $(CUR_LIBS_PATH)/psoc6hal/include
INCLUDE_DIRS_HAL += $(CUR_LIBS_PATH)/psoc6hal/COMPONENT_PSOC6HAL/include/pin_packages

# Collected include directories for libraries
INCLUDE_DIRS_LIBS := $(addprefix -I,$(INCLUDE_DIRS_PDL))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_PLATFORM))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_CORE_LIB))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_WATCHDOG))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_HAL))

################################################################################
# mbedTLS settings
################################################################################
# MbedTLS related include directories
INCLUDE_DIRS_MBEDTLS += $(MBEDTLS_PATH)/mbedtls/include
INCLUDE_DIRS_MBEDTLS += $(MBEDTLS_PATH)/mbedtls/include/mbedtls
INCLUDE_DIRS_MBEDTLS += $(MBEDTLS_PATH)/mbedtls/crypto/include
INCLUDE_DIRS_MBEDTLS += $(MBEDTLS_PATH)/mbedtls/crypto/include/mbedtls
#
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_MBEDTLS))
# Collect source files for MbedTLS
SOURCES_MBEDTLS := $(wildcard $(MBEDTLS_PATH)/mbedtls/library/*.c)
SOURCES_MBEDTLS += $(wildcard $(MBEDTLS_PATH)/mbedtls/crypto/library/*.c)
# Collected source files for libraries
SOURCES_LIBS += $(SOURCES_MBEDTLS)
## mbedTLS settings

################################################################################
# mbedTLS acceleration settings
################################################################################
ifeq ($(USE_CRYPTO_HW), 1)
# cy-mbedtls-acceleration related include directories
INCLUDE_DIRS_MBEDTLS_MXCRYPTO := $(CUR_LIBS_PATH)/cy-mbedtls-acceleration/mbedtls_MXCRYPTO
# Collect source files for MbedTLS acceleration
SOURCES_MBEDTLS_MXCRYPTO := $(wildcard $(CUR_LIBS_PATH)/cy-mbedtls-acceleration/mbedtls_MXCRYPTO/*.c)
#
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_MBEDTLS_MXCRYPTO))
# Collected source files for libraries
SOURCES_LIBS += $(SOURCES_MBEDTLS_MXCRYPTO)
endif
## mbedTLS acceleration settings

ASM_FILES_PDL :=
ifeq ($(COMPILER), GCC_ARM)
ASM_FILES_PDL += $(CUR_LIBS_PATH)/pdl/psoc6pdl/drivers/source/TOOLCHAIN_GCC_ARM/cy_syslib_gcc.S
else
$(error Only GCC ARM is supported at this moment)
endif

ASM_FILES_LIBS := $(ASM_FILES_PDL)
ASM_FILES_LIBS += $(ASM_FILES_PLATFORM)

# Add define for PDL version
DEFINES_PDL += -DPDL_VERSION=$(PDL_VERSION)

DEFINES_LIBS := $(DEFINES_PLATFORM)
DEFINES_LIBS += $(DEFINES_PDL)
