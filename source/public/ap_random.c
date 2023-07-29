#include "public/ap_random.h"

#include "vendor/pcg/pcg_basic.h"

#include <assert.h>
#include <stdlib.h>
#include <time.h>

struct ap_random_module {
	struct ap_module_instance instance;
	pcg32_random_t rng;
};

struct ap_random_module * ap_random_create_module()
{
	struct ap_random_module * mod = ap_module_instance_new("AgpmRandom", 
		sizeof(*mod), NULL, NULL, NULL, NULL);
	pcg32_srandom_r(&mod->rng, (uint64_t)time(NULL), (uint64_t)time);
	return mod;
}

uint32_t ap_random_get(struct ap_random_module * mod, uint32_t upper_bound)
{
	return pcg32_boundedrand_r(&mod->rng, upper_bound);
}

uint32_t ap_random_between(struct ap_random_module * mod, uint32_t nmin, uint32_t nmax)
{
	assert(nmin <= nmax);
	assert((nmax - nmin) < UINT32_MAX);
	return nmin + pcg32_boundedrand_r(&mod->rng, 
		(nmax - nmin) + 1);
}

float ap_random_float(struct ap_random_module * mod, float fmin, float fmax)
{
	float scale;
	float d = fmax - fmin;
	assert(fmin <= fmax);
	scale = (float)((double)pcg32_random_r(&mod->rng) / 
		(double)UINT32_MAX);
	return fmin + scale * d;
}
