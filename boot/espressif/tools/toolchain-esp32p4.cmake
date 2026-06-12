# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

set(CMAKE_SYSTEM_NAME Generic)

set(CMAKE_C_COMPILER riscv32-esp-elf-gcc)
set(CMAKE_CXX_COMPILER riscv32-esp-elf-g++)
set(CMAKE_ASM_COMPILER riscv32-esp-elf-gcc)

set(_march "rv32imafc_zicsr_zifencei_zaamo_zalrsc_zba_zbb_zbs_zcb_zcmp_zcmt_xesploop_xespv")
set(_mflags "-march=${_march} -mabi=ilp32f -mno-cm-popret")

set(CMAKE_C_FLAGS "${_mflags}" CACHE STRING "C Compiler Base Flags")
set(CMAKE_CXX_FLAGS "${_mflags}" CACHE STRING "C++ Compiler Base Flags")
set(CMAKE_EXE_LINKER_FLAGS "-nostartfiles ${_mflags} --specs=nosys.specs" CACHE STRING "Linker Base Flags")
