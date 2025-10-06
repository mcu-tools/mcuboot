#ifndef _STM32_RNG_H_
#define _STM32_RNG_H_

#include "stm32wlxx_hal.h"

extern RNG_HandleTypeDef hrng;

HAL_StatusTypeDef generator_rng_stm32( uint32_t* val);

#endif /*_STM32_RNG_H_*/
