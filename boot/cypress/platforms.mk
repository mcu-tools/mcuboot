################################################################################
# \file platforms.mk
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

include host.mk

# Target platform is built for. PSOC_062_2M is set by default
# Supported:
#   - PSOC_062_2M
#   - PSOC_062_1M
#   - PSOC_062_512K

# default PLATFORM
PLATFORM ?= PSOC_062_2M

# supported platforms
PLATFORMS := PSOC_062_2M PSOC_062_1M PSOC_062_512K

ifneq ($(filter $(PLATFORM), $(PLATFORMS)),)
else
$(error Not supported platform: '$(PLATFORM)')
endif

# For which core this application is built
CORE ?= CM0P

# MCU device selection, based on target device.
# Default chips are used for supported platforms
# This can be redefined in case of other chip usage
ifeq ($(PLATFORM), PSOC_062_2M)
# base kit CY8CPROTO-062-4343W
DEVICE ?= CY8C624ABZI-D44
PLATFORM_SUFFIX := 02
else ifeq ($(PLATFORM), PSOC_062_1M)
# base kit CY8CKIT-062-WIFI-BT
DEVICE ?= CY8C6247BZI-D54
PLATFORM_SUFFIX := 01
else ifeq ($(PLATFORM), PSOC_062_512K)
# base kit CY8CPROTO-062S3-4343W
DEVICE ?= CY8C6245LQI-S3D72
PLATFORM_SUFFIX := 03
endif

# Add device name to defines
DEFINES += $(DEVICE)
DEFINES += $(PLATFORM)

# Convert defines to regular -DMY_NAME style
ifneq ($(DEFINES),)
	DEFINES_PLATFORM :=$(addprefix -D, $(subst -,_,$(DEFINES)))
endif

ifeq ($(MAKEINFO) , 1)
$(info $(PLATFORM_SUFFIX))
$(info $(DEVICE))
$(info $(DEFINES_PLATFORM))
endif
