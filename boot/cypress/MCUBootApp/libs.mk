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

include platforms.mk

PDL_VERSION = 121

THIS_APP_PATH = $(PRJ_DIR)/libs
MBEDTLS_PATH = $(PRJ_DIR)/../../ext

# Add watchdog folder to build
SOURCES_WATCHDOG := $(wildcard $(THIS_APP_PATH)/watchdog/*.c)

# Add retartget IO implementation using pdl
SOURCES_RETARGET_IO_PDL := $(PLATFORM_SOURCES_RETARGET_IO_PDL)

# Collect dirrectories containing headers for PLATFORM
INCLUDE_RETARGET_IO_PDL := $(PLATFORM_INCLUDE_RETARGET_IO_PDL)

# PSOC6HAL source files
SOURCES_HAL_MCUB := $(PLATFORM_SOURCES_HAL_MCUB)

# PSOC6HAL include dirs
INCLUDE_DIRS_HAL_MCUB := $(PLATFORM_INCLUDE_DIRS_HAL_MCUB)

# MbedTLS source files
SOURCES_MBEDTLS := $(wildcard $(MBEDTLS_PATH)/mbedtls/library/*.c)

# Collected source files for libraries
SOURCES_LIBS += $(SOURCES_MBEDTLS)
SOURCES_LIBS += $(SOURCES_WATCHDOG)

# Collect source files for platform dependent libraries
SOURCES_LIBS += $(SOURCES_HAL_MCUB)
SOURCES_LIBS += $(SOURCES_RETARGET_IO_PDL)

# Watchdog related includes
INCLUDE_DIRS_WATCHDOG := $(THIS_APP_PATH)/watchdog

# MbedTLS related include directories
INCLUDE_DIRS_MBEDTLS += $(MBEDTLS_PATH)/mbedtls/include
INCLUDE_DIRS_MBEDTLS += $(MBEDTLS_PATH)/mbedtls/include/mbedtls
INCLUDE_DIRS_MBEDTLS += $(MBEDTLS_PATH)/mbedtls/include/psa
INCLUDE_DIRS_MBEDTLS += $(MBEDTLS_PATH)/mbedtls/library

# Collected include directories for libraries
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_WATCHDOG))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_MBEDTLS))

# Collect platform dependent include dirs
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_HAL_MCUB))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_RETARGET_IO_PDL))
