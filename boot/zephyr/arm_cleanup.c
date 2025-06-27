/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 * Copyright (c) 2025 Siemens Mobility GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <zephyr/irq.h>
#include "zephyr/sys/util_macro.h"
#include <zephyr/toolchain.h>

#ifdef CONFIG_CPU_CORTEX_M
#include <cmsis_core.h>
#endif

#if CONFIG_CPU_HAS_NXP_SYSMPU
#include <fsl_sysmpu.h>
#endif

#ifndef CONFIG_CPU_CORTEX_M

#ifdef CONFIG_ARM_CUSTOM_INTERRUPT_CONTROLLER
extern void z_soc_irq_eoi(unsigned int irq);
#else
#include <zephyr/drivers/interrupt_controller/gic.h>
#endif

#endif /* CONFIG_CPU_CORTEX_M */

/* Macros for inline assembly to improve readability */
#ifdef CONFIG_CPU_CORTEX_R5

#define READ_COPROCESSOR_REGISTER(out, coproc, opc1, crn, crm, opc2) \
	__asm__ volatile("mrc " #coproc ", " #opc1 ", %0, " #crn ", " #crm ", " #opc2 "\n" : "=r" (out) ::);

#define WRITE_COPROCESSOR_REGISTER(in, coproc, opc1, crn, crm, opc2) \
	__asm__ volatile("mcr " #coproc ", " #opc1 ", %0, " #crn ", " #crm ", " #opc2 "\n" :: "r" (in) :)

#endif /* CONFIG_CPU_CORTEX_R5 */

void cleanup_arm_interrupts(void)
{
	/* Allow any pending interrupts to be recognized */
	__ISB();
	__disable_irq();

#ifdef CONFIG_CPU_CORTEX_M
	/* Disable NVIC interrupts */
	for (uint8_t i = 0; i < ARRAY_SIZE(NVIC->ICER); i++) {
		NVIC->ICER[i] = 0xFFFFFFFF;
	}
	/* Clear pending NVIC interrupts */
	for (uint8_t i = 0; i < ARRAY_SIZE(NVIC->ICPR); i++) {
		NVIC->ICPR[i] = 0xFFFFFFFF;
	}
#else
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

#endif /* CONFIG_CPU_CORTEX_M */
}

#if CONFIG_CPU_HAS_ARM_MPU
__weak void z_arm_clear_arm_mpu_config(void)
{
#ifdef CONFIG_CPU_CORTEX_M
	int i;

	int num_regions =
		((MPU->TYPE & MPU_TYPE_DREGION_Msk) >> MPU_TYPE_DREGION_Pos);

	for (i = 0; i < num_regions; i++) {
		ARM_MPU_ClrRegion(i);
	}
#else
	uint8_t i;
	uint8_t num_regions;
	uint32_t mpu_type_register;

	/* Disable MPU */
	uint32_t val;
	READ_COPROCESSOR_REGISTER(val, p15, 0, c1, c0, 0);
	val &= ~BIT(0);
	__DSB();

	WRITE_COPROCESSOR_REGISTER(val, p15, 0, c1, c0, 0);
	__ISB();

	/* The number of MPU regions is stored in bits 15:8 of the MPU type register */
	READ_COPROCESSOR_REGISTER(mpu_type_register, p15, 0, c0, c0, 4);
	num_regions = (uint8_t) ((mpu_type_register >> 8) & BIT_MASK(8));

	for (i = 0; i < num_regions; ++i) {
		/* Select region in the MPU and clear the region size field */
		WRITE_COPROCESSOR_REGISTER(i, p15, 0, c6, c2, 0);
		WRITE_COPROCESSOR_REGISTER(0, p15, 0, c6, c1, 2);
	}
#endif /* CONFIG_CPU_CORTEX_M */
}
#elif CONFIG_CPU_HAS_NXP_SYSMPU
__weak void z_arm_clear_arm_mpu_config(void)
{
	int i;

	int num_regions = FSL_FEATURE_SYSMPU_DESCRIPTOR_COUNT;

	SYSMPU_Enable(SYSMPU, false);

	/* NXP MPU region 0 is reserved for the debugger */
	for (i = 1; i < num_regions; i++) {
		SYSMPU_RegionEnable(SYSMPU, i, false);
	}
}
#endif
