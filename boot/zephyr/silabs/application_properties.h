/***************************************************************************//**
 * @file
 * @brief Representation of Application Properties
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc.  Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement.  This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#ifndef APPLICATION_PROPERTIES_H
#define APPLICATION_PROPERTIES_H

#include <stdint.h>

/***************************************************************************//**
 * @addtogroup Interface
 * @{
 * @addtogroup ApplicationProperties Application Properties
 * @brief Properties of the application that can be accessed by the bootloader
 * @details
 *   Applications must contain an @ref ApplicationProperties_t struct declaring
 *   the application version and capabilities, and so on. The metadata contained
 *   in this struct will be extracted from the application by the Simplicity
 *   Commander tool and placed in the GBL upgrade file. If this struct is not
 *   in the application image, it will be added to the GBL file by the
 *   Simplicity Commander.
 *
 *   The struct is also used to declare whether the application image is signed
 *   and what type of signature is used. If no @ref ApplicationProperties_t
 *   struct is present, the bootloader will assume that the application image
 *   is signed using @ref APPLICATION_SIGNATURE_ECDSA_P256.
 *
 *   To ensure that the bootloader can easily locate the ApplicationProperties_t
 *   struct, if not already done by the linker, Simplicity Commander will modify
 *   word 13 of the application to insert a pointer to the
 *   ApplicationProperties_t struct.
 * @{
 ******************************************************************************/

/// Magic value declaring the existence of an ApplicationProperties_t struct
#define APPLICATION_PROPERTIES_MAGIC     { \
    0x13, 0xb7, 0x79, 0xfa,                \
    0xc9, 0x25, 0xdd, 0xb7,                \
    0xad, 0xf3, 0xcf, 0xe0,                \
    0xf1, 0xb6, 0x14, 0xb8                 \
}

/// Byte-reversed version of ::APPLICATION_PROPERTIES_MAGIC
#define APPLICATION_PROPERTIES_REVERSED  { \
    0xb8, 0x14, 0xb6, 0xf1,                \
    0xe0, 0xcf, 0xf3, 0xad,                \
    0xb7, 0xdd, 0x25, 0xc9,                \
    0xfa, 0x79, 0xb7, 0x13                 \
}

/// Major version number of the AppliationProperties_t struct
#define APPLICATION_PROPERTIES_VERSION_MAJOR (1UL)
/// Minor version number of the AppliationProperties_t struct
#define APPLICATION_PROPERTIES_VERSION_MINOR (2UL)
/// Version number of the ApplicationCertificate_t struct
#define APPLICATION_CERTIFICATE_VERSION  (1UL)
/// The application is not signed
#define APPLICATION_SIGNATURE_NONE       (0UL)
/// @brief The SHA-256 digest of the application is signed using ECDSA with the
///        NIST P-256 curve.
#define APPLICATION_SIGNATURE_ECDSA_P256 (1UL << 0UL)
/// @brief The application is not signed, but has a CRC-32 checksum
#define APPLICATION_SIGNATURE_CRC32      (1UL << 1UL)

/// The application contains a Zigbee wireless stack
#define APPLICATION_TYPE_ZIGBEE          (1UL << 0UL)
/// The application contains a Thread wireless stack
#define APPLICATION_TYPE_THREAD          (1UL << 1UL)
/// The application contains a Flex wireless stack
#define APPLICATION_TYPE_FLEX            (1UL << 2UL)
/// The application contains a Bluetooth wireless stack
#define APPLICATION_TYPE_BLUETOOTH       (1UL << 3UL)
/// The application is an MCU application
#define APPLICATION_TYPE_MCU             (1UL << 4UL)
/// The application contains a Bluetooth application
#define APPLICATION_TYPE_BLUETOOTH_APP   (1UL << 5UL)
/// The application contains a bootloader
#define APPLICATION_TYPE_BOOTLOADER      (1UL << 6UL)
/// The application contains a Zwave wireless stack
#define APPLICATION_TYPE_ZWAVE           (1UL << 7UL)

/// Application Data
typedef struct ApplicationData {
  /// @brief Bitfield representing type of application, e.g.,
  /// @ref APPLICATION_TYPE_ZIGBEE
  uint32_t type;
  /// Version number for this application
  uint32_t version;
  /// Capabilities of this application
  uint32_t capabilities;
  /// Unique ID (UUID or GUID) for the product this application is built for
  uint8_t productId[16];
} ApplicationData_t;

/// Application Certificate
typedef struct ApplicationCertificate {
  /// Version of the certificate structure
  uint8_t structVersion;
  /// Reserved flags
  uint8_t flags[3];
  /// Public key
  uint8_t key[64];
  /// The version number of this certificate
  uint32_t version;
  /// Signature of the certificate
  uint8_t signature[64];
} ApplicationCertificate_t;

/// Application Properties struct
typedef struct {
  /// @brief Magic value indicating this is an ApplicationProperties_t struct.
  /// Must equal @ref APPLICATION_PROPERTIES_MAGIC
  uint8_t magic[16];
  /// Version number of this struct
  uint32_t structVersion;
  /// Type of signature this application is signed with
  uint32_t signatureType;
  /// Location of the signature. Typically points to the end of the application
  uint32_t signatureLocation;
  /// Information about the application
  ApplicationData_t app;
  /// Pointer to information about the certificate
  ApplicationCertificate_t *cert;
  /// Pointer to Long Token Data Section
  uint8_t *longTokenSectionAddress;
  /// Parser Decryption Key
  const uint8_t decryptKey[16];
} ApplicationProperties_t;

/** @} (end addtogroup ApplicationProperties) */
/** @} (end addtogroup Interface) */

/// Application Properties major version shift value
#define APPLICATION_PROPERTIES_VERSION_MAJOR_SHIFT (0U)
/// Application Properties minor version shift value
#define APPLICATION_PROPERTIES_VERSION_MINOR_SHIFT (8U)

/// Application Properties major version mask
#define APPLICATION_PROPERTIES_VERSION_MAJOR_MASK (0x000000FFU)
/// Application Properties minor version mask
#define APPLICATION_PROPERTIES_VERSION_MINOR_MASK (0xFFFFFF00U)

/// Version number of the AppliationProperties_t struct
#define APPLICATION_PROPERTIES_VERSION ((APPLICATION_PROPERTIES_VERSION_MINOR           \
                                         << APPLICATION_PROPERTIES_VERSION_MINOR_SHIFT) \
                                        | (APPLICATION_PROPERTIES_VERSION_MAJOR         \
                                           << APPLICATION_PROPERTIES_VERSION_MAJOR_SHIFT))

#if (APPLICATION_PROPERTIES_VERSION_MAJOR \
     > (APPLICATION_PROPERTIES_VERSION_MAJOR_MASK >> APPLICATION_PROPERTIES_VERSION_MAJOR_SHIFT)) \
|| (APPLICATION_PROPERTIES_VERSION_MINOR \
    > (APPLICATION_PROPERTIES_VERSION_MINOR_MASK >> APPLICATION_PROPERTIES_VERSION_MINOR_SHIFT))
#error "Invalid application properties version"
#endif

#endif // APPLICATION_PROPERTIES_H
