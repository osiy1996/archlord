#include "public/ap_event_binding.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_event_manager.h"
#include "public/ap_object.h"

#include <assert.h>
#include <stdlib.h>

#define	STREAM_BINDING_NAME "BindingName"
#define	STREAM_TOWN_NAME "TownName"
#define	STREAM_RADIUS "Radius"
#define	STREAM_BINDING_TYPE "BindingType"
#define STREAM_EVENT_END "BindingEnd"
#define	STREAM_CHARACTER_RACE_TYPE "CharRaceType"
#define	STREAM_CHARACTER_CLASS_TYPE "CharClassType"

struct ap_event_binding_module {
	struct ap_module_instance instance;
	struct ap_event_manager_module * ap_event_manager;
};

static boolean event_ctor(struct ap_event_binding_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	e->data = alloc(sizeof(struct ap_event_binding_event));
	memset(e->data, 0, sizeof(struct ap_event_binding_event));
	return TRUE;
}

static boolean event_dtor(struct ap_event_binding_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	dealloc(e->data);
	e->data = NULL;
	return TRUE;
}

static boolean event_read(
	struct ap_event_binding_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_event_binding_event * e = ap_event_binding_get_event(mod, data);
	struct ap_event_binding * b = &e->binding;
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = 
			ap_module_stream_get_value_name(stream);
		if (strcmp(value_name, STREAM_BINDING_NAME) == 0) {
			if (!ap_module_stream_get_str(stream, b->binding_name,
					sizeof(b->binding_name))) {
				ERROR("Failed to read binding name.");
				return FALSE;
			}
		}
		else if (strcmp(value_name, STREAM_TOWN_NAME) == 0) {
			if (!ap_module_stream_get_str(stream, b->town_name,
					sizeof(b->town_name))) {
				ERROR("Failed to read binding town name.");
				return FALSE;
			}
		}
		else if (strcmp(value_name, STREAM_RADIUS) == 0) {
			if (!ap_module_stream_get_i32(stream, &b->radius)) {
				ERROR("Failed to read binding radius.");
				return FALSE;
			}
		}
		else if (strcmp(value_name, STREAM_BINDING_TYPE) == 0) {
			if (!ap_module_stream_get_i32(stream, 
					(int32_t *)&b->binding_type)) {
				ERROR("Failed to read binding type.");
				return FALSE;
			}
		}
		else if (strcmp(value_name, STREAM_CHARACTER_RACE_TYPE) == 0) {
			if (!ap_module_stream_get_i32(stream, 
					&b->char_type.race)) {
				ERROR("Failed to read binding character race.");
				return FALSE;
			}
		}
		else if (strcmp(value_name, STREAM_CHARACTER_CLASS_TYPE) == 0) {
			if (!ap_module_stream_get_i32(stream, 
					&b->char_type.cs)) {
				ERROR("Failed to read binding character class.");
				return FALSE;
			}
		}
		else if (strcmp(value_name, STREAM_EVENT_END) == 0) {
			break;
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

static boolean event_write(
	struct ap_event_binding_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_event_binding_event * e = ap_event_binding_get_event(mod, data);
	struct ap_event_binding * b = &e->binding;
	if (!ap_module_stream_write_value(stream, 
			STREAM_BINDING_NAME, b->binding_name)) {
		ERROR("Failed to write binding name.");
		return FALSE;
	}
	if (!strisempty(b->town_name) &&
		!ap_module_stream_write_value(stream, 
			STREAM_TOWN_NAME, b->town_name)) {
		ERROR("Failed to write binding town name.");
		return FALSE;
	}
	if (!ap_module_stream_write_i32(stream, 
			STREAM_RADIUS, b->radius)) {
		ERROR("Failed to write binding radius.");
		return FALSE;
	}
	if (!ap_module_stream_write_i32(stream, 
			STREAM_BINDING_TYPE, b->binding_type)) {
		ERROR("Failed to write binding type.");
		return FALSE;
	}
	if (!ap_module_stream_write_i32(stream, 
			STREAM_CHARACTER_RACE_TYPE, b->char_type.race)) {
		ERROR("Failed to write binding character race.");
		return FALSE;
	}
	if (!ap_module_stream_write_i32(stream, 
			STREAM_CHARACTER_CLASS_TYPE, b->char_type.cs)) {
		ERROR("Failed to write binding character class.");
		return FALSE;
	}
	if (!ap_module_stream_write_i32(stream, STREAM_EVENT_END, 0)) {
		ERROR("Failed to end binding stream.");
		return FALSE;
	}
	return TRUE;
}

static boolean onregister(
	struct ap_event_binding_module * mod,
	struct ap_module_registry * registry)
{
	mod->ap_event_manager = ap_module_registry_get_module(registry, AP_EVENT_MANAGER_MODULE_NAME);
	if (!mod->ap_event_manager) {
		ERROR("Failed to retrieve module (%s).", AP_EVENT_MANAGER_MODULE_NAME);
		return FALSE;
	}
	if (!ap_event_manager_register_event(mod->ap_event_manager,
			AP_EVENT_MANAGER_FUNCTION_BINDING, mod, event_ctor, event_dtor, 
			event_read, event_write, NULL)) {
		ERROR("Failed to register event.");
		return FALSE;
	}
	return TRUE;
}

struct ap_event_binding_module * ap_event_binding_create_module()
{
	struct ap_event_binding_module * mod = ap_module_instance_new(AP_EVENT_BINDING_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	return mod;
}

struct ap_event_binding_event * ap_event_binding_get_event(
	struct ap_event_binding_module * mod,
	struct ap_event_manager_event * e)
{
	return e->data;
}
