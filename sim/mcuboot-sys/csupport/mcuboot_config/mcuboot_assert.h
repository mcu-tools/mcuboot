#ifndef __MCUBOOT_ASSERT_H__
#define __MCUBOOT_ASSERT_H__

void sim_assert(int, const char *test, const char *, unsigned int, const char *);
#define ASSERT(x) sim_assert((x), #x, __FILE__, __LINE__, __func__)

#endif  /* __MCUBOOT_ASSERT_H__ */
