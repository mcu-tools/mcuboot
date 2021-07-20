/*
 * Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <bootutil/bootutil.h>

int main()
{
    struct boot_rsp rsp;
    int rv = boot_go(&rsp);

    if (rv == 0) {
        //    
    }
    while(1);
}
