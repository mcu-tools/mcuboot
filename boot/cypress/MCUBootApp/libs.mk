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

ifneq ($(FAMILY), XMC7000)
# Add watchdog folder to build
SOURCES_WATCHDOG := $(wildcard $(THIS_APP_PATH)/watchdog/*.c)
# Watchdog related includes
INCLUDE_DIRS_WATCHDOG := $(THIS_APP_PATH)/watchdog
endif

# PSOC6HAL source files
SOURCES_HAL_MCUB := $(PLATFORM_SOURCES_HAL_MCUB)

# PSOC6HAL include dirs
INCLUDE_DIRS_HAL_MCUB := $(PLATFORM_INCLUDE_DIRS_HAL_MCUB)

# MbedTLS source files
SOURCES_MBEDTLS := $(wildcard $(MBEDTLS_PATH)/mbedtls/library/*.c)

# Collected source files for libraries
SOURCES_LIBS += $(SOURCES_MBEDTLS)
SOURCES_LIBS += $(SOURCES_WATCHDOG)
SOURCES_LIBS += $(SOURCES_FIH)

# Collect source files for platform dependent libraries
SOURCES_LIBS += $(SOURCES_HAL_MCUB)

# MbedTLS related include directories
ifeq ($(USE_CRYPTO_HW), 1)
ifeq ($(PLATFORM), CYW20829)
# Override mbedtls/compat-2.x.h for Cryptolite CBUS workaround
INCLUDE_DIRS_MBEDTLS += $(PRJ_DIR)/platforms/crypto/CYW20829
endif
endif
INCLUDE_DIRS_MBEDTLS += $(MBEDTLS_PATH)/mbedtls/include
INCLUDE_DIRS_MBEDTLS += $(MBEDTLS_PATH)/mbedtls/include/mbedtls
INCLUDE_DIRS_MBEDTLS += $(MBEDTLS_PATH)/mbedtls/include/psa
INCLUDE_DIRS_MBEDTLS += $(MBEDTLS_PATH)/mbedtls/library

# Collected include directories for libraries
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_WATCHDOG))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_MBEDTLS))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_FIH))

# Collect platform dependent include dirs
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_HAL_MCUB))

###############################################################################
# Print debug information about all settings used and/or set in this file
ifeq ($(VERBOSE), 1)
$(info #### libs.mk ####)
$(info INCLUDE_DIRS_FIH <-> $(INCLUDE_DIRS_FIH))
$(info INCLUDE_DIRS_HAL_MCUB <-> $(INCLUDE_DIRS_HAL_MCUB))
$(info INCLUDE_DIRS_LIBS <-> $(INCLUDE_DIRS_LIBS))
$(info INCLUDE_DIRS_MBEDTLS <-> $(INCLUDE_DIRS_MBEDTLS))
$(info INCLUDE_DIRS_WATCHDOG <-> $(INCLUDE_DIRS_WATCHDOG))
$(info MBEDTLS_PATH <-- $(MBEDTLS_PATH))
$(info PLATFORM <-- $(PLATFORM))
$(info PLATFORM_INCLUDE_DIRS_HAL_MCUB <-- $(PLATFORM_INCLUDE_DIRS_HAL_MCUB))
$(info PLATFORM_SOURCES_HAL_MCUB <-- $(PLATFORM_SOURCES_HAL_MCUB))
$(info PRJ_DIR <-- $(PRJ_DIR))
$(info SOURCES_FIH <-> $(SOURCES_FIH))
$(info SOURCES_HAL_MCUB <-> $(SOURCES_HAL_MCUB))
$(info SOURCES_LIBS <-> $(SOURCES_LIBS))
$(info SOURCES_MBEDTLS <-> $(SOURCES_MBEDTLS))
$(info SOURCES_WATCHDOG <-> $(SOURCES_WATCHDOG))
$(info THIS_APP_PATH <-- $(THIS_APP_PATH))
$(info USE_CRYPTO_HW <-- $(USE_CRYPTO_HW))
endif
