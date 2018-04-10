/*
 * Copyright (c) 2017 Intel Corporation
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

/**
 * @file
 * @brief Bootloader device specific configuration.
 */

/*
 * NOTE: This flash layout is a simple one for demonstration purposes.
 * It assumes we are building MCUboot as a 3rd-stage loader, leaving the
 * stock bootloaders on the Arduino 101 untouched.
 *
 * In this configuration MCUboot will live at 0x40010000
 */

#define FLASH_DEV_NAME			CONFIG_SOC_FLASH_QMSI_DEV_NAME
#define FLASH_ALIGN			4
#define FLASH_AREA_IMAGE_0_OFFSET	0x40020000
#define FLASH_AREA_IMAGE_0_SIZE		0x10000
#define FLASH_AREA_IMAGE_1_OFFSET	0x40030000
#define FLASH_AREA_IMAGE_1_SIZE		0x10000
#define FLASH_AREA_IMAGE_SCRATCH_OFFSET	0x40040000
#define FLASH_AREA_IMAGE_SCRATCH_SIZE	0x10000
