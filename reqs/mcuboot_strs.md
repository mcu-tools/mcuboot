# Introduction
This document contains the Stakeholder Requirements Specification (StRS) for the MCUboot Project.

It is organized based on ISO/IEC/IEEE 29148:2018 Systems and software engineering - Life cycle processes - Requirements engineering, section 8.3.2 StRS example outline.

Section 1 provides an introduction to the purpose, scope, and context of the MCUboot Project and its deliverables.

Section 2 lists referenced material.

Section 3 provides a more detailed description of the business-imposed requirements on the MCUboot Project and its deliverables.

Section 4 captures the requirements on the operation of the MCUboot Project ant its deliverables.

Section 5 captures user needs and requirements users face within the problem domain.

Section 6 captures a _conceptual_ description of the proposed system.

Section 7 captures any further constraints imposed on performing the MCUboot Project (e.g., cost, schedule).

## Stakeholder Purpose

## Stakeholder Scope
The MCUboot Project seeks to produce, develop and maintain an open-source reference secure bootloader for microcontroller-based systems.

The scope of the MCUboot Project is a secure boot for 32-bit Microcontrollers.

## Overview
The scope of the MCUboot Project includes usage in the following domains:

  - RTOS ecosystem providers
    - OSS Projects
    - Commercial Vendors
      - MCU/SOC chip vendors
      - Commercial RTOS providers
  - MCU-based Product integrators
    - Commercial
    - Education
    - FOSS

Additional stakeholders are introduced with the focus on secure boot:

  - Regulatory Agencies (including safety-critical agencies)
  - Standards Certification Businesses

## Stakeholders

### Business Roles

### Marketing Roles

### Regulatory Authorities

### Engineering Presonas

### Manufacturing Personas

### User Personas

#### UserPersona - Template
ame:  Given name, job title

Description:

- "Optional quote"
- Age:
- Job:
- Income:
- Education:
- Family:
- Location:
- Hobbies/Interests

Confidence Rating:

Responsibilities:

- Responsibility 1
- Responsibility 2

Pain Points:

- Pain point 1
- Pain point 2

Goals:

- Goal 1

Influences:

- Influence 1
- Influence 2

Technology:

- Technical Level
- Tech Used 1
- Tech Used 2

#### Inaya, Firmware Integration Engineer
Description:

- "Optional quote"
- Age: 31
- Job: IoT Device Firmware Developer
- Income:
- Education: College
- Family:
- Location:
- Hobbies/Interests:

Confidence Rating: Medium

Responsibilities:

A typical day for Inaya is spent integrating and sustaining integration of 10+ closely related products developed over the last 5 years. Each product requires an immutable boot loader and an application image. Some may require multi-stage boot. She is responsible for the integration of each of these, and for sustainment of integrations as security issues are discovered and addressed, and application features are enhanced.

Pain Points:

- Instability of interfaces and configuration points.
- Finding and resolving feature enhancements bundled with necessary security fixes.
- Having to use a different integration/verification process or tools for different sets of products.
- Having to integrate, verify and release security fixes for the entire set of products within a couple weeks.

Goals:

- Getting the job done right the first time so security, safety, and/or feature issues do not escape to the field.
- Having thorough objective evidence that immutable boot code is complete, correct, and meets the security standard chosen by her business.

Influences:

- The Engineering architect setting technical direction for the business.
- The open source projects being integrated into her product.


Technology:

- Technical Level: Proficient
- Uses Linux and Windows for development
- Uses JTAG-based tools for device firmware debugging 


# References

# Business Management Requirements

## Business Environment

## Mission, Goals, and Objective
The goal of MCUboot is to define a common infrastructure for the bootloader, system flash layout on microcontroller systems, and to provide a secure bootloader that enables easy software upgrade. 

The misssion of the MCUboot Project is to produce, develop amdn maintin an open-source reference for secure bootloader for microcontroller-based systems.

## Business Model
The issues being planned and worked on are tracked using GitHub issues. To participate please visit MCUboot Issues on github.

Companies and other organisations who adopt, deploy or contribute to MCUboot are invited to join the MCUboot community project. Membership of the project is open to all and governance is overseen by a board of Member representatives.

To join the project, a company needs to sign and return a copy of the Membership Agreement and pay an annual fee. 

Memberhsip of the project is independent of membership of Linaro. Linaro provides governance, hosting and other services. 

The MCUboot Project Charter is located at https://www.mcuboot.com/project-charter. It covers the following items:

1. Mission of the MCUboot Project.
2. Membership
3. Project Governing Board
4. Conduct of Board Meetings
5. Quorum and Voting
6. Antitrust Guidelines
7. Budget
8. Treasury and Project Expenses
9. Project IP Policy and Licensing
10. Technical Steering Committee("TSC")
11: Amendments

## Information Environment


# System Operation Requirements

# User Requirements

## Inaya, Firmware Integration Engineer
- As a Firmware Integration Engineer,
I need the bootloader build system to generate a BOM for the boot image,
so that I have a publicizable artifact for security investigations.

- As a Firmware Integration Engineer,
I need to verify the compatibility of source licensing of all source files needed in the resulting executable image,
so that my business is not exposed to unknown licensing liabilities.

- As a Firmware Integration Engineer for a safety-critical device, I need to configure and generate a bootloader image with all the auditable artifacts needed for the safety certification chosen by my business, so that I can sell my product.

- As a Firmware Integration Engineer for a regulated secure device, I need to configure and generate a bootloader image with all the auditable artifacts needed for the security certification chosen by my business, so that I can sell my product.

- As a Firmware Integration Engineer for a regulated device, I need to re-verify all used open source implementations with my replacement qualified toolchain, so that my product can meet certification criteria.

- As a Firmware Integration Engineer, I need to quickly and easily retarget my bootloader feature configuration at replacement hardware,
so I can rapidly and predictably deliver a bootloader to the manufacturing line for the replacement MCU.

- As a Firmware Integration Engineer,
I need the bootloader to expose metadata and status about itself to the loaded image,
so that the loaded image can report this data out for diagnostics.

- As a Firmware Integration Engineer,
I need the bootloader to have the MCU hardware in a well-defined condition upon handoff to the loaded image,
so the loaded image can avoid unnecessary and/or undesirable hardware reconfiguration.

- As a Firmware Integration Engineer,
I need the application image to validate candidate FW updates in the same way as the boot image will,
so that candidate images can be invalidated without having to reboot.

- As a Firmware Integration Engineer of a FW image spread across multiple flash parts (e.g., executable, discontiguous file system),
I need the bootloader to atomically update and validate an image spread across discontiguous flash partitions,
so the entire application image can be updated.

- As a Firmware Integration Engineer of a FW image spread across multiple flash parts (e.g., executable, discontiguous file system),
I need the bootloader to only update image partitions based on dependencies,
so that update time can be reduced by skipping unnecessary partition updates.

- As a Firmware Integration Engineer for inaccessibly located devices,
I need the bootloader to be resilient to power outages,
so that my device does not die due to an untimely power loss.

- As a Firmware Integration Engineer using MCUs that do not support overlapped execution from the flash being programmed,
I need to configure the bootloader to run from RAM,
so the bootloader can apply the update.

- As a Firmware Integration Engineer using MCU with a lockable boot flash partition too small for my full set of boot functionality,
I need the bootloader source to be configurable as a minimal, immutable first stage image and configurable as a full-featured second-stage bootloader image,
so the MCU constraint can boot my application.

- As a Firmware Integration Engineer for devices secured with cryptography,
I need the bootloader to be configurable to use the standard cryptographic algorithms of my choosing,
so that my device uses algorithms appropriate to my product domain.

- As a Firmware Integration Engineer,
I need to configure the bootloader with a serial upgrade capability,
so I can more easily diagnose integration issues on new MCUs and board designs.

- As a Firmware Integration Engineer,
I need the bootloader to drive a serial interface of my choice,
so I can perform serial recovery over the available hardware. 

- As a Firmware Integration Engineer for devices in visble locations,
I need the bootloader to drive a User Interface of my specification,
so that I can visually distinguish between a device with no valid application image, a device that has finished booting and hung in the application, and a device that is applying an update.

- As a Firmware Integration Engineer,
I need the bootloader to persistently log security events,
so I can perform boot and update failure analysis on a returned device.

- As a Firmware Integration Engineer of consumer devices,
I need the bootloader to prevent downgrading the version of application firmware,
so the device will prevent re-opening security holes discovered in downgraded versions.

- As a Firmware Integration Engineer of commercial/industrial devices,
I need the bootloader to allow downgrading the version of application firmware,
so the existing devices at the customer installation used in system update evaluations can be returned to their prior condition.

- As a Firmware Integration Engineer,
I need to configure the bootloader so the boot image is single-threaded,
so that the image is as simple (and small) as possible.

- As a Firmware Integration Engineer,
I need to configure the bootloader so the boot image is multi-threaded,
so that the image can reuse RTOS drivers for complex subsystems (e.g., USB UART device). 

- As a Firmware Integration Engineer for a regulated device,
I need to produce process documentation that demonstrates that the product
was developed according to a qualified process,
so that the product can pass a process audit.

# Detailed Life-cycle Concepts of Proposed System

# Project Constraints

# Validations