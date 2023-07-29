#include "server/as_event_teleport_process.h"

#include "core/log.h"

#include "public/ap_event_teleport.h"
#include "public/ap_optimized_packet2.h"
#include "public/ap_random.h"

#include "server/as_map.h"
#include "server/as_player.h"

#define RADIANS(DEGREES) (((float)M_PI / 180.0f) * (DEGREES))
#define DEGREES(radians) (((RADIANS) * 180.0f) / (float)M_PI)

struct character_attachment {
	struct ap_event_teleport_point * target_point;
	struct au_pos target_position;
	struct ap_event_manager_event * event;
};

struct as_event_teleport_process_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_event_teleport_module * ap_event_teleport;
	struct ap_optimized_packet2_module * ap_optimized_packet2;
	struct ap_random_module * ap_random;
	struct as_character_module * as_character;
	struct as_map_module * as_map;
	struct as_player_module * as_player;
	size_t character_attachment_offset;
};

static struct character_attachment * getcharattachment(
	struct as_event_teleport_process_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->character_attachment_offset);
}

static struct ap_character * findnpc(
	struct as_event_teleport_process_module * mod,
	struct ap_event_manager_base_packet * packet)
{
	if (!packet || packet->source_type != AP_BASE_TYPE_CHARACTER)
		return NULL;
	return as_map_get_npc(mod->as_map, packet->source_id);
}

static struct ap_event_manager_event * findevent(
	struct as_event_teleport_process_module * mod,
	void * source)
{
	return ap_event_manager_get_function(mod->ap_event_manager, source, 
		AP_EVENT_MANAGER_FUNCTION_TELEPORT);
}

static boolean cbcharstopaction(
	struct as_event_teleport_process_module * mod,
	struct ap_character_cb_stop_action * cb)
{
	struct character_attachment * attachment = getcharattachment(mod, cb->character);
	attachment->target_point = NULL;
	attachment->event = NULL;
	return TRUE;
}

static boolean cbcharprocess(
	struct as_event_teleport_process_module * mod,
	struct ap_character_cb_process * cb)
{
	struct character_attachment * attachment = getcharattachment(mod, cb->character);
	float distance;
	if (!attachment->target_point)
		return TRUE;
	distance = au_distance2d(&attachment->target_position, &cb->character->pos);
	if (distance > AP_EVENT_TELEPORT_MAX_USE_RANGE)
		return TRUE;
	ap_event_teleport_make_packet(mod->ap_event_teleport, 
		AP_EVENT_TELEPORT_PACKET_REQUEST_TELEPORT_GRANTED, attachment->event,
		&cb->character->id);
	as_player_send_packet(mod->as_player, cb->character);
	ap_character_stop_movement(mod->ap_character, cb->character);
	ap_character_stop_action(mod->ap_character, cb->character);
	return TRUE;
}

static inline boolean caninteract(
	struct ap_character * character,
	struct ap_event_teleport_point * point)
{
	if (point->use_type) {
		switch (character->factor.char_type.race) {
		case AU_RACE_TYPE_HUMAN:
			if (!CHECK_BIT(point->use_type, AP_EVENT_TELEPORT_USE_HUMAN))
				return FALSE;
			break;
		case AU_RACE_TYPE_ORC:
			if (!CHECK_BIT(point->use_type, AP_EVENT_TELEPORT_USE_ORC))
				return FALSE;
			break;
		case AU_RACE_TYPE_MOONELF:
			if (!CHECK_BIT(point->use_type, AP_EVENT_TELEPORT_USE_MOONELF))
				return FALSE;
			break;
		case AU_RACE_TYPE_DRAGONSCION:
			if (!CHECK_BIT(point->use_type, AP_EVENT_TELEPORT_USE_DRAGONSCION))
				return FALSE;
			break;
		default:
			return FALSE;
		}
	}
	switch (point->special_type) {
	case AP_EVENT_TELEPORT_SPECIAL_NORMAL:
		break;
	case AP_EVENT_TELEPORT_SPECIAL_RETURN_ONLY:
		/* \todo */
		break;
	case AP_EVENT_TELEPORT_SPECIAL_SIEGEWAR:
		/* \todo */
		break;
	case AP_EVENT_TELEPORT_SPECIAL_CASTLE_TO_DUNGEON:
		/* \todo */
		break;
	case AP_EVENT_TELEPORT_SPECIAL_DUNGEON_TO_LANSPHERE:
		/* \todo */
		break;
	case AP_EVENT_TELEPORT_SPECIAL_LANSPHERE:
		/* \todo */
		break;
	}
	return TRUE;
}

static inline boolean canteleport(
	struct ap_character * character,
	struct ap_event_teleport_point * src,
	struct ap_event_teleport_point * dst)
{
	uint32_t i;
	boolean linked = FALSE;
	switch (src->special_type) {
	case AP_EVENT_TELEPORT_SPECIAL_SIEGEWAR:
		/* \todo */
		break;
	case AP_EVENT_TELEPORT_SPECIAL_DUNGEON_TO_LANSPHERE:
		/* \todo */
		break;
	}
	switch (dst->special_type) {
	case AP_EVENT_TELEPORT_SPECIAL_SIEGEWAR:
		/* \todo */
		break;
	}
	for (i = 0; i < src->target_group_count && !linked; i++) {
		uint32_t j;
		for (j = 0; j < dst->group_count; j++) {
			if (strcmp(dst->groups[j].name, src->target_groups[i].name) == 0) {
				linked = TRUE;
				break;
			}
		}
	}
	return linked;
}

static void rotatearound(
	struct au_pos * pos, 
	const struct au_pos * center, 
	float degrees)
{
	float s = sinf(RADIANS(degrees));
	float c = cosf(RADIANS(degrees));
	float x_ = c * (pos->x - center->x) - s * (pos->z - center->z) + center->x;
	float z_ = s * (pos->x - center->x) + c * (pos->z - center->z) + center->z;
	pos->x = x_;
	pos->z = z_;
}

static void randomteleport(
	struct as_event_teleport_process_module * mod,
	struct ap_character * character,
	const struct ap_event_teleport_point * point,
	const struct au_pos * position)
{
	uint32_t i;
	float initialdegree = ap_random_float(mod->ap_random, -4, 4) * 45.0f;
	struct au_pos initialpos = {
		position->x + ap_random_float(mod->ap_random, point->radius_min, point->radius_max),
		position->y,
		position->z };
	for (i = 0; i < 8; i++) {
		struct au_pos pos = initialpos;
		struct as_map_tile_info tile;
		rotatearound(&pos, position, initialdegree + i * 45.0f);
		tile = as_map_get_tile(mod->as_map, &pos);
		if (tile.geometry_block & AS_MAP_GB_GROUND)
			continue;
		ap_event_teleport_start(mod->ap_event_teleport, character, &pos);
		break;
	}
}

static boolean cbreceive(
	struct as_event_teleport_process_module * mod,
	struct ap_event_teleport_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	switch (cb->type) {
	case AP_EVENT_TELEPORT_PACKET_TELEPORT_POINT: {
		struct ap_event_manager_event * e;
		void * source = NULL;
		struct au_pos * position = NULL;
		struct ap_event_teleport_event * teleportevent;
		struct ap_event_teleport_point * target;
		switch (cb->event->source_type) {
		case AP_BASE_TYPE_OBJECT:
			source = as_map_find_object(mod->as_map, cb->event->source_id, &c->pos);
			if (!source)
				return TRUE;
			break;
		case AP_BASE_TYPE_CHARACTER:
			source = as_map_get_npc(mod->as_map, cb->event->source_id);
			if (!source)
				return TRUE;
			break;
		default:
			return TRUE;
		}
		e = findevent(mod, source);
		if (!e)
			break;
		teleportevent = ap_event_teleport_get_event(e);
		if (!teleportevent->point)
			break;
		if (!caninteract(c, teleportevent->point))
			break;
		target = ap_event_teleport_find_point_by_name(mod->ap_event_teleport,
			cb->target_point_name);
		if (!target || !caninteract(c, target))
			break;
		if (!canteleport(c, teleportevent->point, target))
			break;
		if (!target->target_base)
			break;
		switch (target->target.base.type) {
		case AP_BASE_TYPE_OBJECT: {
			struct ap_object * object = target->target_base;
			position = &object->position;
			break;
		}
		case AP_BASE_TYPE_CHARACTER: {
			struct ap_character * npc = target->target_base;
			position = &npc->pos;
			break;
		}
		default:
			return TRUE;
		}
		randomteleport(mod, c, target, position);
		break;
	}
	case AP_EVENT_TELEPORT_PACKET_REQUEST_TELEPORT: {
		struct ap_event_manager_event * e;
		float distance;
		struct character_attachment * attachment;
		void * source = NULL;
		struct au_pos * position = NULL;
		struct ap_event_teleport_event * teleportevent;
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
		teleportevent = ap_event_teleport_get_event(e);
		if (!teleportevent->point)
			break;
		if (!caninteract(c, teleportevent->point))
			break;
		ap_character_stop_action(mod->ap_character, c);
		distance = au_distance2d(position, &c->pos);
		if (distance > AP_EVENT_TELEPORT_MAX_USE_RANGE) {
			vec2 dir = { position->x - c->pos.x, position->z - c->pos.z };
			float len = distance - 0.80f * AP_EVENT_TELEPORT_MAX_USE_RANGE;
			struct au_pos dst = { 0 };
			glm_vec2_normalize(dir);
			dst.x = c->pos.x + dir[0] * len;
			dst.y = c->pos.y;
			dst.z = c->pos.z + dir[1] * len;
			ap_character_set_movement(mod->ap_character, c, &dst, TRUE);
		}
		attachment = getcharattachment(mod, c);
		attachment->target_point = teleportevent->point;
		attachment->target_position = *position;
		attachment->event = e;
		break;
	}
	case AP_EVENT_TELEPORT_PACKET_TELEPORT_LOADING:
		break;
	}
	return TRUE;
}

static boolean cbeventteleportprepare(
	struct as_event_teleport_process_module * mod,
	struct ap_event_teleport_cb_teleport_prepare * cb)
{
	if (ap_character_is_pc(cb->character)) {
		ap_event_teleport_make_start_packet(mod->ap_event_teleport, 
			cb->character->id, cb->target_position);
		as_player_send_packet(mod->as_player, cb->character);
	}
	return TRUE;
}

static boolean cbeventteleportstart(
	struct as_event_teleport_process_module * mod,
	struct ap_event_teleport_cb_teleport_start * cb)
{
	ap_character_make_update_packet(mod->ap_character, cb->character,
		AP_CHARACTER_BIT_POSITION);
	as_map_broadcast(mod->as_map, cb->character);
	ap_event_teleport_make_loading_packet(mod->ap_event_teleport, cb->character->id);
	as_player_send_packet(mod->as_player, cb->character);
	return TRUE;
	ap_optimized_packet2_make_char_move_packet(mod->ap_optimized_packet2,
		cb->character, &cb->character->pos, NULL, 
		AP_CHARACTER_MOVE_FLAG_STOP | AP_CHARACTER_MOVE_FLAG_SYNC,
		0);
	as_map_broadcast(mod->as_map, cb->character);
	return TRUE;
}

static boolean onregister(
	struct as_event_teleport_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_teleport, AP_EVENT_TELEPORT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_optimized_packet2, AP_OPTIMIZED_PACKET2_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_random, AP_RANDOM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	mod->character_attachment_offset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct character_attachment), mod, NULL, NULL);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_STOP_ACTION, mod, cbcharstopaction);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_PROCESS, mod, cbcharprocess);
	ap_event_teleport_add_callback(mod->ap_event_teleport, AP_EVENT_TELEPORT_CB_RECEIVE, mod, cbreceive);
	ap_event_teleport_add_callback(mod->ap_event_teleport, AP_EVENT_TELEPORT_CB_TELEPORT_PREPARE, mod, cbeventteleportprepare);
	ap_event_teleport_add_callback(mod->ap_event_teleport, AP_EVENT_TELEPORT_CB_TELEPORT_START, mod, cbeventteleportstart);
	return TRUE;
}

static void onshutdown(struct as_event_teleport_process_module * mod)
{
}

struct as_event_teleport_process_module * as_event_teleport_process_create_module()
{
	struct as_event_teleport_process_module * mod = ap_module_instance_new(AS_EVENT_TELEPORT_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	return mod;
}
