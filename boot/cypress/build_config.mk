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