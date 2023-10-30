function(mathup num align result)
  math(EXPR out "(((${num}) + ((${align}) - 1)) & ~((${align}) - 1))")
  set(${result} "${out}" PARENT_SCOPE)
endfunction()

function(${SYSBUILD_CURRENT_MODULE_NAME}_pre_image_cmake)
  cmake_parse_arguments(POST_IMAGE_CMAKE "" "IMAGE" "IMAGES" ${ARGN})

  if(NOT "${POST_IMAGE_CMAKE_IMAGE}" STREQUAL "mcuboot")
    return()
  endif()

  set_property(
    DIRECTORY APPEND PROPERTY
    CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_BINARY_DIR}/mcuboot/CMakeCache.txt
    ${CMAKE_BINARY_DIR}/mcuboot/zephyr/.config
    )
endfunction(${SYSBUILD_CURRENT_MODULE_NAME}_pre_image_cmake)

function(${SYSBUILD_CURRENT_MODULE_NAME}_post_image_cmake)
  cmake_parse_arguments(POST_IMAGE_CMAKE "" "IMAGE" "IMAGES" ${ARGN})

  if(NOT "${POST_IMAGE_CMAKE_IMAGE}" STREQUAL "mcuboot")
    return()
  endif()

  foreach(image ${IMAGES})
    set(app_type)
    get_property(app_type TARGET ${image} PROPERTY APP_TYPE)

    if("${app_type}" STREQUAL "MAIN")
      sysbuild_get(mcuboot_image_footer_size IMAGE mcuboot CACHE)
      math(EXPR mcuboot_image_footer_size "${mcuboot_image_footer_size}" OUTPUT_FORMAT HEXADECIMAL)

      set_property(TARGET ${image} APPEND_STRING PROPERTY CONFIG "CONFIG_ROM_END_OFFSET=${mcuboot_image_footer_size}\n")
      return()
    endif()
  endforeach()
endfunction(${SYSBUILD_CURRENT_MODULE_NAME}_post_image_cmake)
