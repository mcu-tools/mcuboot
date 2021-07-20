cmake_minimum_required(VERSION 3.13)

if (NOT DEFINED MCUBOOT_TARGET)
    message(FATAL_ERROR "MCUBOOT_TARGET not defined. Please pass -DMCUBOOT_TARGET flag.")
endif()

project(mcuboot_${MCUBOOT_TARGET})

if (NOT DEFINED IDF_PATH)
    if (DEFINED ENV{IDF_PATH})
        set(IDF_PATH $ENV{IDF_PATH})
    else()
        message(FATAL_ERROR "IDF_PATH not found. Please set IDF_PATH environment variable or pass -DIDF_PATH flag.")
    endif()
endif()

execute_process(
    COMMAND git describe --tags
    WORKING_DIRECTORY ${IDF_PATH}
    OUTPUT_VARIABLE IDF_VER
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if (NOT "${IDF_VER}" MATCHES "v4.3")
    message(FATAL_ERROR "Unsupported ESP-IDF version found in IDF_PATH, please checkout to v4.3")
endif()

if (DEFINED MCUBOOT_CONFIG_FILE)
    set(mcuboot_config_file ${MCUBOOT_CONFIG_FILE})
else()
    set(mcuboot_config_file "${CMAKE_CURRENT_LIST_DIR}/bootloader.conf")
endif()

if (NOT EXISTS "${mcuboot_config_file}")
    message(FATAL_ERROR "MCUboot configuration file does not exist at ${mcuboot_config_file}")
endif()

configure_file(${mcuboot_config_file} dummy.conf)
file(STRINGS ${mcuboot_config_file} BOOTLOADER_CONF)
foreach(config ${BOOTLOADER_CONF})
    if (NOT (${config} MATCHES "#"))
        string(REGEX REPLACE "^[ ]+" "" config ${config})
        string(REGEX MATCH "^[^=]+" CONFIG_NAME ${config})
        string(REPLACE "${CONFIG_NAME}=" "" CONFIG_VALUE ${config})
        add_definitions(-D${CONFIG_NAME}=${CONFIG_VALUE})
    endif()
endforeach()

set(APP_NAME mcuboot_${MCUBOOT_TARGET})
set(APP_EXECUTABLE ${APP_NAME}.elf)

set(MCUBOOT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
set(BOOTUTIL_DIR ${MCUBOOT_ROOT_DIR}/boot/bootutil)
set(MBEDTLS_DIR ${MCUBOOT_ROOT_DIR}/ext/mbedtls)

set(bootutil_srcs
    ${BOOTUTIL_DIR}/src/boot_record.c
    ${BOOTUTIL_DIR}/src/bootutil_misc.c
    ${BOOTUTIL_DIR}/src/bootutil_public.c
    ${BOOTUTIL_DIR}/src/caps.c
    ${BOOTUTIL_DIR}/src/encrypted.c
    ${BOOTUTIL_DIR}/src/fault_injection_hardening.c
    ${BOOTUTIL_DIR}/src/fault_injection_hardening_delay_rng_mbedtls.c
    ${BOOTUTIL_DIR}/src/image_ec.c
    ${BOOTUTIL_DIR}/src/image_ec256.c
    ${BOOTUTIL_DIR}/src/image_ed25519.c
    ${BOOTUTIL_DIR}/src/image_rsa.c
    ${BOOTUTIL_DIR}/src/image_validate.c
    ${BOOTUTIL_DIR}/src/loader.c
    ${BOOTUTIL_DIR}/src/swap_misc.c
    ${BOOTUTIL_DIR}/src/swap_move.c
    ${BOOTUTIL_DIR}/src/swap_scratch.c
    ${BOOTUTIL_DIR}/src/tlv.c
    )

set(mbedtls_srcs
    ${MBEDTLS_DIR}/library/sha256.c
    ${MBEDTLS_DIR}/library/platform_util.c
    )

set(CFLAGS
    "-mlongcalls"
    "-Wno-frame-address"
    "-Wall"
    "-Wextra"
    "-W"
    "-Wdeclaration-after-statement"
    "-Wwrite-strings"
    "-Wlogical-op"
    "-Wshadow"
    "-ffunction-sections"
    "-fdata-sections"
    "-fstrict-volatile-bitfields"
    "-Werror=all"
    "-Wno-error=unused-function"
    "-Wno-error=unused-but-set-variable"
    "-Wno-error=unused-variable"
    "-Wno-error=deprecated-declarations"
    "-Wno-unused-parameter"
    "-Wno-sign-compare"
    "-ggdb"
    "-Os"
    "-D_GNU_SOURCE"
    "-std=gnu99"
    "-Wno-old-style-declaration"
    "-Wno-implicit-int"
    "-Wno-declaration-after-statement"
    )

set(LDFLAGS
    "-nostdlib"
    "-mlongcalls"
    "-Wno-frame-address"
    "-Wl,--cref"
    "-Wl,--Map=${APP_NAME}.map"
    "-fno-rtti"
    "-fno-lto"
    "-Wl,--gc-sections"
    "-Wl,--undefined=uxTopUsedPriority"
    "-lm"
    "-lgcc"
    "-lgcov"
    "-lstdc++"
    "-lc"
    )

add_executable(
    ${APP_EXECUTABLE}
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    )

target_compile_options(
    ${APP_EXECUTABLE}
    PUBLIC
    ${CFLAGS}
    )

target_sources(
    ${APP_EXECUTABLE}
    PUBLIC
    ${bootutil_srcs}
    ${mbedtls_srcs}
    ${CMAKE_CURRENT_LIST_DIR}/port/esp_mcuboot.c
    )

target_include_directories(
    ${APP_EXECUTABLE}
    PUBLIC
    ${BOOTUTIL_DIR}/include
    ${MBEDTLS_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/include
    )

target_link_libraries(
    ${APP_EXECUTABLE}
    PUBLIC
    -T${CMAKE_CURRENT_LIST_DIR}/port/${MCUBOOT_TARGET}/ld/bootloader.ld
    ${LDFLAGS}
    )
