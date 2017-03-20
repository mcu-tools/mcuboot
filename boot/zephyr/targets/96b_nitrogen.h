/*
 * Copyright (c) 2017 Linaro
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

#define FLASH_DRIVER_NAME		CONFIG_SOC_FLASH_NRF5_DEV_NAME
#define FLASH_ALIGN			4
#define FLASH_AREA_IMAGE_0_OFFSET	0x08000
#define FLASH_AREA_IMAGE_0_SIZE		0x34000
#define FLASH_AREA_IMAGE_1_OFFSET	0x3C000
#define FLASH_AREA_IMAGE_1_SIZE		0x34000
#define FLASH_AREA_IMAGE_SCRATCH_OFFSET	0x70000
#define FLASH_AREA_IMAGE_SCRATCH_SIZE	0x0D000
/* Flash sector size is provided by SoC family include */
