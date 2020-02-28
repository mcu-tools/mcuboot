/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 * Copyright (c) 2020 Cypress Semiconductor Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 /*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
 /*******************************************************************************/

#ifndef CY_FLASH_PSOC6_H_
#define CY_FLASH_PSOC6_H_

#include "stddef.h"
#include "stdbool.h"

#ifndef off_t
typedef long int off_t;
#endif

int psoc6_flash_read(off_t addr, void *data, size_t len);
int psoc6_flash_write(off_t addr, const void *data, size_t len);
int psoc6_flash_erase(off_t addr, size_t size);

int psoc6_flash_write_hal(uint8_t data[],
                            uint32_t address,
                            uint32_t len);
#endif /* CY_FLASH_PSOC6_H_ */
