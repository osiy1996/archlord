#include "public/ap_event_point_light.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_event_manager.h"

#include <assert.h>
#include <stdlib.h>

#define STREAM_POINT_LIGHT_START "PointLightStart"
#define STREAM_POINT_LIGHT_END "PointLightEnd"

struct ap_event_point_light_module {
	struct ap_module_instance instance;
	struct ap_event_manager_module * ap_event_manager;
	size_t object_ad_offset;
};

static boolean event_ctor(struct ap_event_point_light_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	struct ap_event_point_light_event * edata = 
		alloc(sizeof(*edata));
	edata->data = ap_module_create_module_data(mod, AP_EVENT_POINT_LIGHT_MDI_EVENT_DATA);
	e->data = edata;
	return TRUE;
}

static boolean event_dtor(struct ap_event_point_light_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	struct ap_event_point_light_event * edata = 
		ap_event_point_light_get_event(e);
	ap_module_destroy_module_data(mod, AP_EVENT_POINT_LIGHT_MDI_EVENT_DATA, edata->data);
	dealloc(edata);
	e->data = NULL;
	return TRUE;
}

static boolean event_read(
	struct ap_event_point_light_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_event_point_light_event * e = 
		ap_event_point_light_get_event(data);
	if (!ap_module_stream_read_next_value(stream))
		return TRUE;
	if (strcmp(ap_module_stream_get_value_name(stream), 
			STREAM_POINT_LIGHT_START) != 0)
		return TRUE;
	if (!ap_module_stream_enum_read(mod, stream, 0, e->data)) {
		ERROR("Failed to read event point light stream.");
		return FALSE;
	}
	if (!ap_module_stream_read_next_value(stream))
		return TRUE;
	if (strcmp(ap_module_stream_get_value_name(stream), 
			STREAM_POINT_LIGHT_END) != 0) {
		WARN("Point light stream did not end as expected.");
	}
	return TRUE;
}

static boolean event_write(
	struct ap_event_point_light_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_event_point_light_event * e = ap_event_point_light_get_event(data);
	if (!ap_module_stream_write_i32(stream, STREAM_POINT_LIGHT_START, 0) ||
		!ap_module_stream_enum_write(mod, stream, 0, e->data) ||
		!ap_module_stream_write_i32(stream, STREAM_POINT_LIGHT_END, 0)) {
		ERROR("Failed to write event point light stream.");
		return FALSE;
	}
	return TRUE;
}

static boolean onregister(
	struct ap_event_point_light_module * mod,
	struct ap_module_registry * registry)
{
	mod->ap_event_manager = ap_module_registry_get_module(registry, AP_EVENT_MANAGER_MODULE_NAME);
	if (!mod->ap_event_manager) {
		ERROR("Failed to retrieve module (%s).", AP_EVENT_MANAGER_MODULE_NAME);
		return FALSE;
	}
	if (!ap_event_manager_register_event(mod->ap_event_manager,
			AP_EVENT_MANAGER_FUNCTION_POINTLIGHT, mod,
			event_ctor, event_dtor, event_read, event_write, NULL)) {
		ERROR("Failed to register event.");
		return FALSE;
	}
	return TRUE;
}

struct ap_event_point_light_module * ap_event_point_light_create_module()
{
	struct ap_event_point_light_module * mod = ap_module_instance_new(
		AP_EVENT_POINT_LIGHT_MODULE_NAME, sizeof(*mod), 
		onregister, NULL, NULL, NULL);
	ap_module_set_module_data(mod, AP_EVENT_POINT_LIGHT_MDI_EVENT_DATA, 
		sizeof(struct ap_event_point_light_data), NULL, NULL);
	return mod;
}

void ap_event_point_light_add_stream_callback(
	struct ap_event_point_light_module * mod,
	const char * module_name,
	ap_module_t callback_module,
	ap_module_stream_read_t read_cb,
	ap_module_stream_write_t write_cb)
{
	ap_module_stream_add_callback(mod, 0, module_name, 
		callback_module, read_cb, write_cb);
}

size_t ap_event_point_light_attach_data(
	struct ap_event_point_light_module * mod,
	enum ap_event_point_light_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

struct ap_event_point_light_event * ap_event_point_light_get_event(struct ap_event_manager_event * e)
{
	return e->data;
}
