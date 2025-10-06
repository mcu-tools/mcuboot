#include "stm32wlxx_hal.h"

RNG_HandleTypeDef hhrng;

HAL_StatusTypeDef generator_rng_stm32(uint32_t* val){

	return HAL_RNG_GenerateRandomNumber(&hhrng, val);
}

