/**
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
#include <string.h>

#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

#define BLINKY_PRIO (8)
#define BLINKY_STACK_SIZE    OS_STACK_ALIGN(128)
static struct os_task task1;
static volatile int g_task1_loops;

/* For LED toggling */
int g_led_pin;

static void
blinky_handler(void *arg)
{
    while (1) {
        ++g_task1_loops;

        os_time_delay(OS_TICKS_PER_SEC / MYNEWT_VAL(BLINKY_TICKS_PER_SEC));

        /* Toggle the LED */
        hal_gpio_toggle(g_led_pin);
    }
}

int
main(int argc, char **argv)
{
    os_stack_t *pstack;

#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif

    sysinit();

    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);

    pstack = malloc(sizeof(os_stack_t) * BLINKY_STACK_SIZE);
    assert(pstack);

    os_task_init(&task1, "blinky", blinky_handler, NULL,
            BLINKY_PRIO, OS_WAIT_FOREVER, pstack, BLINKY_STACK_SIZE);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    return 0;
}

