#include "public/ap_shrine.h"

#include "core/log.h"

#include "public/ap_object.h"

struct ap_shrine_module {
	struct ap_module_instance instance;
	struct ap_object_module * ap_object;
};

static boolean obj_read(
	struct ap_shrine_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	return TRUE;
}

static boolean obj_write(
	struct ap_shrine_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	return TRUE;
}

static boolean onregister(
	struct ap_shrine_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_object, 
		AP_OBJECT_MODULE_NAME);
	ap_object_add_stream_callback(mod->ap_object, AP_OBJECT_MDI_OBJECT,
		AP_SHRINE_MODULE_NAME, mod, obj_read, obj_write);
	return TRUE;
}

struct ap_shrine_module * ap_shrine_create_module()
{
	struct ap_shrine_module * mod = ap_module_instance_new(AP_SHRINE_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	return mod;
}
