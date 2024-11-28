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
THIS_APP_PATH = $(PRJ_DIR)/libs
MBEDTLS_PATH = $(PRJ_DIR)/../../ext

# PSOC6HAL include dirs

# MbedTLS source files
C_FILES += $(wildcard $(MBEDTLS_PATH)/mbedtls/library/*.c)

# MbedTLS related include directories
ifeq ($(USE_CRYPTO_HW), 1)
ifeq ($(FAMILY), CYW20829)
    # Override mbedtls/compat-2.x.h for Cryptolite CBUS workaround
    INCLUDE_DIRS += $(PRJ_DIR)/platforms/crypto/CYW20829
endif
endif

INCLUDE_DIRS += $(MBEDTLS_PATH)/mbedtls/include
INCLUDE_DIRS += $(MBEDTLS_PATH)/mbedtls/include/mbedtls
INCLUDE_DIRS += $(MBEDTLS_PATH)/mbedtls/include/psa
INCLUDE_DIRS += $(MBEDTLS_PATH)/mbedtls/library


