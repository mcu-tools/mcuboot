#ifndef _STM32_RNG_H_
#define _STM32_RNG_H_

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/random/random.h>



int generator_rng_stm32(uint32_t* val);


#endif /*_STM32_RNG_H_*/
