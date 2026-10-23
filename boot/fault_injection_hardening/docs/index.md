# Fault Injection Hardening Library Documentation

Welcome to the Fault Injection Hardening (FIH) Library documentation.

## Quick Start

- **[README](../README.md)** — Overview, features, and basic usage examples
- **[Security Policy](SECURITY.md)** — How to report security vulnerabilities
- **[Contributing](contributing.md)** — How to contribute to the project
- **[Submitting Patches](SubmittingPatches.md)** — Guidelines for submitting code changes

## Features

The FIH library provides four configurable profiles for protecting code against fault injection attacks:

- **HIGH**: Full protections (delay, double-vars, global fail, CFI)
- **MEDIUM**: Double-vars, global fail, CFI (no delay)
- **LOW**: Global fail and CFI only
- **OFF**: No protections

## Integration

The library is designed to be:
- **Standalone**: Can be used independently outside MCUboot
- **Configurable**: Profiles can be set via `fih_config.h`
- **Portable**: Architecture and compiler independent

## Building

See the top-level [CMakeLists.txt](../CMakeLists.txt) for standalone compilation.

When integrated into MCUboot, FIH is built as part of the bootutil library.

## Testing

The library is validated through:
- **Integration Tests**: Part of MCUboot's CI with fault injection simulation
- **Multi-compiler Testing**: Verified with GCC, Clang, IAR, and other compilers
- **Multiple Platforms**: Tested across various ARM and other architectures

See [SubmittingPatches](SubmittingPatches.md#testing) for testing requirements.

## More Information

- [MCUboot Project](https://github.com/mcu-tools/mcuboot)
- [Original Design and Development](../README.md#references)
