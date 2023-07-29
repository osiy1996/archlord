#ifndef _AP_MODULE_INSTANCE_H_
#define _AP_MODULE_INSTANCE_H_

#include "public/ap_module_registry.h"

#ifdef __cplusplus
#define AP_MODULE_INSTANCE_FIND_IN_REGISTRY(REGISTRY, MODULE, MODULE_NAME) \
{\
	*(ap_module_t *)&(MODULE) = ap_module_registry_get_module((REGISTRY), (MODULE_NAME));\
	if (!(MODULE)) {\
		ERROR("Failed to retrieve module (%s).", (MODULE_NAME));\
		return FALSE;\
		\
	}\
}
#else
#define AP_MODULE_INSTANCE_FIND_IN_REGISTRY(REGISTRY, MODULE, MODULE_NAME) \
{\
	(MODULE) = ap_module_registry_get_module((REGISTRY), (MODULE_NAME));\
	if (!(MODULE)) {\
		ERROR("Failed to retrieve module (%s).", (MODULE_NAME));\
		return FALSE;\
		\
	}\
}
#endif

BEGIN_DECLS

typedef boolean(*ap_module_instance_register_t)(
	ap_module_t module_,
	struct ap_module_registry * registry);

typedef boolean(*ap_module_instance_initialize_t)(ap_module_t module_);

typedef void(*ap_module_instance_close_t)(ap_module_t module_);

typedef void(*ap_module_instance_shutdown_t)(ap_module_t module_);

struct ap_module_instance {
	struct ap_module context;
	ap_module_instance_register_t cb_register;
	ap_module_instance_initialize_t cb_initialize;
	ap_module_instance_close_t cb_close;
	ap_module_instance_shutdown_t cb_shutdown;
};

ap_module_t ap_module_instance_new(
	const char * name, 
	size_t size,
	ap_module_instance_register_t register_callback,
	ap_module_instance_initialize_t initialize_callback,
	ap_module_instance_close_t close_callback,
	ap_module_instance_shutdown_t shutdown_callback);

void ap_module_instance_destroy(ap_module_t module_);

END_DECLS

#endif /* _AP_MODULE_INSTANCE_H_ */