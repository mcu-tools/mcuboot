/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2020 Arm Limited
 * Copyright (c) 2021-2023 Nordic Semiconductor ASA
 * Copyright (c) 2025 Aerlync Labs Inc.
 * Copyright (c) 2025 Siemens Mobility GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/timer/system_timer.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/linker/linker-defs.h>
#include <soc.h>

#if defined(CONFIG_BOOT_DISABLE_CACHES)
#include <zephyr/cache.h>
#endif

#if defined(CONFIG_CPU_CORTEX_M)
#include <cmsis_core.h>
#endif

#include "bootutil/bootutil_log.h"
#include "bootutil/bootutil.h"
#include "bootutil/fault_injection_hardening.h"
#include "flash_map_backend/flash_map_backend.h"
#include "do_boot.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#if CONFIG_MCUBOOT_CLEANUP_ARM_CORE
#include <arm_cleanup.h>
#endif

#ifdef CONFIG_SW_VECTOR_RELAY
extern void *_vector_table_pointer;
#endif

struct arm_vector_table {
#if defined(CONFIG_CPU_CORTEX_M)
	uint32_t msp;
	uint32_t reset;
#else
	uint32_t reset;
	uint32_t undef_instruction;
	uint32_t svc;
	uint32_t abort_prefetch;
	uint32_t abort_data;
	uint32_t reserved;
	uint32_t irq;
	uint32_t fiq;
#endif
};

void do_boot(const struct boot_rsp *rsp)
{
	/* vt is static as it shall not land on the stack,
	 * as this procedure modifies stack pointer before usage of *vt
	 */
	static struct arm_vector_table *vt;

	/* The beginning of the image is the ARM vector table, containing
	 * the initial stack pointer address and the reset vector
	 * consecutively. Manually set the stack pointer and jump into the
	 * reset vector
	 */
#ifdef MCUBOOT_RAM_LOAD
	/* Get ram address for image */
	vt = (struct arm_vector_table *)(rsp->br_hdr->ih_load_addr + rsp->br_hdr->ih_hdr_size);
#else
	uintptr_t flash_base;
	int rc;

	/* Jump to flash image */
	rc = flash_device_base(rsp->br_flash_dev_id, &flash_base);
	if (rc != 0) {
		BOOT_LOG_DBG("Error getting flash device base. rc = %d", rc);
		FIH_PANIC;
	}

	vt = (struct arm_vector_table *)(flash_base +
					 rsp->br_image_off +
					 rsp->br_hdr->ih_hdr_size);
#endif

	if (IS_ENABLED(CONFIG_SYSTEM_TIMER_HAS_DISABLE_SUPPORT)) {
		sys_clock_disable();
	}

#ifdef CONFIG_USB_DEVICE_STACK
	/* Disable the USB to prevent it from firing interrupts */
	usb_disable();
#endif
#if CONFIG_MCUBOOT_CLEANUP_ARM_CORE
	cleanup_arm_interrupts(); /* Disable and acknowledge all interrupts */

#if defined(CONFIG_BOOT_DISABLE_CACHES)
	/* Flush and disable instruction/data caches before chain-loading the application */
	(void)sys_cache_instr_flush_all();
	(void)sys_cache_data_flush_all();
	sys_cache_instr_disable();
	sys_cache_data_disable();
#endif

#if CONFIG_CPU_HAS_ARM_MPU || CONFIG_CPU_HAS_NXP_SYSMPU
	z_arm_clear_arm_mpu_config();
#endif

#if defined(CONFIG_BUILTIN_STACK_GUARD) && \
    defined(CONFIG_CPU_CORTEX_M_HAS_SPLIM)
	/* Reset limit registers to avoid inflicting stack overflow on image
	 * being booted.
	 */
	__set_PSPLIM(0);
	__set_MSPLIM(0);
#endif

#else
	irq_lock();
#endif /* CONFIG_MCUBOOT_CLEANUP_ARM_CORE */

#ifdef CONFIG_BOOT_INTR_VEC_RELOC
#if defined(CONFIG_SW_VECTOR_RELAY)
	_vector_table_pointer = vt;
#ifdef CONFIG_CPU_CORTEX_M_HAS_VTOR
	SCB->VTOR = (uint32_t)__vector_relay_table;
#endif
#elif defined(CONFIG_CPU_CORTEX_M_HAS_VTOR)
	SCB->VTOR = (uint32_t)vt;
#endif /* CONFIG_SW_VECTOR_RELAY */
#else /* CONFIG_BOOT_INTR_VEC_RELOC */
#if defined(CONFIG_CPU_CORTEX_M_HAS_VTOR) && defined(CONFIG_SW_VECTOR_RELAY)
	_vector_table_pointer = _vector_start;
	SCB->VTOR = (uint32_t)__vector_relay_table;
#endif
#endif /* CONFIG_BOOT_INTR_VEC_RELOC */

#ifdef CONFIG_CPU_CORTEX_M
	__set_MSP(vt->msp);
#endif

#if CONFIG_MCUBOOT_CLEANUP_ARM_CORE
#ifdef CONFIG_CPU_CORTEX_M
	__set_CONTROL(0x00); /* application will configures core on its own */
	__ISB();
#else
	/* Set mode to supervisor and A, I and F bit as described in the
	 * Cortex R5 TRM */
	__asm__ volatile(
		"   mrs r0, CPSR\n"
		/* change mode bits to supervisor */
		"   bic r0, #0x1f\n"
		"   orr r0, #0x13\n"
		/* set the A, I and F bit */
		"   mov r1, #0b111\n"
		"   lsl r1, #0x6\n"
		"   orr r0, r1\n"

		"   msr CPSR, r0\n"
		::: "r0", "r1");
#endif /* CONFIG_CPU_CORTEX_M */

#endif
#if CONFIG_MCUBOOT_CLEANUP_RAM
	__asm__ volatile (
		/* vt->reset -> r0 */
		"   mov     r0, %0\n"
		/* base to write -> r1 */
		"   mov     r1, %1\n"
		/* size to write -> r2 */
		"   mov     r2, %2\n"
		/* value to write -> r3 */
		"   mov     r3, %3\n"
		"clear:\n"
		"   str     r3, [r1]\n"
		"   add     r1, r1, #4\n"
		"   sub     r2, r2, #4\n"
		"   cbz     r2, out\n"
		"   b       clear\n"
		"out:\n"
		"   dsb\n"
#if CONFIG_MCUBOOT_INFINITE_LOOP_AFTER_RAM_CLEANUP
		"   b       out\n"
#endif /*CONFIG_MCUBOOT_INFINITE_LOOP_AFTER_RAM_CLEANUP */
		/* jump to reset vector of an app */
		"   bx      r0\n"
		:
		: "r" (vt->reset), "i" (DT_CHOSEN_SRAM_ADDR),
		  "i" (DT_CHOSEN_SRAM_SIZE), "i" (0)
		: "r0", "r1", "r2", "r3", "memory"
	);
#else

#ifdef CONFIG_CPU_CORTEX_M
	((void (*)(void))vt->reset)();
#else
	/* Some ARM CPUs like the Cortex-R5 can run in thumb mode but reset into ARM
	 * mode (depending on a CPU signal configurations). To do the switch into ARM
	 * mode, if needed, an explicit branch with exchange instruction set
	 * instruction is needed
	 */
	__asm__("bx %0\n" : : "r" (&vt->reset));
#endif

#endif
}
