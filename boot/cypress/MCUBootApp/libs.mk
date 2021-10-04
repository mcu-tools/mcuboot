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

THIS_APP_PATH = $(PRJ_DIR)/libs
MBEDTLS_PATH = $(PRJ_DIR)/../../ext

# Add platform folder to build
SOURCES_PLATFORM += $(wildcard $(PRJ_DIR)/platforms/*.c)
SOURCES_WATCHDOG := $(wildcard $(THIS_APP_PATH)/watchdog/*.c)

# Add retartget IO implementation using pdl
SOURCES_RETARGET_IO_PDL += $(wildcard $(THIS_APP_PATH)/retarget_io_pdl/*.c)

# Collect dirrectories containing headers for PLATFORM
INCLUDE_RETARGET_IO_PDL += $(THIS_APP_PATH)/retarget_io_pdl

# PSOC6HAL source files
SOURCES_HAL += $(THIS_APP_PATH)/psoc6hal/COMPONENT_PSOC6HAL/source/cyhal_crypto_common.c
SOURCES_HAL += $(THIS_APP_PATH)/psoc6hal/COMPONENT_PSOC6HAL/source/cyhal_hwmgr.c

# MbedTLS source files
SOURCES_MBEDTLS := $(wildcard $(MBEDTLS_PATH)/mbedtls/library/*.c)

# Collected source files for libraries
SOURCES_LIBS += $(SOURCES_HAL)
SOURCES_LIBS += $(SOURCES_MBEDTLS)
SOURCES_LIBS += $(SOURCES_WATCHDOG)
SOURCES_LIBS += $(SOURCES_PLATFORM)
SOURCES_LIBS += $(SOURCES_RETARGET_IO_PDL)

# Include platforms folder
INCLUDE_DIRS_PLATFORM := $(PRJ_DIR)/platforms

# needed for Crypto HW Acceleration and headers inclusion, do not use for peripherals
# peripherals should be accessed
INCLUDE_DIRS_HAL := $(THIS_APP_PATH)/psoc6hal/COMPONENT_PSOC6HAL/include
INCLUDE_DIRS_HAL += $(THIS_APP_PATH)/psoc6hal/include
INCLUDE_DIRS_HAL += $(THIS_APP_PATH)/psoc6hal/COMPONENT_PSOC6HAL/include/pin_packages

# MbedTLS related include directories
INCLUDE_DIRS_MBEDTLS += $(MBEDTLS_PATH)/mbedtls/include
INCLUDE_DIRS_MBEDTLS += $(MBEDTLS_PATH)/mbedtls/include/mbedtls
INCLUDE_DIRS_MBEDTLS += $(MBEDTLS_PATH)/mbedtls/include/psa
INCLUDE_DIRS_MBEDTLS += $(MBEDTLS_PATH)/mbedtls/library

# Watchdog related includes
INCLUDE_DIRS_WATCHDOG := $(THIS_APP_PATH)/watchdog

# Collected include directories for libraries
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_HAL))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_WATCHDOG))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_MBEDTLS))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_RETARGET_IO_PDL))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_PLATFORM))

################################################################################
# mbedTLS hardware acceleration settings
################################################################################
ifeq ($(USE_CRYPTO_HW), 1)
# cy-mbedtls-acceleration related include directories
INCLUDE_DIRS_MBEDTLS_MXCRYPTO := $(THIS_APP_PATH)/cy-mbedtls-acceleration/mbedtls_MXCRYPTO
# Collect source files for MbedTLS acceleration
SOURCES_MBEDTLS_MXCRYPTO := $(wildcard $(THIS_APP_PATH)/cy-mbedtls-acceleration/mbedtls_MXCRYPTO/*.c)
#
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_MBEDTLS_MXCRYPTO))
# Collected source files for libraries
SOURCES_LIBS += $(SOURCES_MBEDTLS_MXCRYPTO)
endif
