#include "public/ap_event_skill_master.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_packet.h"

#include "utility/au_packet.h"

#include <assert.h>
#include <stdlib.h>

#define	STREAM_RACE "SkillRaceType"
#define	STREAM_CLASS "SkillClassType"
#define STREAM_END "SkillMasterEnd"

struct ap_event_skill_master_module {
	struct ap_module_instance instance;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
	struct au_packet packet_event_data;
};

static boolean eventctor(struct ap_event_skill_master_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	e->data = alloc(sizeof(struct ap_event_skill_master_data));
	memset(e->data, 0, sizeof(struct ap_event_skill_master_data));
	return TRUE;
}

static boolean eventdtor(struct ap_event_skill_master_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	dealloc(e->data);
	e->data = NULL;
	return TRUE;
}

static boolean eventread(
	struct ap_event_skill_master_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_event_skill_master_data * e = 
		ap_event_skill_master_get_data(data);
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = 
			ap_module_stream_get_value_name(stream);
		if (strcmp(value_name, STREAM_RACE) == 0) {
			if (!ap_module_stream_get_i32(stream, (int32_t *)&e->race)) {
				ERROR("Failed to read event skill master race.");
				return FALSE;
			}
		}
		else if (strcmp(value_name, STREAM_CLASS) == 0) {
			if (!ap_module_stream_get_i32(stream, (int32_t *)&e->class_)) {
				ERROR("Failed to read event skill master class.");
				return FALSE;
			}
		}
		else if (strcmp(value_name, STREAM_END) == 0) {
			break;
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

static boolean eventwrite(
	struct ap_event_skill_master_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_event_skill_master_data * e = 
		ap_event_skill_master_get_data(data);
	if (!ap_module_stream_write_i32(stream, STREAM_RACE, e->race) ||
		!ap_module_stream_write_i32(stream, STREAM_CLASS, e->class_) ||
		!ap_module_stream_write_i32(stream, STREAM_END, 0)) {
		ERROR("Failed to write event skill master stream.");
		return FALSE;
	}
	return TRUE;
}

static boolean cbmakedatapacket(
	struct ap_event_skill_master_module * mod,
	struct ap_event_manager_cb_make_packet * cb)
{
	struct ap_event_skill_master_data * d = 
		ap_event_skill_master_get_data(cb->event);
	au_packet_make_packet(&mod->packet_event_data, 
		cb->custom_data, FALSE, NULL, 0, 
		&d->race,
		&d->class_);
	return TRUE;
}

static boolean onregister(
	struct ap_event_skill_master_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	if (!ap_event_manager_register_event(mod->ap_event_manager,
			AP_EVENT_MANAGER_FUNCTION_SKILLMASTER, mod,
			eventctor, eventdtor, 
			eventread, eventwrite, NULL)) {
		ERROR("Failed to register event.");
		return FALSE;
	}
	if (!ap_event_manager_register_packet(mod->ap_event_manager,
			AP_EVENT_MANAGER_FUNCTION_SKILLMASTER,
			cbmakedatapacket, NULL)) {
		ERROR("Failed to register event packet.");
		return FALSE;
	}
	return TRUE;
}

struct ap_event_skill_master_module * ap_event_skill_master_create_module()
{
	struct ap_event_skill_master_module * mod = ap_module_instance_new(
		AP_EVENT_SKILL_MASTER_MODULE_NAME, sizeof(*mod), 
		onregister, NULL, NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint8_t),
		AU_PACKET_TYPE_UINT8, 1, /* Packet Type */ 
		AU_PACKET_TYPE_PACKET, 1, /* Event Base Packet */
		AU_PACKET_TYPE_INT32, 1, /* Character ID */
		AU_PACKET_TYPE_INT32, 1, /* Skill ID */
		AU_PACKET_TYPE_INT32, 1, /* Skill TID */
		AU_PACKET_TYPE_INT8, 1, /* Operation Result */
		AU_PACKET_TYPE_INT8, 1, /* Skill Point */
		AU_PACKET_TYPE_INT8, 1, /* Is Reset Consumed? */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_event_data, sizeof(uint8_t),
		AU_PACKET_TYPE_INT8, 1, /* Race Type */
		AU_PACKET_TYPE_INT8, 1, /* Class Type */
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_event_skill_master_add_callback(
	struct ap_event_skill_master_module * mod,
	enum ap_event_skill_master_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

boolean ap_event_skill_master_on_receive(
	struct ap_event_skill_master_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_event_skill_master_cb_receive cb = { 0 };
	const void * basepacket = NULL;
	struct ap_event_manager_base_packet event = { 0 };
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type,
			&basepacket,
			&cb.character_id,
			&cb.skill_id,
			&cb.skill_tid,
			NULL, /* Operation Result */
			NULL, /* Skill Point */
			NULL)) { /* Is Reset Consumed? */
		return FALSE;
	}
	if (basepacket) {
		if (!ap_event_manager_get_base_packet(mod->ap_event_manager, basepacket, &event))
			return FALSE;
		cb.event = &event;
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_EVENT_SKILL_MASTER_CB_RECEIVE, &cb);
}

void ap_event_skill_master_make_packet(
	struct ap_event_skill_master_module * mod,
	enum ap_event_skill_master_packet_type type,
	const struct ap_event_manager_event * event,
	const uint32_t * character_id,
	const uint32_t * skill_id,
	const uint32_t * skill_tid,
	uint32_t result,
	const uint8_t * skill_point,
	const boolean * is_reset_consumed)
{
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * base = NULL;
	uint16_t length = 0;
	if (event) {
		base = ap_packet_get_temp_buffer(mod->ap_packet);
		ap_event_manager_make_base_packet(mod->ap_event_manager, event, base);
	}
	au_packet_make_packet(&mod->packet, buffer, 
		TRUE, &length, AP_EVENT_SKILLMASTER_PACKET_TYPE,
		&type,
		base,
		character_id,
		skill_id, /* Skill ID */
		skill_tid, /* Skill TID */
		&result, /* Operation Result */
		skill_point, /* Skill Point */
		is_reset_consumed); /* Is Reset Consumed? */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}
