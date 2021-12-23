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

# supported platforms
PLATFORMS := PSOC_062_2M PSOC_062_1M PSOC_062_512K

ifneq ($(filter $(PLATFORM), $(PLATFORMS)),)
else
$(error Not supported platform: '$(PLATFORM)')
endif

ifeq ($(PLATFORM), $(filter $(PLATFORM), PSOC_062_2M PSOC_062_1M PSOC_062_512K))
FAMILY := PSOC6
endif

# include family related makefile into build
include platforms/$(FAMILY)/$(FAMILY).mk

DEFINES += $(PLATFORM)
DEFINES += $(FAMILY)

# Convert defines to regular -DMY_NAME style
ifneq ($(DEFINES),)
	PLATFORM_DEFINES :=$(addprefix -D, $(subst -,_,$(DEFINES)))
endif

ifeq ($(MAKEINFO) , 1)
$(info #### platforms.mk ####)
$(info PLATFORM_SUFFIX: $(PLATFORM_SUFFIX))
$(info FAMILY: $(FAMILY))
$(info PLATFORM: $(PLATFORM))
$(info PLATFORM_DEFINES: $(PLATFORM_DEFINES))
endif
