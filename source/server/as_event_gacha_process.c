#include "server/as_event_gacha_process.h"

#include "core/log.h"

#include "public/ap_character.h"
#include "public/ap_event_gacha.h"
#include "public/ap_event_manager.h"
#include "public/ap_optimized_packet2.h"

#include "server/as_map.h"
#include "server/as_player.h"

#define ITEMLISTSIZE 128
#define MAXDROPCOUNT 1024

struct character_attachment {
	struct au_pos target_position;
	struct ap_event_manager_event * event;
};

struct as_event_gacha_process_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_event_gacha_module * ap_event_gacha;
	struct ap_item_module * ap_item;
	struct ap_optimized_packet2_module * ap_optimized_packet2;
	struct as_map_module * as_map;
	struct as_player_module * as_player;
	size_t character_attachment_offset;
	uint32_t item_pool[ITEMLISTSIZE];
	uint32_t item_pool_count;
	struct ap_item_template * gacha_drops[AP_EVENT_GACHA_MAX_TYPE][AP_EVENT_GACHA_MAX_RANK][MAXDROPCOUNT];
	uint32_t gacha_drop_count[AP_EVENT_GACHA_MAX_TYPE][AP_EVENT_GACHA_MAX_RANK];
};

static struct character_attachment * getcharattachment(
	struct as_event_gacha_process_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->character_attachment_offset);
}

static struct ap_event_manager_event * findevent(
	struct as_event_gacha_process_module * mod,
	void * source)
{
	return ap_event_manager_get_function(mod->ap_event_manager, source, 
		AP_EVENT_MANAGER_FUNCTION_GACHA);
}

static boolean cbcharstopaction(
	struct as_event_gacha_process_module * mod,
	struct ap_character_cb_stop_action * cb)
{
	struct character_attachment * attachment = getcharattachment(mod, cb->character);
	attachment->event = NULL;
	return TRUE;
}

static void setpool(
	struct as_event_gacha_process_module * mod,
	struct ap_event_gacha_event * e,
	struct ap_character * character)
{
	uint32_t i;
	uint32_t type = e->type->id - 1;
	uint32_t level = ap_character_get_absolute_level(character);
	mod->item_pool_count = 0;
	for (i = 0; i < AP_EVENT_GACHA_MAX_RANK; i++) {
		uint32_t j;
		for (j = 0; j < mod->gacha_drop_count[type][i]; j++) {
			struct ap_item_template * temp = mod->gacha_drops[type][i][j];
			struct ap_event_gacha_item_template_attachment * attachment = 
				ap_event_gacha_get_item_template_attachment(temp);
			if (ap_item_check_equip_class_restriction(mod->ap_item, temp, character) &&
				level >= attachment->gacha_level_min &&
				level <= attachment->gacha_level_max) {
				mod->item_pool[mod->item_pool_count++] = temp->tid;
				if (mod->item_pool_count == ITEMLISTSIZE)
					break;
			}
		}
		if (mod->item_pool_count == ITEMLISTSIZE)
			break;
	}
}

static boolean cbcharprocess(
	struct as_event_gacha_process_module * mod,
	struct ap_character_cb_process * cb)
{
	struct character_attachment * attachment = getcharattachment(mod, cb->character);
	struct ap_event_manager_event * e = attachment->event;
	float distance;
	if (!e)
		return TRUE;
	distance = au_distance2d(&attachment->target_position, &cb->character->pos);
	if (distance > AP_EVENT_GACHA_MAX_USE_RANGE)
		return TRUE;
	ap_character_stop_movement(mod->ap_character, cb->character);
	ap_character_stop_action(mod->ap_character, cb->character);
	setpool(mod, ap_event_gacha_get_event(e), cb->character);
	ap_event_gacha_make_grant_packet(mod->ap_event_gacha, 
		e, cb->character->id, ap_character_get_absolute_level(cb->character),
		mod->item_pool, mod->item_pool_count);
	as_player_send_packet(mod->as_player, cb->character);
	return TRUE;
}

static boolean cbreceive(
	struct as_event_gacha_process_module * mod,
	struct ap_event_gacha_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	switch (cb->type) {
	case AP_EVENT_GACHA_PACKET_REQUEST: {
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
		if (distance > AP_EVENT_GACHA_MAX_USE_RANGE) {
			vec2 dir = { position->x - c->pos.x, position->z - c->pos.z };
			float len = distance - 0.80f * AP_EVENT_GACHA_MAX_USE_RANGE;
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

static boolean cbitemendreadimport(
	struct as_event_gacha_process_module * mod,
	struct ap_item_cb_end_read_import * cb)
{
	struct ap_item_template * temp = cb->temp;
	if (temp->gacha_type && temp->factor.item.gacha) {
		uint32_t type = temp->gacha_type - 1;
		uint32_t rank = temp->factor.item.gacha - 1;
		uint32_t index;
		assert(type < AP_EVENT_GACHA_MAX_TYPE);
		assert(rank < AP_EVENT_GACHA_MAX_RANK);
		assert(mod->gacha_drop_count[type][rank] < MAXDROPCOUNT);
		index = mod->gacha_drop_count[type][rank]++;
		mod->gacha_drops[type][rank][index] = temp;
	}
	return TRUE;
}

static boolean onregister(
	struct as_event_gacha_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_gacha, AP_EVENT_GACHA_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_optimized_packet2, AP_OPTIMIZED_PACKET2_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	mod->character_attachment_offset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct character_attachment), mod, NULL, NULL);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_STOP_ACTION, mod, cbcharstopaction);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_PROCESS, mod, cbcharprocess);
	ap_item_add_callback(mod->ap_item, AP_ITEM_CB_END_READ_IMPORT, mod, cbitemendreadimport);
	ap_event_gacha_add_callback(mod->ap_event_gacha, AP_EVENT_GACHA_CB_RECEIVE, mod, cbreceive);
	return TRUE;
}

static void onshutdown(struct as_event_gacha_process_module * mod)
{
}

struct as_event_gacha_process_module * as_event_gacha_process_create_module()
{
	struct as_event_gacha_process_module * mod = ap_module_instance_new(AS_EVENT_GACHA_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	return mod;
}
