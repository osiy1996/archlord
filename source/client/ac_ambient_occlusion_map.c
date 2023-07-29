#include "client/ac_ambient_occlusion_map.h"

#include "core/log.h"

#include "public/ap_object.h"

#define INI_NAME_ATOMIC_COUNT "AmbOcclMapAtomicCount"

struct ac_ambient_occlusion_map_module {
	struct ap_module_instance instance;
	struct ap_object_module * ap_object;
};

static boolean obj_read(
	struct ac_ambient_occlusion_map_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	return TRUE;
}

static boolean obj_write(
	struct ac_ambient_occlusion_map_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	if (!ap_module_stream_write_i32(stream, INI_NAME_ATOMIC_COUNT, 0)) {
		ERROR("Failed to write object AO atomic count (oid = %u).",
			((struct ap_object *)data)->object_id);
		return FALSE;
	}
	return TRUE;
}

static boolean onregister(
	struct ac_ambient_occlusion_map_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_object, 
		AP_OBJECT_MODULE_NAME);
	ap_object_add_stream_callback(mod->ap_object, AP_OBJECT_MDI_OBJECT,
		AC_AMBIENT_OCCLUSION_MAP_MODULE_NAME, mod, obj_read, obj_write);
	return TRUE;
}

struct ac_ambient_occlusion_map_module * ac_ambient_occlusion_map_create_module()
{
	struct ac_ambient_occlusion_map_module * mod = ap_module_instance_new(
		AC_AMBIENT_OCCLUSION_MAP_MODULE_NAME, sizeof(*mod), 
		onregister, NULL, NULL, NULL);
	return mod;
}
