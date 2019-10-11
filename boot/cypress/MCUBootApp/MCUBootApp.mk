################################################################################
# \file targets.mk
# \version 1.0
#
# \brief
# Makefile to describe supported boards and platforms for Cypress MCUBoot based applications.
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

# Cypress' MCUBoot Application supports GCC ARM only at this moment 
# Set default compiler to GCC if not specified from command line
COMPILER ?= GCC_ARM
BUILDCFG ?= Debug

ifneq ($(COMPILER), GCC_ARM)
$(error Only GCC ARM is supported at this moment)
endif

CUR_APP_PATH = $(CURDIR)/$(APP_NAME)

include $(CUR_APP_PATH)/targets.mk
include $(CUR_APP_PATH)/libs.mk
include $(CUR_APP_PATH)/toolchains.mk

# Application-specific DEFINES
DEFINES_APP := -DMBEDTLS_CONFIG_FILE="\"crypto_config_sw.h\""
#DEFINES_APP += -DMCUBOOT_APP_DEF
#DEFINES_APP += 

# TODO: MCUBoot library
SOURCES_MCUBOOT := $(wildcard $(CURDIR)/../bootutil/src/*.c)

SOURCES_APP := $(wildcard $(CUR_APP_PATH)/*.c)
SOURCES_APP += $(wildcard $(CURDIR)/cy_flash_pal/*.c)
#SOURCES_APP += $(SOURCES_MCUBOOT)

#INCLUDES_DIRS_MCUBOOT := $(addprefix -I, $(CURDIR)/../bootutil/include)
#INCLUDES_DIRS_MCUBOOT += $(addprefix -I, $(CURDIR)/../bootutil/src)

INCLUDE_DIRS_APP := $(addprefix -I, $(CURDIR))
INCLUDE_DIRS_APP += $(addprefix -I, $(CURDIR)/cy_flash_pal/include)
INCLUDE_DIRS_APP += $(addprefix -I, $(CUR_APP_PATH))
INCLUDE_DIRS_APP += $(addprefix -I, $(CUR_APP_PATH)/config)
INCLUDE_DIRS_APP += $(addprefix -I, $(CUR_APP_PATH)/os)

ASM_FILES_APP :=