# Introduction
## Purpose
## Scope
## Overview
## Definitions
# References
# System Overview
# Logical View
## MCUboot-to-Loaded Executable Interface
### Measured boot
- The MCUboot executable shall report data shared with the loaded image in one memory block of fixed location and size.

- The MCUboot executable shall encode content in the Shared Data area using a type-length-value format with:

  - An unsigned 16-bit entry type field
  - An unsigned 16-bit entry length field

- The MCUboot executable shall begin the Shared Data with a TLV header where:

  - the TLV header type is a specified, reserved value indicating the beginning of the Shared Data block
  - the TLV header length indicates the byte length of valid data within the Shared Data block.

- When the MCUboot executable transitions execution to the loaded image,
the MCUboot shared data block pading and unused memory shall be zeros.

# Process View
# Development View
# Physical View
# Scenarios