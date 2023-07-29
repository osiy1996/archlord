#include "public/ap_event_manager.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_base.h"
#include "public/ap_character.h"
#include "public/ap_module.h"
#include "public/ap_object.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"

#include <assert.h>
#include <stdlib.h>

#define CONDITION_MAX_TRY 10
#define FLAG_PACKET_TYPE 0x80
#define	MAX_RANGE_TO_ARISE_EVENT 600

#define STREAM_NAME_FUNCTION "Function"
#define STREAM_NAME_EID "EventID"
#define STREAM_NAME_COND_START "CondStart"
#define STREAM_NAME_COND_END "CondEnd"
#define STREAM_NAME_COND_TYPE "CondType"
#define STREAM_NAME_TARGET_ITID "CondTargetItemTID"
#define STREAM_NAME_AREA_TYPE "CondAreaType"
#define STREAM_NAME_AREA_SPHERE_RADIUS "CondAreaSphereRadius"
#define STREAM_NAME_AREA_FAN_RADIUS "CondAreaFanRadius"
#define STREAM_NAME_AREA_ANGLE "CondAreaAngle"
#define STREAM_NAME_AREA_BOX_INF "CondAreaBoxInf"
#define STREAM_NAME_AREA_BOX_SUP "CondAreaBoxSup"

struct ap_event_manager_module {
	struct ap_module_instance instance;
	struct ap_base_module * ap_base;
	struct ap_character_module * ap_character;
	struct ap_object_module * ap_object;
	struct ap_packet_module * ap_packet;
	size_t object_ad_offset;
	size_t character_ad_offset;
	ap_module_t event_module[AP_EVENT_MANAGER_FUNCTION_COUNT];
	ap_module_default_t event_ctor[AP_EVENT_MANAGER_FUNCTION_COUNT];
	ap_module_default_t event_dtor[AP_EVENT_MANAGER_FUNCTION_COUNT];
	ap_module_stream_read_t event_stream_reader[AP_EVENT_MANAGER_FUNCTION_COUNT];
	ap_module_stream_write_t event_stream_writer[AP_EVENT_MANAGER_FUNCTION_COUNT];
	void * event_user_data[AP_EVENT_MANAGER_FUNCTION_COUNT];
	ap_module_default_t event_make_packet[AP_EVENT_MANAGER_FUNCTION_COUNT];
	ap_module_default_t event_receive_packet[AP_EVENT_MANAGER_FUNCTION_COUNT];
	struct au_packet packet;
	struct au_packet packet_event;
};

static void destroyattachment(
	struct ap_event_manager_module * mod,
	struct ap_event_manager_attachment * a)
{
	uint32_t i;
	for (i = 0; i < a->event_count; i++) {
		int fn = a->events[i].function;
		if (mod->event_dtor[fn])
			mod->event_dtor[fn](mod->event_module[fn], &a->events[i]);
	}
}

static boolean streamread(
	struct ap_event_manager_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	enum ap_base_type type;
	uint32_t id;
	struct ap_event_manager_attachment * edata = 
		ap_event_manager_get_attachment(mod, data);
	size_t len;
	if (!ap_module_stream_read_next_value(stream))
		return TRUE;
	len = strlen(STREAM_NAME_FUNCTION);
	edata->event_count = 0;
	type = ap_base_get_type(data);
	id = ap_base_get_id(data);
	while (TRUE) {
		const char * value_name = ap_module_stream_get_value_name(stream);
		const char * value = ap_module_stream_get_value(stream);
		struct ap_event_manager_event * e; 
		uint32_t index = 0;
		if (strncmp(value_name, STREAM_NAME_FUNCTION, len) != 0)
			return TRUE;
		if (edata->event_count >= COUNT_OF(edata->events)) {
			ERROR("Exceeded max. number of events (id = %u).", id);
			return FALSE;
		}
		e = &edata->events[edata->event_count];
		e->function = strtol(value, NULL, 10);
		if (!AP_EVENT_MANAGER_IS_FUNCTION_VALID(e->function)) {
			ERROR("Invalid event function type (id = %u).", id);
			return FALSE;
		}
		if (!ap_module_stream_read_next_value(stream))
			return TRUE;
		value_name = ap_module_stream_get_value_name(stream);
		value = ap_module_stream_get_value(stream);
		if (strcmp(value_name, STREAM_NAME_EID) != 0)
			return FALSE;
		e->eid = strtol(value, NULL, 10);
		e->source = data;
		/* TODO: Check local-generate flag. */
		edata->event_count++;
		if (mod->event_ctor[e->function])
			mod->event_ctor[e->function](mod->event_module[e->function], e);
		if (!ap_module_stream_read_next_value(stream))
			return TRUE;
		value_name = ap_module_stream_get_value_name(stream);
		value = ap_module_stream_get_value(stream);
		if (strcmp(value_name, STREAM_NAME_COND_START) != 0)
			return FALSE;
		while (strcmp(value_name, STREAM_NAME_COND_END) != 0) {
			while (TRUE) {
				if (!ap_module_stream_read_next_value(stream))
					return TRUE;
				value_name = ap_module_stream_get_value_name(stream);
				value = ap_module_stream_get_value(stream);
				if (strcmp(value_name, STREAM_NAME_COND_TYPE) == 0) {
					ap_event_manager_set_cond(mod, e, strtol(value, NULL, 10));
				}
				else if (strcmp(value_name, STREAM_NAME_TARGET_ITID) == 0) {
					assert(e->cond.target != NULL);
					assert(index < AP_EVENT_MANAGER_TARGET_NUMBER);
					if (index < AP_EVENT_MANAGER_TARGET_NUMBER) {
						e->cond.target->item_tid[index++] = 
							strtol(value, NULL, 10);
					}
				}
				else if (strcmp(value_name, STREAM_NAME_AREA_TYPE) == 0) {
					assert(e->cond.area != NULL);
					e->cond.area->type = strtol(value, NULL, 10);
				}
				else if (strcmp(value_name, STREAM_NAME_AREA_SPHERE_RADIUS) == 0) {
					assert(e->cond.area != NULL);
					e->cond.area->data.sphere_radius = 
						strtof(value, NULL);
				}
				else if (strcmp(value_name, STREAM_NAME_AREA_FAN_RADIUS) == 0) {
					assert(e->cond.area != NULL);
					e->cond.area->data.pan.radius = 
						strtof(value, NULL);
				}
				else if (strcmp(value_name, STREAM_NAME_AREA_ANGLE) == 0) {
					assert(e->cond.area != NULL);
					e->cond.area->data.pan.angle = 
						strtof(value, NULL);
				}
				else if (strcmp(value_name, STREAM_NAME_AREA_BOX_INF) == 0) {
					assert(e->cond.area != NULL);
					if (sscanf(value, "%f,%f,%f", 
						&e->cond.area->data.box.inf.x,
						&e->cond.area->data.box.inf.y,
						&e->cond.area->data.box.inf.z) != 3) {
						return FALSE;
					}
				}
				else if (strcmp(value_name, STREAM_NAME_AREA_BOX_SUP) == 0) {
					assert(e->cond.area != NULL);
					if (sscanf(value, "%f,%f,%f", 
						&e->cond.area->data.box.sup.x,
						&e->cond.area->data.box.sup.y,
						&e->cond.area->data.box.sup.z) != 3) {
						return FALSE;
					}
				}
				else if (strcmp(value_name, STREAM_NAME_COND_END) == 0) {
					break;
				}
				else {
					assert(0);
				}
			}
		}
		if (mod->event_stream_reader[e->function] &&
			!mod->event_stream_reader[e->function](mod->event_module[e->function], e, stream)) {
			return FALSE;
		}
		if (!ap_module_stream_read_next_value(stream))
			return TRUE;
	}
	return TRUE;
}

static boolean streamwrite(
	struct ap_event_manager_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_event_manager_attachment * edata = 
		ap_event_manager_get_attachment(mod, data);
	uint32_t i;
	for (i = 0; i < edata->event_count; i++) {
		struct ap_event_manager_event * e = &edata->events[i];
		if (!AP_EVENT_MANAGER_IS_FUNCTION_VALID(e->function))
			continue;
		if (!ap_module_stream_write_i32(stream, 
				STREAM_NAME_FUNCTION, e->function) ||
			!ap_module_stream_write_i32(stream, 
				STREAM_NAME_EID, e->eid) ||
			!ap_module_stream_write_i32(stream, 
				STREAM_NAME_COND_START, 0)) {
			return FALSE;
		}
		if (e->cond.target) {
			uint32_t j;
			if (!ap_module_stream_write_i32(stream, 
					STREAM_NAME_COND_TYPE, AP_EVENT_MANAGER_COND_TARGET)) {
				return FALSE;
			}
			for (j = 0; j < AP_EVENT_MANAGER_TARGET_NUMBER && e->cond.target->item_tid[j]; j++) {
				if (!ap_module_stream_write_i32(stream, 
						STREAM_NAME_TARGET_ITID, e->function)) {
					return FALSE;
				}
			}
		}
		if (e->cond.area) {
			struct ap_event_manager_cond_area * area = e->cond.area;
			if (!ap_module_stream_write_i32(stream, 
					STREAM_NAME_COND_TYPE, AP_EVENT_MANAGER_COND_AREA) ||
				!ap_module_stream_write_i32(stream, 
					STREAM_NAME_AREA_TYPE, area->type)) {
				return FALSE;
			}
			switch (area->type) {
			case AP_EVENT_MANAGER_AREA_SPHERE:
				if (!ap_module_stream_write_f32(stream, 
						STREAM_NAME_AREA_SPHERE_RADIUS,
						area->data.sphere_radius)) {
					return FALSE;
				}
				break;
			case AP_EVENT_MANAGER_AREA_FAN:
				if (!ap_module_stream_write_f32(stream, 
						STREAM_NAME_AREA_FAN_RADIUS,
						area->data.pan.radius) ||
					!ap_module_stream_write_f32(stream, 
						STREAM_NAME_AREA_ANGLE,
						area->data.pan.angle)) {
					return FALSE;
				}
				break;
			case AP_EVENT_MANAGER_AREA_BOX: {
				char tmp[128];
				snprintf(tmp, sizeof(tmp), "%f,%f,%f",
					area->data.box.inf.x,
					area->data.box.inf.y,
					area->data.box.inf.z);
				if (!ap_module_stream_write_value(stream, 
						STREAM_NAME_AREA_BOX_INF, tmp)) {
					return FALSE;
				}
				snprintf(tmp, sizeof(tmp), "%f,%f,%f",
					area->data.box.sup.x,
					area->data.box.sup.y,
					area->data.box.sup.z);
				if (!ap_module_stream_write_value(stream, 
						STREAM_NAME_AREA_BOX_SUP, tmp)) {
					return FALSE;
				}
				break;
			}
			}
		}
		if (!ap_module_stream_write_i32(stream, 
				STREAM_NAME_COND_END, 0)) {
			return FALSE;
		}
		if (mod->event_stream_writer[e->function] &&
			!mod->event_stream_writer[e->function](mod->event_module[e->function], e, stream)) {
			return FALSE;
		}
	}
	return TRUE;
}

static boolean objdtor(struct ap_event_manager_module * mod, void * data)
{
	destroyattachment(mod,
		ap_module_get_attached_data(data, mod->object_ad_offset));
	return TRUE;
}

static boolean chardtor(struct ap_event_manager_module * mod, void * data)
{
	destroyattachment(mod,
		ap_module_get_attached_data(data, mod->character_ad_offset));
	return TRUE;
}

static boolean onregister(
	struct ap_event_manager_module * mod,
	struct ap_module_registry * registry)
{
	mod->ap_character = ap_module_registry_get_module(registry, AP_CHARACTER_MODULE_NAME);
	if (!mod->ap_character) {
		ERROR("Failed to retrieve module (%s).", AP_CHARACTER_MODULE_NAME);
		return FALSE;
	}
	mod->character_ad_offset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct ap_event_manager_attachment), 
		mod, NULL, chardtor);
	if (mod->character_ad_offset == SIZE_MAX) {
		ERROR("Failed to attach character data.");
		return FALSE;
	}
	ap_character_add_stream_callback(mod->ap_character, AP_CHARACTER_MDI_STATIC,
		AP_EVENT_MANAGER_MODULE_NAME, mod, streamread, streamwrite);
	mod->ap_object = ap_module_registry_get_module(registry, AP_OBJECT_MODULE_NAME);
	if (!mod->ap_object) {
		ERROR("Failed to retrieve module (%s).", AP_OBJECT_MODULE_NAME);
		return FALSE;
	}
	mod->object_ad_offset = ap_object_attach_data(mod->ap_object,
		AP_OBJECT_MDI_OBJECT, sizeof(struct ap_event_manager_attachment), 
		mod, NULL, objdtor);
	if (mod->object_ad_offset == SIZE_MAX) {
		ERROR("Failed to attach object data.");
		return FALSE;
	}
	ap_object_add_stream_callback(mod->ap_object, AP_OBJECT_MDI_OBJECT,
		AP_EVENT_MANAGER_MODULE_NAME, mod, streamread, streamwrite);
	mod->ap_base = ap_module_registry_get_module(registry, AP_BASE_MODULE_NAME);
	mod->ap_packet = ap_module_registry_get_module(registry, AP_PACKET_MODULE_NAME);
	return (mod->ap_base != NULL && mod->ap_packet != NULL);
}

struct ap_event_manager_module * ap_event_manager_create_module()
{
	struct ap_event_manager_module * mod = ap_module_instance_new(
		AP_EVENT_MANAGER_MODULE_NAME, sizeof(*mod), onregister, NULL, NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint8_t),
		AU_PACKET_TYPE_INT8, 1, /* Source Type */
		AU_PACKET_TYPE_INT32, 1, /* Source ID (Object/Item/Character) */
		AU_PACKET_TYPE_INT32, 1, /* Effect ID */
		AU_PACKET_TYPE_INT32, 1, /* Function ID */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_event, sizeof(uint8_t),
		AU_PACKET_TYPE_PACKET, 1, /* Event Base Packet */
		AU_PACKET_TYPE_PACKET, 1, /* Event Custom Data */
		AU_PACKET_TYPE_END);
	return mod;
}

boolean ap_event_manager_register_event(
	struct ap_event_manager_module * mod,
	enum ap_event_manager_function_type function,
	ap_module_t event_module,
	ap_module_default_t ctor,
	ap_module_default_t dtor,
	ap_module_stream_read_t stream_read,
	ap_module_stream_write_t stream_write,
	void * user_data)
{
	if (!AP_EVENT_MANAGER_IS_FUNCTION_VALID(function))
		return FALSE;
	mod->event_module[function] = event_module;
	mod->event_ctor[function] = ctor;
	mod->event_dtor[function] = dtor;
	mod->event_stream_reader[function] = stream_read;
	mod->event_stream_writer[function] = stream_write;
	mod->event_user_data[function] = user_data;
	return TRUE;
}

boolean ap_event_manager_register_packet(
	struct ap_event_manager_module * mod,
	enum ap_event_manager_function_type function,
	ap_module_default_t make_packet,
	ap_module_default_t receive_packet)
{
	if (!AP_EVENT_MANAGER_IS_FUNCTION_VALID(function))
		return FALSE;
	mod->event_make_packet[function] = make_packet;
	mod->event_receive_packet[function] = receive_packet;
	return TRUE;
}

void ap_event_manager_set_cond(
	struct ap_event_manager_module * mod,
	struct ap_event_manager_event * e, 
	uint32_t cond)
{
	if (cond & AP_EVENT_MANAGER_COND_TARGET) {
		if (!e->cond.target)
			e->cond.target = alloc(sizeof(*e->cond.target));
		memset(e->cond.target, 0, sizeof(*e->cond.target));
	}
	else if (e->cond.target) {
		dealloc(e->cond.target);
		e->cond.target = NULL;
	}
	if (cond & AP_EVENT_MANAGER_COND_AREA) {
		if (!e->cond.area)
			e->cond.area = alloc(sizeof(*e->cond.area));
		memset(e->cond.area, 0, sizeof(*e->cond.area));
	}
	else if (e->cond.area) {
		dealloc(e->cond.area);
		e->cond.area = NULL;
	}
	if (cond & AP_EVENT_MANAGER_COND_TIME) {
		if (!e->cond.time)
			e->cond.time = alloc(sizeof(*e->cond.time));
		memset(e->cond.time, 0, sizeof(*e->cond.time));
	}
	else if (e->cond.time) {
		dealloc(e->cond.time);
		e->cond.time = NULL;
	}
}

struct ap_event_manager_attachment * ap_event_manager_get_attachment(
	struct ap_event_manager_module * mod,
	void * data)
{
	switch (ap_base_get_type(data)) {
	case AP_BASE_TYPE_OBJECT:
		return ap_module_get_attached_data(data, 
			mod->object_ad_offset);
	case AP_BASE_TYPE_CHARACTER:
		return ap_module_get_attached_data(data, 
			mod->character_ad_offset);
	default:
		assert(0);
		return NULL;
	}
}

struct ap_event_manager_event * ap_event_manager_get_function(
	struct ap_event_manager_module * mod,
	void * source,
	enum ap_event_manager_function function)
{
	uint32_t i;
	struct ap_event_manager_attachment * a = 
		ap_event_manager_get_attachment(mod, source);
	if (!a)
		return NULL;
	for (i = 0; i < a->event_count; i++) {
		if (a->events[i].function == function)
			return &a->events[i];
	}
	return NULL;
}

boolean ap_event_manager_get_base_packet(
	struct ap_event_manager_module * mod,
	const void * data,
	struct ap_event_manager_base_packet * packet)
{
	return au_packet_get_field(&mod->packet, FALSE, data, 0,
		&packet->source_type,
		&packet->source_id,
		&packet->eid,
		&packet->function);
}

void ap_event_manager_make_base_packet(
	struct ap_event_manager_module * mod,
	const struct ap_event_manager_event * event,
	void * buffer)
{
	if (!event->eid || 
		(event->eid & AP_EVENT_MANAGER_EID_FLAG_LOCAL)) {
		int32_t type = ap_base_get_type(event->source);
		uint32_t id = ap_base_get_id(event->source);
		au_packet_make_packet(&mod->packet, buffer, 
			FALSE, NULL, 0,
			&type,
			&id,
			NULL,
			&event->function);
	}
	else {
		au_packet_make_packet(&mod->packet, buffer, 
			FALSE, NULL, 0,
			NULL,
			NULL,
			&event->eid,
			NULL);
	}
}

boolean ap_event_manager_make_event_data_packet(
	struct ap_event_manager_module * mod,
	struct ap_event_manager_event * event,
	void * buffer,
	uint16_t * length)
{
	void * base;
	void * custom = NULL;
	if (!AP_EVENT_MANAGER_IS_FUNCTION_VALID(event->function))
		return FALSE;
	base = ap_packet_get_temp_buffer(mod->ap_packet);
	ap_event_manager_make_base_packet(mod, event, base);
	if (mod->event_make_packet[event->function]) {
		struct ap_event_manager_cb_make_packet cb = { event, 
			ap_packet_get_temp_buffer(mod->ap_packet) };
		mod->event_make_packet[event->function](
			mod->event_module[event->function], &cb);
		custom = cb.custom_data;
	}
	au_packet_make_packet(&mod->packet_event, buffer, 
		TRUE, length, AP_EVENT_MANAGER_PACKET_TYPE,
		base,
		custom);
	if (custom)
		ap_packet_pop_temp_buffers(mod->ap_packet, 2);
	else
		ap_packet_pop_temp_buffers(mod->ap_packet, 1);
	return TRUE;
}
