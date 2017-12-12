#ifndef H_BOOTSIM_
#define H_BOOTSIM_

void sim_assert(int, const char *test, const char *, unsigned int, const char *);
#define ASSERT(x) sim_assert((x), #x, __FILE__, __LINE__, __func__)

#endif
