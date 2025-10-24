#include "boot_rng.h"

static const struct device *entropy_dev = NULL;
static bool initialized = false;

int generator_hw_rng(uint32_t *val)
{

	*val = sys_rand32_get();
	
	return 0;

}
