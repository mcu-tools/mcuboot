#ifndef _BOOT_RNG_H_
#define _BOOT_RNG_H_

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/random/random.h>



int generator_hw_rng(uint32_t* val);


#endif /*_BOOT_RNG_H_*/
