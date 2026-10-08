/*
 * Copyright (c) 2026 Wavenumber
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Cortex-A cleanup before chain-loading the application.
 *
 * The interrupt half is the same shape as arm_cortex_r.c. What an A needs and
 * an R does not is below it: an MMU rather than an MPU, and a branch predictor
 * that has to be invalidated because Zephyr's AArch32 cache code does not.
 *
 * An external L2 controller is deliberately not handled here. It is not part of
 * the core, SoCs integrate it differently, and some Cortex-A parts have none,
 * so it belongs to the SoC layer. Where one is enabled the SoC must clean it
 * before this runs, or the application reads stale memory.
 */

#include <stdint.h>
#include <zephyr/irq.h>
#include <zephyr/sys/barrier.h>
#include <zephyr/sys/util_macro.h>
#include <zephyr/toolchain.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/sys_io.h>

#ifdef CONFIG_ARM_CUSTOM_INTERRUPT_CONTROLLER
extern void z_soc_irq_eoi(unsigned int irq);
#else
#include <zephyr/drivers/interrupt_controller/gic.h>
#endif

/*
 * BOOT_DISABLE_CACHES calls sys_cache_data_flush_all() and
 * sys_cache_data_disable(), and without CONFIG_CACHE_MANAGEMENT both compile
 * to nothing. On a RAM-load target that is silently fatal: the image the
 * loader copied stays in the data cache, the application is fetched from a
 * DDR that never received it, and the handoff succeeds or fails depending on
 * whether the lines were evicted. It presents as an intermittent boot, which
 * is the hardest kind of failure to attribute to a missing Kconfig.
 */
#if defined(CONFIG_BOOT_DISABLE_CACHES) && !defined(CONFIG_CACHE_MANAGEMENT)
#error "BOOT_DISABLE_CACHES needs CACHE_MANAGEMENT; without it the cache operations do nothing"
#endif

#define WRITE_CP15(value, coproc, opc1, crn, crm, opc2)                        \
	__asm__ volatile("mcr " #coproc ", " #opc1 ", %0, " #crn ", " #crm ", " \
			 #opc2 "\n" ::"r"(value) :"memory")

#define READ_CP15(out, coproc, opc1, crn, crm, opc2)                           \
	__asm__ volatile("mrc " #coproc ", " #opc1 ", %0, " #crn ", " #crm ", " \
			 #opc2 "\n" : "=r"(out)::"memory")

/* System control register bits this file clears. */
#define SCTLR_M BIT(0)  /* MMU */
#define SCTLR_C BIT(2)  /* data cache */
#define SCTLR_I BIT(12) /* instruction cache */
#define SCTLR_TRE BIT(28) /* TEX remap */
#define SCTLR_AFE BIT(29) /* access flag */

void cleanup_arm_interrupts(void)
{
	/* Allow any pending interrupts to be recognized */
	__ISB();
	__disable_irq();

	for (unsigned int i = 0; i < CONFIG_NUM_IRQS; ++i) {
		irq_disable(i);
	}

	for (unsigned int i = 0; i < CONFIG_NUM_IRQS; ++i) {
#ifdef CONFIG_ARM_CUSTOM_INTERRUPT_CONTROLLER
		z_soc_irq_eoi(i);
#else
		arm_gic_eoi(i);
#endif /* CONFIG_ARM_CUSTOM_INTERRUPT_CONTROLLER */
	}

#if defined(CONFIG_GIC) && !defined(CONFIG_ARM_CUSTOM_INTERRUPT_CONTROLLER)
	/*
	 * Switch the controller itself off, not only the interrupts in it.
	 *
	 * Disabling and acknowledging each interrupt leaves the distributor and
	 * the CPU interface enabled, so the application starts with a live
	 * controller it has not configured. Measured on a Zynq-7000 against the
	 * state the first-stage loader leaves: the distributor read 0x1 and the
	 * CPU interface 0x3 where the reference had both at zero, with the
	 * priority mask open at 0xF0.
	 *
	 * Nothing was enabled behind them in that measurement, so nothing fired,
	 * but an application that enables one interrupt then inherits a
	 * controller already willing to deliver it.
	 */
	sys_write32(0U, GICC_CTLR);
	sys_write32(0U, GICD_CTLR);
	/* And the priority mask, which decides what the interface would deliver
	 * if anything turned it back on. Left at 0xF0 it is inert only for as
	 * long as the interface stays off.
	 */
	sys_write32(0U, GICC_PMR);
	barrier_dsync_fence_full();
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(arm_armv8_timer) && DT_NODE_HAS_PROP(DT_INST(0, arm_armv8_timer), reg)
	/*
	 * Stop the timer from being able to interrupt.
	 *
	 * This timer is in the Cortex-A9 private memory region beside the
	 * interrupt controller, and the kernel leaves its comparator and its
	 * interrupt enable set because that is how it keeps time. Measured
	 * against the first-stage loader: the control register read 0x7 where
	 * the reference had 0x1, so the loader hands over a timer armed to fire
	 * rather than one merely counting.
	 *
	 * It did not fire in that measurement, because the timer's private
	 * interrupt was still disabled in the distributor. An application that
	 * enables that one interrupt would inherit a comparator already set.
	 *
	 * The proper home for this is sys_clock_disable(), which MCUboot already
	 * calls and which this timer driver does not implement: without
	 * CONFIG_SYSTEM_TIMER_HAS_DISABLE_SUPPORT that call compiles to nothing.
	 * Implementing it in the driver would fix every user of it rather than
	 * this one.
	 */
	sys_write32(0U, DT_REG_ADDR(DT_INST(0, arm_armv8_timer)) + 0x08U);
	barrier_dsync_fence_full();
#endif
}

#if defined(CONFIG_ARM_AARCH32_MMU)
__weak void z_arm_clear_arm_mmu_config(void)
{
	uint32_t sctlr;

	/*
	 * Runs after the caches have been cleaned and disabled, so the image
	 * already sits in memory. What is left is the state that would still
	 * describe this boot loader once the application is running.
	 *
	 * The instruction cache and the branch predictor are invalidated rather
	 * than only disabled: entries fetched and predictions made against these
	 * translation tables are not valid under the ones the application
	 * installs, and arch_icache_disable() clears the enable bit without
	 * invalidating anything.
	 *
	 * Invalidated, and for the branch predictor that is all. Clearing its enable
	 * bit as well costs the next image its branch prediction, because Zephyr's
	 * AArch32 start-up does not set that bit and nothing else does: measured at
	 * 29 per cent of flash read throughput on a Zynq-7000. An invalidated
	 * predictor holds nothing from this loader, so disabling it buys no isolation.
	 */
	WRITE_CP15(0, p15, 0, c7, c5, 0); /* invalidate the instruction cache */
	WRITE_CP15(0, p15, 0, c7, c5, 6); /* invalidate the branch predictor */
	barrier_dsync_fence_full();
	barrier_isync_fence_full();

	READ_CP15(sctlr, p15, 0, c1, c0, 0);
	sctlr &= ~(SCTLR_M | SCTLR_C | SCTLR_I | SCTLR_AFE | SCTLR_TRE);
	barrier_dsync_fence_full();
	WRITE_CP15(sctlr, p15, 0, c1, c0, 0);
	barrier_isync_fence_full();

	/*
	 * The translation lookaside buffer is invalidated after the unit is
	 * switched off, so nothing can refill it from tables that are about to
	 * stop being the ones in use.
	 */
	WRITE_CP15(0, p15, 0, c8, c7, 0); /* invalidate the entire unified TLB */
	barrier_dsync_fence_full();
	barrier_isync_fence_full();

	/*
	 * Only now the translation tables and the domain permissions that
	 * described this boot loader. They are inert with the unit off and live
	 * the moment anything turns it on, so leaving them hands the application
	 * a mapping it did not choose: measured against the first-stage loader,
	 * TTBR0 held 0x0091004A where the reference had 0x0000C05B and DACR
	 * 0x55555555 against 0xFFFFFFFF.
	 *
	 * After the unit is off, not before. Clearing TTBR0 while translation is
	 * still enabled makes the next instruction fetch walk a null table, and
	 * the handoff dies between the branch and the application's first line.
	 */
	WRITE_CP15(0, p15, 0, c2, c0, 0); /* TTBR0 */
	WRITE_CP15(0, p15, 0, c2, c0, 1); /* TTBR1 */
	WRITE_CP15(0, p15, 0, c2, c0, 2); /* TTBCR */
	WRITE_CP15(0, p15, 0, c3, c0, 0); /* DACR */
	barrier_dsync_fence_full();
	barrier_isync_fence_full();
}
#endif /* CONFIG_ARM_AARCH32_MMU */
