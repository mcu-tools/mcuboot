#ifndef H_MCUBOOT_CONFIG_
#define H_MCUBOOT_CONFIG_

// This header file acts as an adapter layer from Kconfig-style
//
//   '#define CONFIG_foo'
//
// to MCUBoot-style
//
//   '#define foo'
//
// macros. This file is manually maintained for now.
//
// The adapter layer keeps MCUBoot semi-agnostic to Zephyr and
// MyNewt's configuration systems. See MyNewt's adapter in it's own
// mcuboot_config.h file.

#ifdef CONFIG_MCUBOOT_VALIDATE_SLOT0
#define MCUBOOT_VALIDATE_SLOT0
#endif

#endif /* H_MCUBOOT_CONFIG_ */
