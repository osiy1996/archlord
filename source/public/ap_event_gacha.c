#include <assert.h>
#include <stdlib.h>

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_event_gacha.h"
#include "public/ap_event_manager.h"
#include "public/ap_object.h"

#define STREAM_GACHA_START "GachaStart"
#define STREAM_GACHA_END "GachaEnd"
#define STREAM_GACHA_TYPE "Type"

struct ap_event_gacha_module {
	struct ap_module_instance instance;
	struct ap_event_manager_module * ap_event_manager;
};

static boolean event_ctor(struct ap_event_gacha_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	e->data = alloc(sizeof(struct ap_event_gacha_event));
	memset(e->data, 0, sizeof(struct ap_event_gacha_event));
	return TRUE;
}

static boolean event_dtor(struct ap_event_gacha_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	dealloc(e->data);
	e->data = NULL;
	return TRUE;
}

static boolean event_read(
	struct ap_event_gacha_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_event_gacha_event * e = ap_event_gacha_get_event(data);
	if (!ap_module_stream_read_next_value(stream))
		return TRUE;
	if (strcmp(ap_module_stream_get_value_name(stream), 
			STREAM_GACHA_START) != 0)
		return TRUE;
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = 
			ap_module_stream_get_value_name(stream);
		if (strcmp(value_name, STREAM_GACHA_TYPE) == 0) {
			if (!ap_module_stream_get_i32(stream, &e->gacha_type)) {
				ERROR("Failed to read event gacha type.");
				return FALSE;
			}
		}
		else if (strcmp(value_name, STREAM_GACHA_END) == 0) {
			break;
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

static boolean event_write(
	struct ap_event_gacha_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_event_gacha_event * e = ap_event_gacha_get_event(data);
	if (!ap_module_stream_write_i32(stream, STREAM_GACHA_START, 0) ||
		!ap_module_stream_write_i32(stream, STREAM_GACHA_TYPE, 
			e->gacha_type) ||
		!ap_module_stream_write_i32(stream, STREAM_GACHA_END, 0)) {
		ERROR("Failed to write event gacha stream.");
		return FALSE;
	}
	return TRUE;
}

static boolean onregister(
	struct ap_event_gacha_module * mod,
	struct ap_module_registry * registry)
{
	mod->ap_event_manager = ap_module_registry_get_module(registry, AP_EVENT_MANAGER_MODULE_NAME);
	if (!mod->ap_event_manager) {
		ERROR("Failed to retrieve module (%s).", AP_EVENT_MANAGER_MODULE_NAME);
		return FALSE;
	}
	if (!ap_event_manager_register_event(mod->ap_event_manager,
			AP_EVENT_MANAGER_FUNCTION_GACHA, 
			mod, event_ctor, event_dtor, event_read, event_write, NULL)) {
		ERROR("Failed to register event.");
		return FALSE;
	}
	return TRUE;
}

struct ap_event_gacha_module * ap_event_gacha_create_module()
{
	struct ap_event_gacha_module * mod = ap_module_instance_new(AP_EVENT_GACHA_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	return mod;
}

struct ap_event_gacha_event * ap_event_gacha_get_event(struct ap_event_manager_event * e)
{
	return e->data;
}
