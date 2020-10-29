/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/arm/aarch32/cortex_m/cmsis.h>

void cleanup_arm_nvic(void) {
	/* Allow any pending interrupts to be recognized */
	__ISB();
	__disable_irq();

	/* Disable NVIC interrupts */
	for (uint8_t i = 0; i < ARRAY_SIZE(NVIC->ICER); i++) {
		NVIC->ICER[i] = 0xFFFFFFFF;
	}
	/* Clear pending NVIC interrupts */
	for (uint8_t i = 0; i < ARRAY_SIZE(NVIC->ICPR); i++) {
		NVIC->ICPR[i] = 0xFFFFFFFF;
	}
}

#if CONFIG_CPU_HAS_ARM_MPU
__weak void z_arm_clear_arm_mpu_config(void)
{
	int i;

	int num_regions =
		((MPU->TYPE & MPU_TYPE_DREGION_Msk) >> MPU_TYPE_DREGION_Pos);

	for (i = 0; i < num_regions; i++) {
		ARM_MPU_ClrRegion(i);
	}
}
#endif
