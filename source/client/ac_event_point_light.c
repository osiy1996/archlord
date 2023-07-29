#include "client/ac_event_point_light.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_event_manager.h"
#include "public/ap_event_point_light.h"

#include <assert.h>
#include <stdlib.h>

#define	STREAM_RGB "Color"
#define	STREAM_RADIUS "Radius"
#define	STREAM_FLAG "Flag"

struct ac_event_point_light_module {
	struct ap_module_instance instance;
	struct ap_event_point_light_module * ap_event_point_light;
	size_t data_offset;
};

static boolean stream_read(
	struct ac_event_point_light_module * mod,
	void * data, 
	struct ap_module_stream * stream)
{
	struct ac_event_point_light_data * d = ac_event_point_light_get_data(mod, data);
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = 
			ap_module_stream_get_value_name(stream);
		if (strcmp(value_name, STREAM_RGB) == 0) {
			int32_t value;
			if (!ap_module_stream_get_i32(stream, &value)) {
				ERROR("Failed to read point light RGB.");
				return FALSE;
			}
			d->red = (uint8_t)((value >> 0) & 0xFF);
			d->green = (uint8_t)((value >> 8) & 0xFF);
			d->blue = (uint8_t)((value >> 16) & 0xFF);
		}
		else if (strcmp(value_name, STREAM_RADIUS) == 0) {
			if (!ap_module_stream_get_f32(stream, &d->radius)) {
				ERROR("Failed to read point light radius.");
				return FALSE;
			}
		}
		else if (strcmp(value_name, STREAM_FLAG) == 0) {
			if (!ap_module_stream_get_i32(stream, &d->enable_flag)) {
				ERROR("Failed to read point light flag.");
				return FALSE;
			}
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

static boolean stream_write(
	struct ac_event_point_light_module * mod,
	void * data, 
	struct ap_module_stream * stream)
{
	struct ac_event_point_light_data * d = ac_event_point_light_get_data(mod, data);
	uint32_t rgb = d->red | (d->green << 8) | (d->blue << 16);
	if (!ap_module_stream_write_i32(stream, STREAM_RGB, rgb) ||
		!ap_module_stream_write_f32(stream, 
			STREAM_RADIUS, d->radius) ||
		!ap_module_stream_write_i32(stream, 
			STREAM_FLAG, d->enable_flag)) {
		ERROR("Failed to write event point light stream.");
		return FALSE;
	}
	return TRUE;
}

static boolean onregister(
	struct ac_event_point_light_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_point_light, 
		AP_EVENT_POINT_LIGHT_MODULE_NAME);
	ap_event_point_light_add_stream_callback(mod->ap_event_point_light,
		AC_EVENT_POINT_LIGHT_MODULE_NAME, mod, stream_read, stream_write);
	mod->data_offset = ap_event_point_light_attach_data(mod->ap_event_point_light,
		AP_EVENT_POINT_LIGHT_MDI_EVENT_DATA, sizeof(struct ac_event_point_light_data),
		mod, NULL, NULL);
	return TRUE;
}

struct ac_event_point_light_module * ac_event_point_light_create_module()
{
	struct ac_event_point_light_module * mod = ap_module_instance_new(
		AC_EVENT_POINT_LIGHT_MODULE_NAME, sizeof(*mod), 
		onregister, NULL, NULL, NULL);
	return mod;
}

struct ac_event_point_light_data * ac_event_point_light_get_data(
	struct ac_event_point_light_module * mod,
	struct ap_event_point_light_data * data)
{
	return ap_module_get_attached_data(data, mod->data_offset);
}
