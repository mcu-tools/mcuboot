# Copyright (c) 2022 Legrand North America, LLC.
#
# SPDX-License-Identifier: Apache-2.0


cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

add_subdirectory_ifdef(CONFIG_MCUBOOT_BOOTUTIL_LIB mcuboot_util)
add_subdirectory_ifdef(CONFIG_MCUBOOT mcuboot_svc)
