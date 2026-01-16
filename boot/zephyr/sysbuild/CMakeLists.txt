#
# Copyright (c) 2023-2026 Nordic Semiconductor
#
# SPDX-License-Identifier: Apache-2.0
#

function(mathup num align result)
  math(EXPR out "(((${num}) + ((${align}) - 1)) & ~((${align}) - 1))")
  set(${result} "${out}" PARENT_SCOPE)
endfunction()

function(${SYSBUILD_CURRENT_MODULE_NAME}_pre_cmake)
  cmake_parse_arguments(PRE_CMAKE "" "" "IMAGES" ${ARGN})

  # Search through all images and identify any that are MCUboot images - these must have "mcuboot"
  # in the name somewhere, then double check they are MCUboot by comparing the source directory
  # of the image. Once found, add the CMake cache output and zephyr config for the image to the
  # sysbuild dependency list so that sysbuild will re-configure if they are changed (as image
  # data is passed on to the main image for image signing purposes).
  if(SB_CONFIG_BOOTLOADER_MCUBOOT)
    set(mcuboot_dir ${ZEPHYR_MCUBOOT_MODULE_DIR}/boot/zephyr)
    cmake_path(NATIVE_PATH mcuboot_dir NORMALIZE mcuboot_dir_normalized)

    foreach(image ${PRE_CMAKE_IMAGES})
      if(image MATCHES mcuboot)
        get_property(footer_update_images TARGET ${image} PROPERTY MCUBOOT_FOOTER_UPDATE_IMAGES)

        if(footer_update_images)
          ExternalProject_Get_Property(${image} SOURCE_DIR)
          cmake_path(NATIVE_PATH SOURCE_DIR NORMALIZE source_dir_normalized)

          if(${source_dir_normalized} STREQUAL ${mcuboot_dir_normalized})
            set_property(
              DIRECTORY APPEND PROPERTY
              CMAKE_CONFIGURE_DEPENDS
              ${CMAKE_BINARY_DIR}/${image}/CMakeCache.txt
              ${CMAKE_BINARY_DIR}/${image}/zephyr/.config
            )
          endif()
        endif()
      endif()
    endforeach()
  endif()
endfunction(${SYSBUILD_CURRENT_MODULE_NAME}_pre_cmake)

function(${SYSBUILD_CURRENT_MODULE_NAME}_post_image_cmake)
  cmake_parse_arguments(POST_IMAGE_CMAKE "" "IMAGE" "IMAGES" ${ARGN})

  if(SB_CONFIG_BOOTLOADER_MCUBOOT AND image MATCHES mcuboot)
    set(mcuboot_dir ${ZEPHYR_MCUBOOT_MODULE_DIR}/boot/zephyr)
    cmake_path(NATIVE_PATH mcuboot_dir NORMALIZE mcuboot_dir_normalized)
    get_property(footer_update_images TARGET ${image} PROPERTY MCUBOOT_FOOTER_UPDATE_IMAGES)

    if(footer_update_images)
      ExternalProject_Get_Property(${image} SOURCE_DIR)
      cmake_path(NATIVE_PATH SOURCE_DIR NORMALIZE source_dir_normalized)

      if(${source_dir_normalized} STREQUAL ${mcuboot_dir_normalized})
        sysbuild_get(mcuboot_image_footer_size IMAGE ${image} CACHE)
        sysbuild_get(mcuboot_image_upgrade_footer_size IMAGE ${image} CACHE)
        math(EXPR mcuboot_image_footer_size "${mcuboot_image_footer_size}" OUTPUT_FORMAT HEXADECIMAL)
        math(EXPR mcuboot_image_upgrade_footer_size "${mcuboot_image_upgrade_footer_size}" OUTPUT_FORMAT HEXADECIMAL)

        foreach(update_image ${footer_update_images})
          set_config_int(${update_image} CONFIG_ROM_END_OFFSET ${mcuboot_image_footer_size})
          set_config_int(${update_image} CONFIG_MCUBOOT_UPDATE_FOOTER_SIZE ${mcuboot_image_upgrade_footer_size})
        endforeach()
      endif()
    endif()
  endif()
endfunction(${SYSBUILD_CURRENT_MODULE_NAME}_post_image_cmake)
