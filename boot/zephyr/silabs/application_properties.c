/***************************************************************************//**
 * @file
 * @brief Application Properties Source File
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "application_properties.h"

#ifdef APP_PROPERTIES_CONFIG_FILE
#include APP_PROPERTIES_CONFIG_FILE
#else
#include "app_properties_config.h"
#endif

const ApplicationProperties_t sl_app_properties = {
  .magic = APPLICATION_PROPERTIES_MAGIC,
  .structVersion = APPLICATION_PROPERTIES_VERSION,
  .signatureType = SL_APPLICATION_SIGNATURE,
  .signatureLocation = SL_APPLICATION_SIGNATURE_LOCATION,
  .app = {
    .type = SL_APPLICATION_TYPE,
    .version = SL_APPLICATION_VERSION,
    .capabilities = SL_APPLICATION_CAPABILITIES,
    .productId = SL_APPLICATION_PRODUCT_ID
  },
  .cert = 0,
  .longTokenSectionAddress = 0,
  .decryptKey = { 0 }
};
