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

#ifndef __BOOT_SERIAL_H__
#define __BOOT_SERIAL_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Function pointers to read/write data from uart.
 * read returns the number of bytes read, str points to buffer to fill,
 *  cnt is the number of bytes to fill within buffer, *newline will be
 *  set if newline is the last character.
 * write takes as it's arguments pointer to data to write, and the count
 *  of bytes.
 */
struct boot_uart_funcs {
    int (*read)(char *str, int cnt, int *newline);
    void (*write)(const char *ptr, int cnt);
};

/**
 * Start processing newtmgr commands for uploading image0 over serial.
 * Assumes serial port is open and waits for download command.
 */
void boot_serial_start(const struct boot_uart_funcs *f);

/**
 * Start processing newtmgr commands for uploading image0 over serial.
 * Assumes serial port is open and waits for download command.
 * This function will return if there is no echo command received within
 * the given timeout. If an echo command is received within this timeout, the
 * function is similar to boot_serial_start
 */
void boot_serial_check_start(const struct boot_uart_funcs *f,int32_t timeout_in_ms);

#ifdef __cplusplus
}
#endif

#endif /*  __BOOT_SERIAL_H__ */
