// SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

//! Utility functions used throughout MCUboot

pub fn align_up(num: u32, align: u32) -> u32 {
    assert!(align.is_power_of_two());

    (num + (align - 1)) & !(align - 1)
}
