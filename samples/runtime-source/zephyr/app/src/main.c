/*
 * Copyright (c) 2017 Linaro, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void)
{
	printk("Hello World from %s on %s, slot %s!\n",
	       MCUBOOT_HELLO_WORLD_FROM, CONFIG_BOARD,
           DT_PROP(DT_CHOSEN(zephyr_code_partition), label));
}
