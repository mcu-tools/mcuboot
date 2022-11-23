# Introduction
This document contains the System Requirements Specification (SyRS) for the MCUboot Project.

It is organized based on ISO/IEC/IEEE 29148:2018 Systems and software engineering - Life cycle processes - Requirements engineering, section 8.4.2 SyRS example outline.

Section 1 provides an introduction to the purpose, scope, and context of the MCUboot Project and its deliverables.

Section 2 lists referenced material.

Section 3 captures the formal requirements on the operation of the system (system requirements) to meet the subset stakeholder needs agreed upon by the representative stakeholders.

Section 4 captures how the resulting system will be verified that it meets all the system requirements.

## System Purpose
The MCUboot Project seeks to produce, develop and maintain an open-source reference secure bootloader for microcontroller-based systems.

## System Scope

### Breadth
The scope of the MCUboot Project includes:

1. Source, verification suites, and documentation for:

  - MCUboot functionality and framework interfaces
  - MCUboot port glue code into one or more target OS ecosystems (e.g., Mynewt)

2. Processes and resources for:

  - hosting the MCUboot Project
  - managing the MCUboot Project

### Needs Addressed by MCUboot Project
The MCUboot Project solution will address the following high-level needs:

- TBD

### Needs Addressed by MCUboot Reference
The MCUboot Reference solution will address the following high-level needs:

- Reusable immutable bootloader application
- Reusable mutable bootloader application
- Bootloader runs from flash as execute-in-place image.
- Bootloader runs from RAM.

## System Overview
### System Context
#### System Perspective
#### System Boundaries - Summary
#### System Modeling - General
### System Functions
### User Characteristics
## Definitions
# References
# Requirements
## Functional
## Usability
## Performance
## Interface
### MCUboot Executable Metadata
- The MCUboot executable shall present metadata about itself to the loaded image.

### MCUboot Execution Status
- The MCUboot executable shall present status about itself to the loaded image.

### MCUboot Execution Logs
- The MCUboot executable shall persistently log security events.

## System Operations
## System Modes and States
## Physical Characteristics
## Enironmental Conditions
## Security
## Information Management
## Policy and Regulations
- Where automated verification of compatibility of source licensing is available,
when automated verification of compatibility of source licensing is requested,
the build system of the MCUboot solution shall verify the compatibility of source licensing of all source files needed in the resulting executable image.

## System Life Cycle Sustainment
## Packaging, Handling, Shipping and Transportation
# Verifications
## Functional
## Usability
## Performance
## Interface
## System Operations
## System Modes and States
## Physical Characteristics
## Environmental Conditions
## Security
## Information Management
## Policy and Regulation
## System Life Cycle Sustainment
## Packaging, Handling, Shipping and Transportation
# Appendices
## Assumptions and Dependencies
## Acronyms and Abbreviations