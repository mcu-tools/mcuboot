#
# CMakeLists.txt for the Fault Injection Hardening library
#
# This is the top-level CMakeLists.txt for building the FIH library as a
# standalone component. When used within MCUboot, the FIH sources are typically
# integrated into the bootutil library. This file allows FIH to be built and
# tested independently.
#
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2020 Arm Limited
#

cmake_minimum_required(VERSION 3.12)
project(fault_injection_hardening)

# Add the FIH library
add_library(fih STATIC
    src/fault_injection_hardening.c
)

target_include_directories(fih
    PUBLIC
        include
)

# When used outside MCUboot, callers must provide fih_config.h on the include path
# or in their project. The following demonstrates how to add it:
#
# target_include_directories(your_target PRIVATE path/to/your/fih_config.h)

# Enable testing (optional)
enable_testing()

# Add tests if they exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
