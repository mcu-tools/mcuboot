/*
 * Copyright (c) 2025 STMicroelectonics
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

#include "../bootutil/include/bootutil/bootutil.h"
#include "../bootutil/include/bootutil/ramload.h"

#include <zephyr/devicetree.h>

int boot_get_image_exec_ram_info(uint32_t image_id,
                                 uint32_t *exec_ram_start,
                                 uint32_t *exec_ram_size)
{
#ifdef CONFIG_SOC_SERIES_STM32N6X
    *exec_ram_start = DT_PROP_BY_IDX(DT_NODELABEL(axisram1), reg, 0);
    *exec_ram_size = DT_PROP_BY_IDX(DT_NODELABEL(axisram1), reg, 1);
#endif

    return 0;
}
