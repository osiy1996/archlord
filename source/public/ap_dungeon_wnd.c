#include <assert.h>
#include <stdlib.h>

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_object.h"
#include "public/ap_dungeon_wnd.h"

#define INI_NAME_DUNGEON "Dungeon"
#define INI_NAME_DUNGEONINFO "DungeonInfo2"

struct ap_dungeon_wnd_module {
	struct ap_module_instance instance;
	struct ap_object_module * ap_object;
	size_t object_ad_offset;
};

static boolean obj_read(
	struct ap_dungeon_wnd_module * mod,
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_dungeon_wnd_object_data * obj = 
		ap_module_get_attached_data(data, mod->object_ad_offset);
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = ap_module_stream_get_value_name(stream);
		const char * value = ap_module_stream_get_value(stream);
		if (strcmp(value_name, INI_NAME_DUNGEON) == 0) {
			obj->is_inside_dungeon = strtol(value, NULL, 10) != 0;
		}
		else if (strcmp(value_name, INI_NAME_DUNGEONINFO) == 0) {
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

static boolean obj_write(
	struct ap_dungeon_wnd_module * mod,
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_object * obj = data;
	struct ap_dungeon_wnd_object_data * dwnd = 
		ap_module_get_attached_data(data, mod->object_ad_offset);
	if (!ap_module_stream_write_i32(stream, INI_NAME_DUNGEON, 
			dwnd->is_inside_dungeon)) {
		ERROR("Failed to write object dungeon data (oid = %u).",
			obj->object_id);
		return FALSE;
	}
	return TRUE;
}

static boolean onregister(
	struct ap_dungeon_wnd_module * mod,
	struct ap_module_registry * registry)
{
	mod->ap_object = ap_module_registry_get_module(registry, AP_OBJECT_MODULE_NAME);
	if (!mod->ap_object)
		return FALSE;
	mod->object_ad_offset = ap_object_attach_data(mod->ap_object, 
		AP_OBJECT_MDI_OBJECT, sizeof(struct ap_dungeon_wnd_object_data), 
		mod, NULL, NULL);
	if (mod->object_ad_offset == SIZE_MAX) {
		ERROR("Failed to add attached object data.");
		return FALSE;
	}
	ap_object_add_stream_callback(mod->ap_object, AP_OBJECT_MDI_OBJECT,
		"DungeonWnd", mod, obj_read, obj_write);
	return TRUE;
}


struct ap_dungeon_wnd_module * ap_dungeon_wnd_create_module()
{
	struct ap_dungeon_wnd_module * mod = ap_module_instance_new(AP_DUNGEON_WND_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	return mod;
}
