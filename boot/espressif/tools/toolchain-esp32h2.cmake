# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

set(CMAKE_SYSTEM_NAME Generic)

set(CMAKE_C_COMPILER riscv32-esp-elf-gcc)
set(CMAKE_CXX_COMPILER riscv32-esp-elf-g++)
set(CMAKE_ASM_COMPILER riscv32-esp-elf-gcc)

set(CMAKE_C_FLAGS "-march=rv32imac_zicsr_zifencei_zaamo_zalrsc" CACHE STRING "C Compiler Base Flags")
set(CMAKE_CXX_FLAGS "-march=rv32imac_zicsr_zifencei_zaamo_zalrsc" CACHE STRING "C++ Compiler Base Flags")
set(CMAKE_EXE_LINKER_FLAGS "-nostartfiles -march=rv32imac_zicsr_zifencei_zaamo_zalrsc --specs=nosys.specs" CACHE STRING "Linker Base Flags")
