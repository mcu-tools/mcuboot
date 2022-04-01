/* Copyright 2022, Infineon Technologies AG.  All rights reserved.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef EXEC_TIME_CHECK_H
#define EXEC_TIME_CHECK_H

#include "timebase_us.h"

/*******************************************************************************
* Macro Definition: EXEC_TIME_CHECK_BEGIN
* Macro Definition: EXEC_TIME_CHECK_END
****************************************************************************//**
*
* \param result
* Execution time result in micro seconds. Shall be declared as 32 bit.
*
* \funcusage
* {
*   uint32_t time;
*
*   EXEC_TIME_CHECK_BEGIN(&time);
*       CyDelayUs(1000);
*   EXEC_TIME_CHECK_END();
*
*   printf("%"PRIu32"\n", time);
* }
*
*******************************************************************************/
#define EXEC_TIME_CHECK_BEGIN(result)                           \
    do {                                                        \
        uint32_t* const exec_check_res = (result);              \
        uint32_t exec_check_start = timebase_us_get_tick()

#define EXEC_TIME_CHECK_END()                                           \
        *exec_check_res = timebase_us_get_tick() - exec_check_start;    \
    } while(false)


#endif /* EXEC_TIME_CHECK_H */