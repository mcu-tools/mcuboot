#ifndef H_BOOTUTIL_HWRNG_H_
#define H_BOOTUTIL_HWRNG_H_

#include "ignore.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <mcuboot_config/mcuboot_config.h>

#ifdef MCUBOOT_HAVE_HWRNG
#include <mcuboot_config/mcuboot_rng.h>

#define BOOT_RNG(...) MCUBOOT_RNG(__VA_ARGS__)

#else

#define BOOT_RNG(...) IGNORE(__VA_ARGS__)

#endif /* MCUBOOT_HAVE_RNG */

#ifdef __cplusplus
}
#endif

#endif
