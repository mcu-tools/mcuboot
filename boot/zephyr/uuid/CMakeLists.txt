#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

if(CONFIG_MCUBOOT_UUID_SINGLE_VID)
  if(CONFIG_MCUBOOT_UUID_VID OR CONFIG_MCUBOOT_UUID_CID)
    zephyr_library_sources(
      uuid.c
      )
  endif()

  # Generate VID value and raw value definition
  if(CONFIG_MCUBOOT_UUID_VID OR CONFIG_MCUBOOT_UUID_CID)
    if("${CONFIG_MCUBOOT_UUID_VID_VALUE}" STREQUAL "" AND CONFIG_MCUBOOT_UUID_VID)
      message(WARNING "VID value not set")
      return()
    endif()

    string(REGEX MATCHALL "^([0-9A-F][0-9A-F]|\-)+$" match_parts "${CONFIG_MCUBOOT_UUID_VID_VALUE}")
    if("${match_parts}" STREQUAL "${CONFIG_MCUBOOT_UUID_VID_VALUE}")
      set(UUID_VID ${match_parts})
    else()
      set(UUID_DNS_NAMESPACE 6ba7b810-9dad-11d1-80b4-00c04fd430c8)
      string(
        UUID UUID_VID
        NAMESPACE ${UUID_DNS_NAMESPACE}
        NAME ${CONFIG_MCUBOOT_UUID_VID_VALUE}
        TYPE SHA1 UPPER
      )
    endif()

    if(CONFIG_MCUBOOT_UUID_VID)
      # Convert UUID into C array.
      string(REGEX REPLACE "([0-9A-F][0-9A-F])\-?" "0x\\1, " UUID_VID_RAW "${UUID_VID}")
      zephyr_compile_definitions(MCUBOOT_UUID_VID_VALUE=${UUID_VID_RAW})
    endif()
  endif()

  # Generate VID value(s) and raw value definition(s)
  if(CONFIG_MCUBOOT_UUID_CID)
    set(MCUBOOT_IMAGES_COUNT ${CONFIG_UPDATEABLE_IMAGE_NUMBER})
    foreach(image_id RANGE ${MCUBOOT_IMAGES_COUNT})
      if(CONFIG_MCUBOOT_UUID_CID_IMAGE_${image_id})
        if("${CONFIG_MCUBOOT_UUID_CID_IMAGE_${image_id}_VALUE}" STREQUAL "")
          message(WARNING "CID value not set for image ${image_id}")
          return()
        endif()

        # Check if RAW UUID format is used
        string(REGEX MATCHALL "^([0-9A-F][0-9A-F]|\-)+$" match_parts "${CONFIG_MCUBOOT_UUID_CID_IMAGE_${image_id}_VALUE}")
        if("${match_parts}" STREQUAL "${CONFIG_MCUBOOT_UUID_CID_IMAGE_${image_id}_VALUE}")
          set(UUID_CID_IMAGE_${image_id} ${match_parts})
        elseif(NOT "${UUID_VID}" STREQUAL "")
          # If not - generate UUID based on VID and CID values
          string(
            UUID UUID_CID_IMAGE_${image_id}
            NAMESPACE ${UUID_VID}
            NAME ${CONFIG_MCUBOOT_UUID_CID_IMAGE_${image_id}_VALUE}
            TYPE SHA1 UPPER
          )
        else()
          message(WARNING "VID value not set, cannot generate CID for image ${image_id}")
          return()
        endif()

        # Convert UUID into C array.
        string(REGEX REPLACE "([0-9A-F][0-9A-F])\-?" "0x\\1, " UUID_CID_IMAGE_${image_id}_RAW "${UUID_CID_IMAGE_${image_id}}")
        zephyr_compile_definitions(MCUBOOT_UUID_CID_IMAGE_${image_id}_VALUE=${UUID_CID_IMAGE_${image_id}_RAW})
      endif()
    endforeach()
  endif()
endif()
