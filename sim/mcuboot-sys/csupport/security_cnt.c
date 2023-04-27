/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2023 Arm Limited
 */

#include "bootutil/security_cnt.h"
#include "mcuboot_config/mcuboot_logging.h"
#include "bootutil/fault_injection_hardening.h"

/*
 * Since the simulator is executing unit tests in parallel,
 * the storage area where the security counter values reside
 * has to be managed per thread from Rust's side.
 */
#ifdef MCUBOOT_HW_ROLLBACK_PROT

int sim_set_nv_counter_for_image(uint32_t image_index, uint32_t security_counter_value);

int sim_get_nv_counter_for_image(uint32_t image_index, uint32_t* data);

fih_ret boot_nv_security_counter_init(void) {
    return FIH_SUCCESS;
}

fih_ret boot_nv_security_counter_get(uint32_t image_id, fih_int *security_cnt) {
    uint32_t counter = 0;
    FIH_DECLARE(fih_rc, FIH_FAILURE);
    fih_rc = fih_ret_encode_zero_equality(sim_get_nv_counter_for_image(image_id, &counter));

    MCUBOOT_LOG_INF("Read security counter value (%d) for image: %d\n", counter, image_id);
    *security_cnt = fih_int_encode(counter);

    FIH_RET(fih_rc);
}

int32_t boot_nv_security_counter_update(uint32_t image_id, uint32_t img_security_cnt) {
    MCUBOOT_LOG_INF("Writing security counter value (%d) for image: %d\n", img_security_cnt, image_id);

    return sim_set_nv_counter_for_image(image_id, img_security_cnt);
}

#endif /* MCUBOOT_HW_ROLLBACK_PROT */
