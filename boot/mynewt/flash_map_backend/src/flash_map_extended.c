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

#include <flash_map/flash_map.h>
#include <flash_map_backend/flash_map_backend.h>

int flash_area_id_from_multi_image_slot(int image_index, int slot)
{
    switch (slot) {
    case 0: return FLASH_AREA_IMAGE_PRIMARY(image_index);
    case 1: return FLASH_AREA_IMAGE_SECONDARY(image_index);
    case 2: return FLASH_AREA_IMAGE_SCRATCH;
    }
    return 255;
}

int flash_area_id_to_multi_image_slot(int image_index, int area_id)
{
    if (area_id == FLASH_AREA_IMAGE_PRIMARY(image_index)) {
        return 0;
    }
    if (area_id == FLASH_AREA_IMAGE_SECONDARY(image_index)) {
        return 1;
    }
    return 255;
}
