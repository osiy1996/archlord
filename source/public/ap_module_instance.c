#include "public/ap_module_instance.h"

#include "core/malloc.h"
#include "core/string.h"

ap_module_t ap_module_instance_new(
	const char * name, 
	size_t size,
	ap_module_instance_register_t register_callback,
	ap_module_instance_initialize_t initialize_callback,
	ap_module_instance_close_t close_callback,
	ap_module_instance_shutdown_t shutdown_callback)
{
    struct ap_module_instance * instance = alloc(size);
    memset(instance, 0, size);
    ap_module_init(&instance->context, name);
	instance->cb_register = register_callback;
	instance->cb_initialize = initialize_callback;
	instance->cb_close = close_callback;
	instance->cb_shutdown = shutdown_callback;
    return instance;
}

void ap_module_instance_destroy(ap_module_t module_)
{
    dealloc(module_);
}
