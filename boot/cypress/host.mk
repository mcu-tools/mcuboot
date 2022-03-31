################################################################################
# \file host.mk
# \version 1.0
#
# \brief
# Makefile to describe host environment for Cypress MCUBoot based applications.
#
################################################################################
# \copyright
# Copyright 2018-2021 Cypress Semiconductor Corporation
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

# Detect host OS to make resolving compiler pathes easier
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Darwin)
	HOST_OS = osx
else
	ifeq ($(UNAME_S), Linux)
		HOST_OS = linux
	else
		HOST_OS = win
	endif
endif

ifeq ($(HOST_OS), win)
    define get_os_path
$(shell cygpath -m $(1))
    endef
else
    define get_os_path
$(1)
    endef
endif

PRJ_DIR=$(call get_os_path, $(CURDIR))

###############################################################################
# Print debug information about all settings used and/or set in this file
ifeq ($(VERBOSE), 1)
$(info #### host.mk ####)
$(info CURDIR <-- $(CURDIR))
$(info HOST_OS <-- $(HOST_OS))
$(info UNAME_S <-> $(UNAME_S))
endif
