#include "client/ac_event_effect.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_event_manager.h"
#include "public/ap_object.h"

#include <assert.h>
#include <stdlib.h>

#define STREAM_MONO "M"
#define STREAM_EFFECT_SOUND_PATH "SOUND\\EFFECT\\"
#define STREAM_EFFECT_MONO_SOUND_PATH "SOUND\\EFFECT\\"
#define STREAM_CUSTOM_FLAGS "AGCMEVENTEFFECT_CUSTOM_FLAGS"
#define STREAM_CUSTOM_FLAGS_LENGTH 28
#define STREAM_CONDITION_FLAGS "AGCMEVENTEFFECT_CONDITION_FLAGS"
#define STREAM_CONDITION_FLAGS_LENGTH 31
#define STREAM_SS_CONDITION_FLAGS "AGCMEVENTEFFECT_SS_CONDITION_FLAGS"
#define STREAM_STATUS_FLAGS "AGCMEVENTEFFECT_STATUS_FLAGS"
#define STREAM_EFFECT_DATA "AGCMEVENTEFFECT_EFFECT_DATA"
#define STREAM_EFFECT_DATA_LENGTH 27
#define STREAM_EFFECT_ROTATION "AGCMEVENTEFFECT_ROTATION_DATA"
#define STREAM_EFFECT_ROTATION_LENGTH 29
#define STREAM_SOUND_DATA "AGCMEVENTEFFECT_SOUND_DATA"
#define STREAM_SOUND_DATA_LENGTH 26
#define STREAM_EFFECT_CUST_DATA "AGCMEVENTEFFECT_CUST_DATA"
#define STREAM_EFFECT_CUST_DATA_LENGTH 25
#define STREAM_ANIMATION_DATA "AGCMEVENTEFFECT_ANIMATION_DATA"
#define STREAM_CHAR_ANIM_ATTACHED_DATA_SOUND "AGCMEVENTEFFECT_CAAD_SOUND_DATA"
#define STREAM_CHAR_ANIM_ATTACHED_DATA_SOUND_CONDITION "AGCMEVENTEFFECT_CAAD_SOUND_CONDITION_FLAGS"

size_t g_AcEventEffectObjectTemplateAttachmentOffset = 0;

struct ac_event_effect_module {
	struct ap_module_instance instance;
	struct ap_object_module * ap_object;
};

static struct ac_event_effect_use_effect_set_data * getdata(
	struct ac_event_effect_use_effect_set * set,
	uint32_t index)
{
	struct ac_event_effect_use_effect_set_list * list = set->list;
	struct ac_event_effect_use_effect_set_list * last = NULL;
	while (list) {
		if (list->data.index == index)
			return &list->data;
		last = list;
		list = list->next;
	}
	list = alloc(sizeof(*list));
	memset(list, 0, sizeof(*list));
	list->data.index = index;
	if (last)
		last->next = list;
	else
		set->list = list;
	return &list->data;
}

static boolean useeffectsetstreamread(
	struct ac_event_effect_module * mod,
	struct ac_event_effect_use_effect_set * set,
	struct ap_module_stream * stream)
{
	while (ap_module_stream_read_next_value(stream)) {
		const char * valuename = ap_module_stream_get_value_name(stream);
		const char * value = ap_module_stream_get_value(stream);
		uint32_t index = -1;
		struct ac_event_effect_use_effect_set_data * data;
		if (strncmp(valuename, STREAM_CONDITION_FLAGS, STREAM_CONDITION_FLAGS_LENGTH) == 0) {
			uint32_t flags;
			if (sscanf(value, "%d:%d", &index, &flags) != 2) {
				ERROR("Failed to read effect condition flags.");
				return FALSE;
			}
			data = getdata(set, index);
			data->condition_flags = flags;
			set->condition_flags |= flags;
		}
		else if (strncmp(valuename, STREAM_EFFECT_DATA, STREAM_EFFECT_DATA_LENGTH) == 0) {
			int parentnodeid = 0;
			uint32_t eid = 0;
			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;
			float scale = 0.0f;
			uint32_t startgap = 0;
			if (sscanf(value, "%d:%d:%f:%f:%f:%f:%d:%d", &index, &eid, 
					&x, &y, &z, &scale, &parentnodeid, &startgap) != 8) {
				ERROR("Failed to read effect data.");
				return FALSE;
			}
			data = getdata(set, index);
			data->eid = eid;
			data->offset.x = x;
			data->offset.y = y;
			data->offset.z = z;
			data->scale = scale;
			data->parent_node_id = parentnodeid;
			data->start_gap = startgap;
		}
		else if (strncmp(valuename, STREAM_EFFECT_CUST_DATA, STREAM_EFFECT_CUST_DATA_LENGTH) == 0) {
			char customdata[AC_EVENT_EFFECT_MAX_CUSTOM_DATA_SIZE];
			if (sscanf(value, "%d:%s", &index, customdata) != 2) {
				ERROR("Failed to read effect custom data.");
				return FALSE;
			}
			data = getdata(set, index);
			if (!strisempty(data->custom_data))
				return FALSE;
			strlcpy(data->custom_data, customdata, sizeof(data->custom_data));
		}
		else if (strncmp(valuename, STREAM_SOUND_DATA, STREAM_SOUND_DATA_LENGTH) == 0) {
			char soundname[AC_EVENT_EFFECT_MAX_SOUND_NAME_SIZE];
			if (sscanf(value, "%d:%s", &index, soundname) != 2) {
				ERROR("Failed to read effect sound data.");
				return FALSE;
			}
			data = getdata(set, index);
			if (!strisempty(data->sound_name))
				return FALSE;
			strlcpy(data->sound_name, soundname, sizeof(data->sound_name));
		}
		else if (strncmp(valuename, STREAM_EFFECT_ROTATION, STREAM_EFFECT_ROTATION_LENGTH) == 0) {
			struct ac_event_effect_use_effect_set_data_rotation rotation = { 0 };
			if (sscanf(value, "%d:%f:%f:%f:%f:%f:%f:%f:%f:%f", 
					&index,
					&rotation.right.x,
					&rotation.right.y,
					&rotation.right.z,
					&rotation.up.x,
					&rotation.up.y,
					&rotation.up.z,
					&rotation.at.x,
					&rotation.at.y,
					&rotation.at.z) != 10) {
				ERROR("Failed to read effect rotation data.");
				return FALSE;
			}
			data = getdata(set, index);
			if (data->rotation)
				return FALSE;
			data->rotation = alloc(sizeof(rotation));
			if (!data->rotation)
				return FALSE;
			memcpy(data->rotation, &rotation, sizeof(rotation));
		}
		else if (strncmp(valuename, STREAM_CUSTOM_FLAGS, STREAM_CUSTOM_FLAGS_LENGTH) == 0) {
			if (sscanf(value, "%d", &set->custom_flags) != 1) {
				ERROR("Failed to read effect set custom flags.");
				return FALSE;
			}
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

static boolean useeffectsetstreamwrite(
	struct ac_event_effect_module * mod,
	struct ac_event_effect_use_effect_set * set,
	struct ap_module_stream * stream)
{
	boolean result = TRUE;
	char valuename[256];
	char value[256];
	struct ac_event_effect_use_effect_set_list * list;
	if (set->custom_flags)
		result &= ap_module_stream_write_i32(stream, STREAM_CUSTOM_FLAGS, set->custom_flags);
	list = set->list;
	while (list) {
		struct ac_event_effect_use_effect_set_data * data = &list->data;
		if (!data->eid && strisempty(data->sound_name)) {
			list = list->next;
			continue;
		}
		if (data->condition_flags) {
			snprintf(valuename, sizeof(valuename), "%s%d", STREAM_CONDITION_FLAGS, data->index);
			snprintf(value, sizeof(value), "%d:%d", data->index, data->condition_flags);
			result &= ap_module_stream_write_value(stream, valuename, value);
		}
		snprintf(valuename, sizeof(valuename), "%s%d", STREAM_EFFECT_DATA, data->index);
		au_ini_mgr_print_compact(value, "%d:%d:%f:%f:%f:%f:%d:%d", data->index, 
			data->eid, 
			data->offset.x, 
			data->offset.y, 
			data->offset.z, 
			data->scale,
			data->parent_node_id,
			data->start_gap);
		result &= ap_module_stream_write_value(stream, valuename, value);
		if (data->rotation) {
			snprintf(valuename, sizeof(valuename), "%s%d", STREAM_EFFECT_ROTATION, data->index);
			au_ini_mgr_print_compact(value, "%d:%f:%f:%f:%f:%f:%f:%f:%f:%f", 
				data->index, 
				data->rotation->right.x,
				data->rotation->right.y,
				data->rotation->right.z,
				data->rotation->up.x,
				data->rotation->up.y,
				data->rotation->up.z,
				data->rotation->at.x,
				data->rotation->at.y,
				data->rotation->at.z);
			result &= ap_module_stream_write_value(stream, valuename, value);
		}
		if (!strisempty(data->custom_data)) {
			snprintf(valuename, sizeof(valuename), "%s%d", STREAM_EFFECT_CUST_DATA, data->index);
			snprintf(value, sizeof(value), "%d:%s", data->index, data->custom_data);
			result &= ap_module_stream_write_value(stream, valuename, value);
		}
		if (!strisempty(data->sound_name)) {
			snprintf(valuename, sizeof(valuename), "%s%d", STREAM_SOUND_DATA, data->index);
			snprintf(value, sizeof(value), "%d:%s", data->index, data->sound_name);
			result &= ap_module_stream_write_value(stream, valuename, value);
		}
		list = list->next;
	}
	return result;
}

static boolean cbobjecttemplatestreamread(
	struct ac_event_effect_module * mod,
	void * data,
	struct ap_module_stream * stream)
{
	return useeffectsetstreamread(mod, 
		ac_event_effect_get_object_template_attachment(data), stream);
}

static boolean cbobjecttemplatestreamwrite(
	struct ac_event_effect_module * mod,
	void * data,
	struct ap_module_stream * stream)
{
	return useeffectsetstreamwrite(mod, 
		ac_event_effect_get_object_template_attachment(data), stream);
}

static boolean onregister(
	struct ac_event_effect_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_object, AP_OBJECT_MODULE_NAME);
	g_AcEventEffectObjectTemplateAttachmentOffset = ap_object_attach_data(mod->ap_object, 
		AP_OBJECT_MDI_OBJECT_TEMPLATE, sizeof(struct ac_event_effect_use_effect_set), 
		mod, NULL, NULL);
	ap_object_add_stream_callback(mod->ap_object, AP_OBJECT_MDI_OBJECT_TEMPLATE,
		AC_EVENT_EFFECT_MODULE_NAME, mod, 
		cbobjecttemplatestreamread, cbobjecttemplatestreamwrite);
	return TRUE;
}

struct ac_event_effect_module * ac_event_effect_create_module()
{
	struct ac_event_effect_module * mod = ap_module_instance_new(
		AC_EVENT_EFFECT_MODULE_NAME, sizeof(*mod), 
		onregister, NULL, NULL, NULL);
	return mod;
}
