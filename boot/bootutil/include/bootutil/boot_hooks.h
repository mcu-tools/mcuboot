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

#include "bootutil/bootutil.h"
#include "bootutil/fault_injection_hardening.h"
#include <limits.h>

#define DO_HOOK_CALL(f, ret_default, ...) \
    f(__VA_ARGS__)

#define DO_HOOK_CALL_FIH(f, fih_ret_default, fih_rc, ...) \
    do { \
        FIH_CALL(f, fih_rc, __VA_ARGS__); \
    } while(0);

#define HOOK_CALL_NOP(f, ret_default, ...) ret_default

#define HOOK_CALL_FIH_NOP(f, fih_ret_default, fih_rc, ...) \
    do { \
        fih_rc = fih_ret_default; \
    } while(0);

#ifdef MCUBOOT_IMAGE_ACCESS_HOOKS

#define BOOT_HOOK_CALL(f, ret_default, ...) \
    DO_HOOK_CALL(f, ret_default, __VA_ARGS__)

#define BOOT_HOOK_CALL_FIH(f, fih_ret_default, fih_rc, ...) \
    DO_HOOK_CALL_FIH(f, fih_ret_default, fih_rc, __VA_ARGS__)

#else

#define BOOT_HOOK_CALL(f, ret_default, ...) \
    HOOK_CALL_NOP(f, ret_default, __VA_ARGS__)

#define BOOT_HOOK_CALL_FIH(f, fih_ret_default, fih_rc, ...) \
    HOOK_CALL_FIH_NOP(f, fih_ret_default, fih_rc, __VA_ARGS__)

#endif /* MCUBOOT_IMAGE_ACCESS_HOOKS */

#ifdef MCUBOOT_BOOT_GO_HOOKS

#define BOOT_HOOK_GO_CALL_FIH(f, fih_ret_default, fih_rc, ...) \
    DO_HOOK_CALL_FIH(f, fih_ret_default, fih_rc, __VA_ARGS__);

#else

#define BOOT_HOOK_GO_CALL_FIH(f, fih_ret_default, fih_rc, ...) \
    HOOK_CALL_FIH_NOP(f, fih_ret_default, fih_rc, __VA_ARGS__)

#endif /* MCUBOOT_BOOT_GO_HOOKS  */

#ifdef MCUBOOT_BOOT_IMAGE_JUMP_HOOKS

#define BOOT_HOOK_IMAGE_JUMP_CALL_FIH(f, fih_rc, ...) \
    DO_HOOK_CALL_FIH(f, FIH_SUCCESS, fih_rc, __VA_ARGS__);

#else

#define BOOT_HOOK_IMAGE_JUMP_CALL_FIH(f, fih_rc, ...) \
    HOOK_CALL_FIH_NOP(f, FIH_SUCCESS, fih_rc, __VA_ARGS__)

#endif /* MCUBOOT_BOOT_IMAGE_JUMP_HOOKS */

#ifdef MCUBOOT_FIND_NEXT_SLOT_HOOKS

#define BOOT_HOOK_FIND_SLOT_CALL(f, ret_default, ...) \
    DO_HOOK_CALL(f, ret_default, __VA_ARGS__)

#else

#define BOOT_HOOK_FIND_SLOT_CALL(f, ret_default, ...) \
    HOOK_CALL_NOP(f, ret_default, __VA_ARGS__)

#endif /* MCUBOOT_FIND_NEXT_SLOT_HOOKS */

#ifdef MCUBOOT_FLASH_AREA_HOOKS

#define BOOT_HOOK_FLASH_AREA_CALL(f, ret_default, ...) \
    DO_HOOK_CALL(f, ret_default, __VA_ARGS__)

#else

#define BOOT_HOOK_FLASH_AREA_CALL(f, ret_default, ...) \
    HOOK_CALL_NOP(f, ret_default, __VA_ARGS__)

#endif /* MCUBOOT_FLASH_AREA_ID_HOOKS */

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
 *         FIH_BOOT_HOOK_REGULAR: follow the normal execution path.
 */
fih_ret boot_image_check_hook(int img_index, int slot);

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

/** Hook will be invoked when boot_serial requests device reset.
 *  The hook may be used to prevent device reset.
 *
 * @param force set to true when request tries to force reset.
 *
 * @retval 0 when reset should be performed;
 *         BOOT_RESET_REQUEST_HOOK_BUSY when some processing is still in
 *         progress;
 *         BOOT_RESET_REQUEST_HOOK_TIMEOUT internal process timed out;
 *         BOOT_RESET_REQUEST_HOOK_CHECK_FAILED internal code failed to
 *         obtian status;
 *         BOOT_RESET_REQUEST_HOOK_INTERNAL_ERROR unspecified internal
 *         error while checking status.
 */
int boot_reset_request_hook(bool force);

/**
 * Hook to implement custom action before boot_go() function.
 *
 * @param rsp boot response structure.
 *
 * @retval FIH_SUCCESS: boot_go() should be skipped, boot response is already
 *         filled.
 *         FIH_FAILURE: boot_go() should be skipped, boot response is already
 *         filled with error.
 *         FIH_BOOT_HOOK_REGULAR: follow the normal execution path.
 */
fih_ret boot_go_hook(struct boot_rsp *rsp);

/**
 * Hook called right after boot_go() succeeds, before do_boot() transfers
 * control to the application.
 *
 * This hook receives the selected boot_rsp so it can inspect or modify it
 * before the jump is made.  Typical uses include configuring flash access
 * rights based on the chosen image slot or applying memory remapping.
 *
 * @param rsp  Pointer to the boot response selected by boot_go().
 *
 * @retval FIH_SUCCESS  Hook executed successfully; proceed with do_boot().
 * @retval FIH_FAILURE  An error occurred; MCUboot will treat this as a boot
 *                      failure and panic.
 */
fih_ret boot_image_jump_hook(struct boot_rsp *rsp);

/**
 * Hook to implement custom action before retrieving flash area ID.
 *
 * @param image_index the index of the image pair
 * @param slot slot number
 * @param area_id the flash area ID to be populated
 *
 * @retval 0 the flash area ID was fetched successfully;
 *         BOOT_HOOK_REGULAR follow the normal execution path to get the flash
 *         area ID;
 *         otherwise an error-code value.
 */
int flash_area_id_from_multi_image_slot_hook(int image_index, int slot,
                                             int *area_id);

/**
 * Hook to implement custom action before retrieving flash area device ID.
 *
 * @param fa the flash area structure
 * @param device_id the device ID to be populated
 *
 * @retval 0 the device ID was fetched successfully;
 *         BOOT_HOOK_REGULAR follow the normal execution path to get the device
 *         ID;
 *         otherwise an error-code value.
 */
int flash_area_get_device_id_hook(const struct flash_area *fa,
                                  uint8_t *device_id);

#define BOOT_RESET_REQUEST_HOOK_BUSY		1
#define BOOT_RESET_REQUEST_HOOK_TIMEOUT		2
#define BOOT_RESET_REQUEST_HOOK_CHECK_FAILED	3
#define BOOT_RESET_REQUEST_HOOK_INTERNAL_ERROR	4

/**
 * Finds the preferred slot containing the image.
 *
 * @param[in]   state        Boot loader status information.
 * @param[in]   image        Image, for which the slot should be found.
 * @param[out]  active_slot  Number of the preferred slot.
 *
 * @return 0 if a slot was requested;
 *         BOOT_HOOK_REGULAR follow the normal execution path.
 */
int boot_find_next_slot_hook(struct boot_loader_state *state, uint8_t image, enum boot_slot *active_slot);

/* Swap-state hook family.  Uses the same DO_HOOK_CALL / HOOK_CALL_NOP dispatch
 * machinery as the other hook families (image-access, boot-go, find-slot, ...):
 * when MCUBOOT_SWAP_STATE_HOOKS is enabled BOOT_SWAP_STATE_HOOK_CALL invokes the
 * hook and yields its return value; when disabled it evaluates to ret_default
 * with no call (and no link dependency on the hook symbol).
 *
 * Semantics are REPLACE: a compiled-in hook implementation fully owns status
 * bookkeeping, so a hook, once present, always handles the call -- it never
 * defers to the built-in path.  Callers use the idiom:
 *
 *     int rc = BOOT_SWAP_STATE_HOOK_CALL(f, BOOT_SWAP_STATE_HOOK_REGULAR, ...);
 *     if (rc != BOOT_SWAP_STATE_HOOK_REGULAR) {
 *         return rc;              // hook owned it: success or a real error
 *     }
 *     ... built-in implementation ...
 *
 * The amend variant (built-in runs first, hook augments at a defined point)
 * passes ret_default = 0, i.e. "nothing to amend" when the family is disabled:
 *
 *     rc = BOOT_SWAP_STATE_HOOK_CALL(f, 0, ...);
 *
 * BOOT_SWAP_STATE_HOOK_REGULAR is the family's "not handled" sentinel.  It is NOT
 * BOOT_HOOK_REGULAR: BOOT_HOOK_REGULAR == BOOT_EFLASH == 1 (bootutil_public.h)
 * and these hooks return BOOT_EFLASH on real flash errors, so reusing it would
 * misread a flash error as "defer to the built-in status-byte path" and run it
 * after a flash fault.  INT_MIN is distinct from 0 and every BOOT_E* code, so no
 * hook return value can collide with it. */
#define BOOT_SWAP_STATE_HOOK_REGULAR  INT_MIN

#ifdef MCUBOOT_SWAP_STATE_HOOKS
#define BOOT_SWAP_STATE_HOOK_CALL(f, ret_default, ...) \
    DO_HOOK_CALL(f, ret_default, __VA_ARGS__)
#else
#define BOOT_SWAP_STATE_HOOK_CALL(f, ret_default, ...) \
    HOOK_CALL_NOP(f, ret_default, __VA_ARGS__)
#endif

/* boot_hooks.h is a PUBLIC header with consumers that never include the private
 * bootutil_priv.h (e.g. boot/zephyr/flash_map_extended.c) -- struct boot_status
 * is defined only there, so forward-declare it or the prototypes below create
 * prototype-scope tags (warning / incompatible-declaration risk). */
struct boot_status;

/**
 * Swap-state hook implementations are strong symbols that MUST be linked
 * whenever MCUBOOT_SWAP_STATE_HOOKS is defined.
 *
 * @return 0 on success, or a BOOT_E* error which the caller returns verbatim.
 *         BOOT_HOOK_REGULAR has no meaning for this family because there is no
 *         runtime fallthrough.
 */
int boot_write_status_hook(const struct boot_loader_state *state,
                           struct boot_status *bs);
int swap_status_init_hook(struct boot_loader_state *state,
                          const struct flash_area *fap,
                          const struct boot_status *bs);
int swap_read_status_bytes_hook(const struct flash_area *fap,
                                struct boot_loader_state *state,
                                struct boot_status *bs);

/**
 * Commit swap-state authority to its permanent home before the swap flow
 * erases a staging area that may still hold it.  A downstream swap-state
 * provider implements this to make its authority durable first, so an
 * interruption after the erase still recovers.  The scratch swap flow
 * (swap_scratch.c) is the only caller today; a future algorithm with a
 * pre-swap staging erase can attach here.
 *
 * @return 0 on success, or a BOOT_E* error. The caller must not erase the
 *         staging area after an error.
 */
int swap_status_before_erase_hook(const struct boot_loader_state *state,
                                  const struct flash_area *fap_pri,
                                  const struct flash_area *fap_staging,
                                  struct boot_status *bs);

/**
 * Amends boot_swap_image() after swap_run(), for hook implementations that keep
 * swap progress outside the image trailers.  It reconstructs any primary
 * trailer field left uncommitted by an interrupted swap (the data copy may be
 * complete while MAGIC was not yet written) and erases a stale scratch trailer
 * that would otherwise re-enter the swap path on the next boot.
 *
 * @return 0 on success, or a BOOT_E* error which the caller asserts on.
 */
int boot_swap_complete_hook(struct boot_loader_state *state,
                            struct boot_status *bs);

/**
 * Identifies a single field of the swap-state metadata record kept by a
 * swap-state-metadata provider (read/written/committed by the hooks below).
 * The encryption-key and IV identifiers are reserved: no caller passes them
 * yet, and a hook implementation may reject them.
 */
enum boot_swap_meta_field {
    BOOT_SWAP_META_MAGIC = 0,
    BOOT_SWAP_META_IMAGE_OK,
    BOOT_SWAP_META_COPY_DONE,
    BOOT_SWAP_META_SWAP_INFO,
    BOOT_SWAP_META_SWAP_SIZE,
    BOOT_SWAP_META_ENC_KEY_0,   /* reserved */
    BOOT_SWAP_META_ENC_KEY_1,   /* reserved */
    BOOT_SWAP_META_IV           /* reserved */
};

/**
 * Reports the physical constraints of a swap-state-metadata provider's
 * backing store, as populated by boot_swap_meta_geometry_hook().
 */
struct boot_swap_meta_geometry {
    /** Smallest unit the provider can write atomically. */
    uint32_t atomic_io_size;
    /** Bytes the provider reserves at the tail of the image slot. */
    uint32_t image_tail_reserved;
    /** True if the provider itself lays down a v1-style image trailer. */
    bool materializes_trailer;
};

/**
 * Reads the swap state (as swap_read_status_bytes_hook() would) from a
 * swap-state-metadata provider's record instead of the native trailer.
 *
 * @return 0 on success, or a BOOT_E* error which the caller returns verbatim.
 */
int boot_read_swap_state_hook(const struct flash_area *fap,
                              struct boot_swap_state *state);

/**
 * Reads a single field out of the provider's swap-state metadata record.
 *
 * @param field the field to read.
 * @param out   buffer to receive the field's value.
 * @param len   size of @p out, in bytes.
 *
 * @return 0 on success, or a BOOT_E* error.
 */
int boot_swap_meta_read_field_hook(const struct flash_area *fap,
                                   enum boot_swap_meta_field field,
                                   void *out, size_t len);

/**
 * Writes a single field into the provider's swap-state metadata record.
 *
 * @param field the field to write.
 * @param in    buffer holding the field's new value.
 * @param len   size of @p in, in bytes.
 *
 * @return 0 on success, or a BOOT_E* error.
 */
int boot_swap_meta_write_field_hook(const struct flash_area *fap,
                                    enum boot_swap_meta_field field,
                                    const void *in, size_t len);

/**
 * Commits the in-memory boot_status to the provider's metadata record.
 *
 * @return 0 on success, or a BOOT_E* error.
 */
int boot_swap_meta_commit_hook(const struct flash_area *fap,
                               struct boot_status *bs);

/**
 * Resets/erases the provider's swap-state metadata record for a slot.
 *
 * @return 0 on success, or a BOOT_E* error.
 */
int boot_swap_meta_reset_hook(const struct boot_loader_state *state,
                              const struct flash_area *fap, int slot);

/**
 * Reports the provider's backing-store geometry so the built-in swap logic
 * can size scratch usage and atomic writes correctly.
 *
 * @return 0 on success, or a BOOT_E* error.
 */
int boot_swap_meta_geometry_hook(const struct flash_area *fap,
                                 struct boot_swap_meta_geometry *geo);

#endif /*H_BOOTUTIL_HOOKS*/
