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

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "syscfg/syscfg.h"
#include "sysflash/sysflash.h"
#include "testutil/testutil.h"
#include "hal/hal_flash.h"
#include <flash_map_backend/flash_map_backend.h>

#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil_priv.h"

#include "mbedtls/sha256.h"

TEST_CASE_DECL(boot_test_nv_ns_10)
TEST_CASE_DECL(boot_test_nv_ns_01)
TEST_CASE_DECL(boot_test_nv_ns_11)
TEST_CASE_DECL(boot_test_vm_ns_10)
TEST_CASE_DECL(boot_test_vm_ns_01)
TEST_CASE_DECL(boot_test_vm_ns_11_a)
TEST_CASE_DECL(boot_test_vm_ns_11_b)
TEST_CASE_DECL(boot_test_vm_ns_11_2areas)
TEST_CASE_DECL(boot_test_nv_bs_10)
TEST_CASE_DECL(boot_test_nv_bs_11)
TEST_CASE_DECL(boot_test_nv_bs_11_2areas)
TEST_CASE_DECL(boot_test_vb_ns_11)
TEST_CASE_DECL(boot_test_no_hash)
TEST_CASE_DECL(boot_test_no_flag_has_hash)
TEST_CASE_DECL(boot_test_invalid_hash)
TEST_CASE_DECL(boot_test_revert)
TEST_CASE_DECL(boot_test_revert_continue)
TEST_CASE_DECL(boot_test_permanent)
TEST_CASE_DECL(boot_test_permanent_continue)

TEST_SUITE(boot_test_main)
{
    boot_test_nv_ns_10();
    boot_test_nv_ns_01();
    boot_test_nv_ns_11();
    boot_test_vm_ns_10();
    boot_test_vm_ns_01();
    boot_test_vm_ns_11_a();
    boot_test_vm_ns_11_b();
    boot_test_vm_ns_11_2areas();
    boot_test_nv_bs_10();
    boot_test_nv_bs_11();
    boot_test_nv_bs_11_2areas();
    boot_test_vb_ns_11();
    boot_test_no_hash();
    boot_test_no_flag_has_hash();
    boot_test_invalid_hash();
    boot_test_revert();
    boot_test_revert_continue();
    boot_test_permanent();
    boot_test_permanent_continue();
}

int
boot_test_all(void)
{
    boot_test_main();
    return tu_any_failed;
}

#if MYNEWT_VAL(SELFTEST)

int
main(int argc, char **argv)
{
    ts_config.ts_print_results = 1;
    tu_parse_args(argc, argv);

    tu_init();

    boot_test_all();

    return tu_any_failed;
}

#endif
