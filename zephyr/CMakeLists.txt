# Copyright (c) 2022 Legrand North America, LLC.
#
# SPDX-License-Identifier: Apache-2.0


cmake_minimum_required(VERSION 3.10 FATAL_ERROR)


# Detect the platform reliably
if(NOT ZEPHYR_BASE)
  message(FATAL_ERROR "ZEPHYR_BASE needs to be defined for Zephyr builds")
endif()

# Verify the module name is set correctly
set(expected_module_name "mcuboot")
if(NOT "${module_name}" STREQUAL ${expected_module_name})
  message(FATAL_ERROR "
    ------------------------------------------------------------------------
    A module configuration error has occurred.
    
    This module is named '${module_name}' when '${expected_module_name}' is expected. 
    Verify `module.yml` contains the line 'name: ${expected_module_name}'.
    ------------------------------------------------------------------------")
endif()
unset(expected_module_name)

# Do not allow in source builds
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)


#
# Module options managed through Kconfig and use names CONFIG_*
#

zephyr_include_directories(../boot/zephyr/include)

add_subdirectory(subsys)
