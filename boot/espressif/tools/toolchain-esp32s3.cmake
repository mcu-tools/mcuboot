# SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

set(CMAKE_SYSTEM_NAME Generic)

set(CMAKE_C_COMPILER xtensa-esp32s3-elf-gcc)
set(CMAKE_CXX_COMPILER xtensa-esp32s3-elf-g++)
set(CMAKE_ASM_COMPILER xtensa-esp32s3-elf-gcc)
set(_CMAKE_TOOLCHAIN_PREFIX xtensa-esp32s3-elf-)

set(CMAKE_C_FLAGS "-mlongcalls" CACHE STRING "C Compiler Base Flags" FORCE)
set(CMAKE_CXX_FLAGS "-mlongcalls" CACHE STRING "C++ Compiler Base Flags" FORCE)
set(CMAKE_ASM_FLAGS "${UNIQ_CMAKE_ASM_FLAGS}" CACHE STRING "ASM Compiler Base Flags" FORCE)

set(CMAKE_EXE_LINKER_FLAGS "-Wl,--gc-sections" CACHE STRING "Linker Base Flags")
