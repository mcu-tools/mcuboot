/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * Original license:
 *
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

/**
 * @file
 * @brief Hooks definition implementation API
 *
 * This file contains API interface definition for hooks which can be
 * implemented for overide some of MCUboot's native routines.
 */

#ifndef H_BOOTUTIL_HOOKS
#define H_BOOTUTIL_HOOKS

/** Hook for provide image header data.
 *
 * @param img_index the index of the images pair
 * @param slot slot number
 * @param img_hed image header structure to be populated
 *
 * @retval 0: header was read/populated
 *         FIH_FAILURE: image is invalid,
 *         1 if hook not implemented for the image-slot,
 *         othervise an error-code value.
 */
int boot_read_image_header_hook(int img_index, int slot,
                                struct image_header *img_head);

/** Hook for Validate primary image hash/signature
 *
 * @param img_index the index of the images pair
 * 
 * @retval FIH_SUCCESS: image is valid,
 *         FIH_FAILURE: image is invalid,
 *         fih encoded 1 if hook not implemented for the image-slot.
 */
fih_int boot_image_check_hook(int img_index);

/** Hook for implement image update
 *
 * @param img_index the index of the images pair
 * @param img_hed the image header of the secondary image
 * @param area the flash area of the secondary image. 
 *
 * @retval 0: header was read/populated
 *         FIH_FAILURE: image is invalid,
 *         1 if hook not implemented for the image-slot,
 *         othervise an error-code value.
 */
int boot_perform_update_hook(int img_index, struct image_header *img_head,
                             const struct flash_area *area);

#endif /*H_BOOTUTIL_HOOKS*/
