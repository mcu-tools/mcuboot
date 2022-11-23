# Introduction
This document contains the OS System Element Requirements Specification (OSRS) for the MCUboot Project.

It is organized based on ISO/IEC/IEEE 29148:2018 Systems and software engineering - Life cycle processes - Requirements engineering, section 8.4.2 SyRS example outline.

Section 1 provides an introduction to the purpose, scope, and context of the MCUboot Project and its deliverables.

Section 2 lists referenced material.

Section 3 captures the formal requirements on the operation of the system element (system element requirements) to meet the assigned system requirements and architecture definition constraints.

Section 4 captures how the resulting system element will be verified that it meets all the assigned system element requirements.

## OS System Element Purpose
The Operating System (OS) system element provides basic abstractions for building executables, scheduling work, and sharing resources among different threads/interrupts.

## OS System Element Scope
The OS System Element provides the following capabilities, as needed, to the MCUboot executable:

- Extensible build and linking system
- Extensible configuration system
- Extensible and replaceable toolchain
- Primitives supporting preemptive and/or cooperative multi-threading.
- Automated test framework and test runner

## OS System Element Overview
### Context
### Functions
### User Characteristics
## Definitions
# References
# Requirements
## Functional
- Where automated verification of compatibility of source licensing is available,
when automated verification of compatibility of source licensing is requested,
the build system of the provided ported MCUboot solution shall verify the compatibility of source licensing of all source files needed in the resulting executable image.

## Usability
## Performance
## Interface
## System Operations
## System Modes and States
## Physical Charactteristics
## Environmental Conditions
## Security
## Information Management
## Policy and Regulations
## System Life Cycle Sustainment
## Packaging, Handling, Shipping and Transportation
# Verifications
## Functional
## Usability
## Performance
## Interface
## System Operations
## System Modes and States
## Physical Charactteristics
## Environmental Conditions
## Security
## Information Management
## Policy and Regulations
## System Life Cycle Sustainment
## Packaging, Handling, Shipping and Transportation
# Appendices
## Assumptions and Dependencies
## Acronyms and Abbreviations