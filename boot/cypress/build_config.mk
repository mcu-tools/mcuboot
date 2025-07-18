################################################################################
# \copyright
# Copyright 2025 Cypress Semiconductor Corporation
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

# Defines whether or not show verbose build output
VERBOSE ?= 0

# Application name by default
APP_NAME ?= MCUBootApp

# Weather or now execute post build script after build - set to 0 for CI
POST_BUILD_ENABLE ?= 1

# Default number of GCC compilation threads
THREADS_NUM ?= 8

# Build configuration
BUILDCFG ?= Debug

# Defines whether or not make all compile warnings into errors for application
# source code (but not for library source code)
WARN_AS_ERR ?= 1

# Python path definition
ifeq ($(OS), Windows_NT)
    PYTHON_PATH?=python
else
    PYTHON_PATH?=python3
endif

# The cysecuretools path
CY_SEC_TOOLS_PATH ?= $(shell $(PYTHON_PATH) $(CURDIR)/scripts/find_cysectools.py)