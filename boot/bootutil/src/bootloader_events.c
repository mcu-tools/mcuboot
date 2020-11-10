/*
 * Copyright (c) 2020 IP-Logix Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bootutil/bootloader_events.h"

/* The user can provide an implementation of this function in their own code
 * to respond to bootloader events, for example to update UI or LEDs.
 */
 __attribute__((weak))
void bootloader_event(enum bootloader_event event,
					  struct bootloader_event_param *param)
{
	/* -- */
}