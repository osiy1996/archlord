#ifndef _AP_TICK_H_
#define _AP_TICK_H_

#include "public/ap_module_instance.h"

#define AP_TICK_MODULE_NAME "AgpmTick"

BEGIN_DECLS

struct ap_tick_module;

struct ap_tick_module * ap_tick_create_module();

uint64_t ap_tick_get(struct ap_tick_module * mod);

END_DECLS

#endif /* _AP_TICK_H_ */
