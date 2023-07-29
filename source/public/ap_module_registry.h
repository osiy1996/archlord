#ifndef _AP_MODULE_REGISTRY_H_
#define _AP_MODULE_REGISTRY_H_

#include "public/ap_module.h"

struct ap_module_registry {
	void ** list;
};

BEGIN_DECLS

struct ap_module_registry * ap_module_registry_new();

void ap_module_registry_destroy(struct ap_module_registry * registry);

boolean ap_module_registry_register(
	struct ap_module_registry * registry, 
	ap_module_t module_);

ap_module_t ap_module_registry_get_module(
	struct ap_module_registry * registry, 
	const char * module_name);

END_DECLS

#endif /* _AP_MODULE_REGISTRY_H_ */