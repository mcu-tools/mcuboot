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
 * implemented to overide or to amend some of MCUboot's native routines.
 */

#ifndef H_BOOTUTIL_HOOKS
#define H_BOOTUTIL_HOOKS

#ifdef MCUBOOT_IMAGE_ACCESS_HOOKS

#define BOOT_HOOK_CALL(f, ret_default, ...) f(__VA_ARGS__)

#define BOOT_HOOK_CALL_FIH(f, fih_ret_default, fih_rc, ...) \
    do { \
        FIH_CALL(f, fih_rc, __VA_ARGS__); \
    } while(0);

#else

#define BOOT_HOOK_CALL(f, ret_default, ...) ret_default

#define BOOT_HOOK_CALL_FIH(f, fih_ret_default, fih_rc, ...) \
    do { \
        fih_rc = fih_ret_default; \
    } while(0);

#endif

/** Hook for provide image header data.
 *
 * This Hook may be used to overide image header read implementation or doing
 * a custom action before.
 *
 * @param img_index the index of the image pair
 * @param slot slot number
 * @param img_head image header structure to be populated
 *
 * @retval 0: header was read/populated, skip direct header data read
 *         BOOT_HOOK_REGULAR: follow the normal execution path,
 *         otherwise an error-code value.
 */
int boot_read_image_header_hook(int img_index, int slot,
                                struct image_header *img_head);

/** Hook for Validate image hash/signature
 *
 * This Hook may be used to overide image validation procedure or doing
 * a custom action before.
 *
 * @param img_index the index of the image pair
 * @param slot slot number
 * 
 * @retval FIH_SUCCESS: image is valid, skip direct validation
 *         FIH_FAILURE: image is invalid, skip direct validation
 *         fih encoded BOOT_HOOK_REGULAR: follow the normal execution path.
 */
fih_int boot_image_check_hook(int img_index, int slot);

/** Hook for implement image update
 *
 * This hook is for for implementing an alternative mechanism of image update or
 * doing a custom action before.
 *
 * @param img_index the index of the image pair
 * @param img_head the image header of the secondary image
 * @param area the flash area of the secondary image.
 *
 * @retval 0: update was done, skip performing the update
 *         BOOT_HOOK_REGULAR: follow the normal execution path,
 *         otherwise an error-code value.
 */
int boot_perform_update_hook(int img_index, struct image_header *img_head,
                             const struct flash_area *area);

/** Hook for implement image's post copying action
 *
 * This hook is for implement action which might be done right after image was
 * copied to the primary slot. This hook is called in MCUBOOT_OVERWRITE_ONLY
 * mode only.
 *
 * @param img_index the index of the image pair
 * @param area the flash area of the primary image.
 * @param size size of copied image.
 *
 * @retval 0: success, mcuboot will follow normal code execution flow after
 *            execution of this call.
 *         non-zero: an error, mcuboot will return from
 *         boot_copy_image() with error.
 *         Update will be undone so might be resume on the next boot.
 */
int boot_copy_region_post_hook(int img_index, const struct flash_area *area,
                               size_t size);

/** Hook for implement image's post recovery upload action
 *
 * This hook is for implement action which might be done right after image was
 * copied to the primary slot. This hook is called in serial recovery upload
 * operation.
 *
 * @param img_index the index of the image pair
 * @param area the flash area of the primary image.
 * @param size size of copied image.
 *
 * @retval 0: success, mcuboot will follow normal code execution flow after
 *            execution of this call.
 *         non-zero: an error, will be transferred as part of comand response
 *            as "rc" entry.
 */
int boot_serial_uploaded_hook(int img_index, const struct flash_area *area,
                              size_t size);

/** Hook for implement the image's slot installation status fetch operation for
 *  the MGMT custom command.
 *
 * The image's slot installation status is custom property. It's detailed
 * definition depends on user implementation. It is only defined that the status
 * will be set to 0 if this hook not provides another value.
 *
 * @param img_index the index of the image pair
 * @param slot slot number
 * @param img_install_stat the image installation status to be populated
 *
 * @retval 0: the installaton status was fetched successfully,
 *         BOOT_HOOK_REGULAR: follow the normal execution path, status will be
 *         set to 0
 *         otherwise an error-code value. Error-code is ignored, but it is up to
 *         the implementation to reflect this error in img_install_stat.
 */
int boot_img_install_stat_hook(int image_index, int slot,
                               int *img_install_stat);


/** Hook for getting installation progress.
 *
 * @param img_index the index of the image pair
 * @param slot slot number
 *
 * @retval 0 when idle;
 *         [1,99] when in progress;
 *         100 when done;
 *         [-100,-1] when failed, representing percentage at which
 *             process has failed;
 *          <= -101 when no status is available.
 *
 * Returning value below -100 means that hook does not want caller to
 * process returned value. This is useful, for example, when boot logic
 * has chosen path where there will be no installation performed and
 * so no reporting is required.
 * Within one upload session hook may start with returning value below -100
 * and then change to returning progress, but it may not do opposite: starting
 * with returning progress and then deciding to case providing such information
 * is considered an error.
 */
int boot_img_install_progress_hook(int image_index, int slot);

#endif /*H_BOOTUTIL_HOOKS*/
