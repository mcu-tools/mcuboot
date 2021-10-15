cmake_minimum_required(VERSION 3.10.2)
project(mcuboot-selfhosted
	DESCRIPTION "Standalone MCUBOOT simulation with MBEDTLS"
	LANGUAGES C
	VERSION 0.0.0)

if (MCUBOOT_ROOT)
else()
	set(MCUBOOT_ROOT ../../)
endif()

if (MBEDTLS_ROOT)
else()
	set(MBEDTLS_ROOT ${MCUBOOT_ROOT}/ext/mbedtls)
endif()

set(CMAKE_C_FLAGS "-O0 -g")

include_directories(${MCUBOOT_ROOT}/boot/bootutil/include)
include_directories(${MBEDTLS_ROOT}/include)
include_directories(${MCUBOOT_ROOT}/boot/selfhosted/include)

set(SOURCE
	${MCUBOOT_ROOT}/boot/selfhosted/src/main.c
	${MCUBOOT_ROOT}/boot/selfhosted/src/app_enc_keys.c
	${MCUBOOT_ROOT}/boot/selfhosted/src/flash_map_backend.c
	${MCUBOOT_ROOT}/boot/selfhosted/src/mcuboot.c
	${MCUBOOT_ROOT}/boot/bootutil/src/loader.c
	${MCUBOOT_ROOT}/boot/bootutil/src/swap_move.c
	${MCUBOOT_ROOT}/boot/bootutil/src/swap_scratch.c
	${MCUBOOT_ROOT}/boot/bootutil/src/bootutil_public.c
	${MCUBOOT_ROOT}/boot/bootutil/src/bootutil_misc.c
	${MCUBOOT_ROOT}/boot/bootutil/src/image_validate.c
	${MCUBOOT_ROOT}/boot/bootutil/src/tlv.c
	${MCUBOOT_ROOT}/boot/bootutil/src/image_ec256.c
	${MCUBOOT_ROOT}/boot/bootutil/src/swap_misc.c
)

add_definitions(-DMCUBOOT_SIGNATURE_ALGORITHM=SIGNATURE_TYPE_EC256)
add_definitions(-DMCUBOOT_LOG_LEVEL=3)
add_definitions(-DMCUBOOT_SWAP_USING_SCRATCH=1)
add_definitions(-DMCUBOOT_HAVE_LOGGING=1)

link_directories(${MBEDTLS_ROOT}/build/library)

add_executable(bootme ${SOURCE})

target_link_libraries(bootme mbedcrypto)

