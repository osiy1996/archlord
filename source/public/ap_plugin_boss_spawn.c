#include "public/ap_plugin_boss_spawn.h"

#include "public/ap_object.h"

struct ap_plugin_boss_spawn_module {
	struct ap_module_instance instance;
	struct ap_object_module * ap_object;
};

static boolean obj_read(ap_module_t mod, void * data, struct ap_module_stream * stream)
{
	return TRUE;
}

static boolean obj_write(ap_module_t mod, void * data, struct ap_module_stream * stream)
{
	return TRUE;
}

static boolean onregister(
	struct ap_plugin_boss_spawn_module * mod,
	struct ap_module_registry * registry)
{
	mod->ap_object = ap_module_registry_get_module(registry, AP_OBJECT_MODULE_NAME);
	if (!mod->ap_object)
		return FALSE;
	ap_object_add_stream_callback(mod->ap_object, AP_OBJECT_MDI_OBJECT,
		"Plugin_BossSpawn", mod, obj_read, obj_write);
	return TRUE;
}

struct ap_plugin_boss_spawn_module * ap_plugin_boss_spawn_create_module()
{
	struct ap_plugin_boss_spawn_module * mod = ap_module_instance_new(
		AP_PLUGIN_BOSS_SPAWN_MODULE_NAME, sizeof(*mod), onregister, NULL, NULL, NULL);
	return mod;
}
