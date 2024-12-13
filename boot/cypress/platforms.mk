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

# supported platforms list
PSOC_06X := PSOC_061_2M PSOC_061_1M PSOC_061_512K PSOC_062_2M PSOC_062_1M PSOC_062_512K PSOC_063_1M
CYW_xx829 := CYW20829 CYW89829
XMC7000 := XMC7200 XMC7100
PLATFORMS := $(PSOC_06X) $(CYW_xx829) $(XMC7000)

ifneq ($(filter $(PLATFORM), $(PLATFORMS)),)
else
    $(error Not supported platform: '$(PLATFORM)')
endif

ifeq ($(PLATFORM), $(filter $(PLATFORM), $(PSOC_06X)))
    FAMILY := PSOC6
else ifeq ($(PLATFORM), CYW20829)
    FAMILY := CYW20829
else ifeq ($(PLATFORM), CYW89829)
    FAMILY := CYW20829
else ifeq ($(PLATFORM), $(filter $(PLATFORM), $(XMC7000)))
    FAMILY := XMC7000
endif

# include family related makefile into build
include platforms/$(FAMILY).mk

DEFINES += -D$(PLATFORM)
DEFINES += -D$(FAMILY)
