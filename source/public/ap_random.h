#ifndef _AP_RANDOM_H_
#define _AP_RANDOM_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_factors.h"
#include "public/ap_module_instance.h"

#define AP_RANDOM_MODULE_NAME "AgpmRandom"

BEGIN_DECLS

struct ap_random_module;

struct ap_random_module * ap_random_create_module();

/**
 * \brief Generate a random number.
 * \param upper_bound Upper bound for generated number.
 * 
 * \return A randomly generated number between [0,upper_bound).
 */
uint32_t ap_random_get(struct ap_random_module * mod, uint32_t upper_bound);

/**
 * \brief Generate a random number within bounds.
 * \param min Lower bound for generated number.
 * \param max Upper bound for generated number.
 * 
 * \return A randomly generated number between [nmin,nmax].
 */
uint32_t ap_random_between(struct ap_random_module * mod, uint32_t nmin, uint32_t nmax);

float ap_random_float(struct ap_random_module * mod, float fmin, float fmax);

END_DECLS

#endif /* _AP_RANDOM_H_ */
