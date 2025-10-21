#include "rng_stm32.h"

static const struct device *entropy_dev = NULL;
static bool initialized = false;

int generator_rng_stm32(uint32_t *val)
{

	*val = sys_rand32_get();
	
	return 0;

}
