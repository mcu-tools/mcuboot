/*
 * Copyright (c) 2021 Infineon Technologies AG
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if !defined(CY_SERVICE_APP)
#define CY_SERVICE_APP

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(MCUBOOT_HW_ROLLBACK_PROT)

#include <stdint.h>

#ifndef SERVICE_APP_DESC_SIZE
#define SERVICE_APP_DESC_SIZE           0x14
#endif /* SERVICE_APP_DESC_SIZE */

#ifndef SERVICE_APP_SIZE
#error "Size of service application is undefined!"
#endif /* SERVICE_APP_SIZE */

#define CYBOOT_REQUEST_EXT_APP          (3UL)

/*
 * Service application regions in external flash
 *
0x60070000  -------------------------
            |                       |
            |                       |
            |   Service App Binary  |
            |                       |
            |                       |
            |                       |
            |                       |
0x60078000  -------------------------
            |                       |
            |   Service App Input   |
            |         Params        |
            |                       |
0x60078400  -------------------------
            |                       |
            |     Service App       |
            |   Descriptor Addr     |
            |                       |
0x60078420  -------------------------
*/

#ifndef SERVICE_APP_OFFSET
#error "Service application offset is undefined!"
#endif

#ifndef SERVICE_APP_INPUT_PARAMS_OFFSET
#error "Service application input parameters offset is undefined!"
#endif

#ifndef SERVICE_APP_DESC_OFFSET
#error "Service application descriptor offset is undefined!"
#endif

/*
 *  0x00    SERVICE_APP_DESCR_SIZE  Service application descriptor object size,
 *                                  includes size entry (hardcoded value 20 bytes).
 *  0x04    SERVICE_APP_ADDR        Start address of service application in the external memory (offset).
 *  0x08    SERVICE_APP_SIZE        Service application image size.
 *  0x0C    INPUT_PARAM_ADDR        Address of input parameters for service application in the
 *                                  external memory (offset).
 *  0x10    INPUT_PARAM_SIZE        Input parameters size.
*/
typedef struct
{
    uint32_t service_app_descr_size;
    uint32_t service_app_addr;
    uint32_t service_app_size;
    uint32_t input_param_addr;
    uint32_t input_param_size;
} service_app_desc_type;

/*
* Function initilizes data required for service app execution and triggers system reset
* to initiate service app execution
*/
void call_service_app(uint8_t * reprov_packet);

/*
* Returns completion status of the service application
*/
int32_t check_service_app_status(void);

#endif /* MCUBootApp */

#if defined(__cplusplus)
}
#endif

#endif /* MCUBOOT_HW_ROLLBACK_PROT */
