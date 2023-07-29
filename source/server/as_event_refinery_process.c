#include "server/as_event_refinery_process.h"

#include "core/log.h"

#include "public/ap_character.h"
#include "public/ap_event_refinery.h"
#include "public/ap_optimized_packet2.h"

#include "server/as_map.h"
#include "server/as_player.h"

struct character_attachment {
	struct au_pos target_position;
	struct ap_event_manager_event * event;
};

struct as_event_refinery_process_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_event_refinery_module * ap_event_refinery;
	struct ap_optimized_packet2_module * ap_optimized_packet2;
	struct as_map_module * as_map;
	struct as_player_module * as_player;
	size_t character_attachment_offset;
};

static struct character_attachment * getcharattachment(
	struct as_event_refinery_process_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->character_attachment_offset);
}

static struct ap_event_manager_event * findevent(
	struct as_event_refinery_process_module * mod,
	void * source)
{
	return ap_event_manager_get_function(mod->ap_event_manager, source, 
		AP_EVENT_MANAGER_FUNCTION_REFINERY);
}

static boolean cbcharstopaction(
	struct as_event_refinery_process_module * mod,
	struct ap_character_cb_stop_action * cb)
{
	struct character_attachment * attachment = getcharattachment(mod, cb->character);
	attachment->event = NULL;
	return TRUE;
}

static boolean cbcharprocess(
	struct as_event_refinery_process_module * mod,
	struct ap_character_cb_process * cb)
{
	struct character_attachment * attachment = getcharattachment(mod, cb->character);
	float distance;
	if (!attachment->event)
		return TRUE;
	distance = au_distance2d(&attachment->target_position, &cb->character->pos);
	if (distance > AP_EVENT_REFINERY_MAX_USE_RANGE)
		return TRUE;
	ap_event_refinery_make_packet(mod->ap_event_refinery, 
		AP_EVENT_REFINERY_PACKET_GRANT, attachment->event,
		&cb->character->id);
	as_player_send_packet(mod->as_player, cb->character);
	ap_character_stop_movement(mod->ap_character, cb->character);
	ap_character_stop_action(mod->ap_character, cb->character);
	return TRUE;
}

static boolean cbreceive(
	struct as_event_refinery_process_module * mod,
	struct ap_event_refinery_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	switch (cb->type) {
	case AP_EVENT_REFINERY_PACKET_REQUEST: {
		struct ap_event_manager_event * e;
		float distance;
		struct character_attachment * attachment;
		void * source = NULL;
		struct au_pos * position = NULL;
		switch (cb->event->source_type) {
		case AP_BASE_TYPE_OBJECT:
			source = as_map_find_object(mod->as_map, cb->event->source_id, &c->pos);
			if (!source)
				return TRUE;
			position = &((struct ap_object *)source)->position;
			break;
		case AP_BASE_TYPE_CHARACTER:
			source = as_map_get_npc(mod->as_map, cb->event->source_id);
			if (!source)
				return TRUE;
			position = &((struct ap_character *)source)->pos;
			break;
		default:
			return TRUE;
		}
		e = findevent(mod, source);
		if (!e)
			break;
		ap_character_stop_action(mod->ap_character, c);
		distance = au_distance2d(position, &c->pos);
		if (distance > AP_EVENT_REFINERY_MAX_USE_RANGE) {
			vec2 dir = { position->x - c->pos.x, position->z - c->pos.z };
			float len = distance - 0.80f * AP_EVENT_REFINERY_MAX_USE_RANGE;
			struct au_pos dst = { 0 };
			glm_vec2_normalize(dir);
			dst.x = c->pos.x + dir[0] * len;
			dst.y = c->pos.y;
			dst.z = c->pos.z + dir[1] * len;
			ap_character_set_movement(mod->ap_character, c, &dst, TRUE);
		}
		attachment = getcharattachment(mod, c);
		attachment->target_position = *position;
		attachment->event = e;
		break;
	}
	}
	return TRUE;
}

static boolean onregister(
	struct as_event_refinery_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_refinery, AP_EVENT_REFINERY_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_optimized_packet2, AP_OPTIMIZED_PACKET2_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	mod->character_attachment_offset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct character_attachment), mod, NULL, NULL);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_STOP_ACTION, mod, cbcharstopaction);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_PROCESS, mod, cbcharprocess);
	ap_event_refinery_add_callback(mod->ap_event_refinery, AP_EVENT_REFINERY_CB_RECEIVE, mod, cbreceive);
	return TRUE;
}

static void onshutdown(struct as_event_refinery_process_module * mod)
{
}

struct as_event_refinery_process_module * as_event_refinery_process_create_module()
{
	struct as_event_refinery_process_module * mod = ap_module_instance_new(AS_EVENT_REFINERY_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	return mod;
}
