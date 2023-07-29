#include <assert.h>

#include "core/file_system.h"
#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_admin.h"
#include "public/ap_character.h"
#include "public/ap_config.h"
#include "public/ap_event_teleport.h"
#include "public/ap_map.h"
#include "public/ap_optimized_packet2.h"
#include "public/ap_packet.h"
#include "public/ap_tick.h"

#include "server/as_map.h"
#include "server/as_player.h"
#include "server/as_server.h"

#define SEGMENTCOUNT \
	(AP_SECTOR_WORLD_INDEX_WIDTH * AP_SECTOR_DEFAULT_DEPTH)

struct rect {
	vec2 a;
	vec2 b;
	vec2 c;
	vec2 d;
};

struct as_map_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_config_module * ap_config;
	struct ap_event_teleport_module * ap_event_teleport;
	struct ap_item_module * ap_item;
	struct ap_map_module * ap_map;
	struct ap_object_module * ap_object;
	struct ap_optimized_packet2_module * ap_optimized_packet2;
	struct ap_packet_module * ap_packet;
	struct ap_tick_module * ap_tick;
	struct as_player_module * as_player;
	struct as_server_module * as_server;
	size_t char_ad_offset;
	struct ap_admin character_admin;
	struct ap_admin npc_admin;
	struct ap_admin object_admin;
	struct as_map_sector * sectors;
	struct as_map_sector void_sector;
	struct ap_character ** character_list;
	struct ap_character ** tmp_character_list;
	struct as_map_item_drop ** item_drop_lists[2];
	struct as_map_region regions[AP_MAP_MAX_REGION_COUNT];
	struct as_map_sector ** sector_lists[2];
	struct as_map_item_drop * item_drops;
	struct as_map_item_drop * free_item_drops;
};

static inline struct as_map_sector * getsector(
	struct as_map_module * mod,
	uint32_t x, 
	uint32_t z)
{
	size_t size = ap_module_get_module_data_size(mod, AS_MAP_MDI_SECTOR);
	size_t stride = size * AP_SECTOR_WORLD_INDEX_HEIGHT;
	assert(mod->sectors != NULL);
	assert(x < AP_SECTOR_WORLD_INDEX_WIDTH);
	assert(z < AP_SECTOR_WORLD_INDEX_HEIGHT);
	return (struct as_map_sector *)((uintptr_t)mod->sectors +
		x * stride + z * size);
}

struct as_map_sector * getsectorbypos(
	struct as_map_module * mod,
	const struct au_pos * pos)
{
	uint32_t x = 0;
	uint32_t z = 0;
	if (!ap_scr_pos_to_index(&pos->x, &x, &z))
		return &mod->void_sector;
	return getsector(mod, x, z);
}

struct as_map_segment * getsegmentbypos(
	struct as_map_module * mod,
	const struct au_pos * pos)
{
	uint32_t x = 0;
	uint32_t z = 0;
	uint32_t sx;
	uint32_t sz;
	struct as_map_sector * s;
	if (!ap_scr_pos_to_index(&pos->x, &x, &z))
		return &mod->void_sector.segments[0][0];
	s = getsector(mod, x, z);
	sx = (uint32_t)((pos->x - s->begin.x) / AP_SECTOR_STEPSIZE);
	sz = (uint32_t)((pos->z - s->begin.z) / AP_SECTOR_STEPSIZE);
	if (sx >= AP_SECTOR_DEFAULT_DEPTH ||
		sz >= AP_SECTOR_DEFAULT_DEPTH) {
		return &mod->void_sector.segments[0][0];
	}
	return &s->segments[sx][sz];
}

static boolean findinlist(
	struct ap_character ** list, 
	uint32_t count,
	struct ap_character * c)
{
	uint32_t i;
	for (i = 0; i < count; i++) {
		if (list[i] == c)
			return TRUE;
	}
	return FALSE;
}

static boolean findinsectorlist(
	struct as_map_sector ** list, 
	uint32_t count,
	struct as_map_sector * s)
{
	uint32_t i;
	for (i = 0; i < count; i++) {
		if (list[i] == s)
			return TRUE;
	}
	return FALSE;
}

static void appendlist(
	struct as_map_module * mod, 
	struct as_map_sector * s,
	struct ap_character *** list)
{
	struct ap_character * c = s->characters;
	while (c) {
		vec_push_back((void **)list, &c);
		c = as_map_get_character_ad(mod, c)->in_sector.next;
	}
}

static void appendlistwithfilter(
	struct as_map_module * mod, 
	struct as_map_sector * s,
	uint32_t instance_id,
	struct ap_character *** list)
{
	struct ap_character * c = s->characters;
	while (c) {
		struct as_map_character * mc = as_map_get_character_ad(mod, c);
		if (mc->instance_id == instance_id)
			vec_push_back((void **)list, &c);
		c = as_map_get_character_ad(mod, c)->in_sector.next;
	}
}

static void getcharlist(
	struct as_map_module * mod,
	struct as_map_sector * sector, 
	struct ap_character *** list)
{
	int32_t i;
	vec_clear(*list);
	for (i = -1; i < 2; i++) {
		int32_t j;
		for (j = -1; j < 2; j++) {
			struct as_map_sector * s = getsector(mod, 
				sector->index_x + i, sector->index_z + j);
			appendlist(mod, s, list);
		}
	}
}

static void getcharlistwithfilter(
	struct as_map_module * mod,
	struct as_map_sector * sector, 
	uint32_t instance_id,
	struct ap_character *** list)
{
	int32_t i;
	vec_clear(*list);
	for (i = -1; i < 2; i++) {
		int32_t j;
		for (j = -1; j < 2; j++) {
			struct as_map_sector * s = getsector(mod, 
				sector->index_x + i, sector->index_z + j);
			appendlistwithfilter(mod, s, instance_id, list);
		}
	}
}

static void addlink(
	struct as_map_module * mod,
	struct as_map_sector * sector, 
	struct ap_character * character,
	struct as_map_character * cmap)
{
	cmap->sector = sector;
	if (sector->characters) {
		/* If there is already a character in the list,
		 * link it with the new character. */
		struct as_map_character * head = 
			as_map_get_character_ad(mod, sector->characters);
		cmap->in_sector.prev = NULL;
		cmap->in_sector.next = sector->characters;
		head->in_sector.prev = character;
	}
	else {
		cmap->in_sector.prev = NULL;
		cmap->in_sector.next = NULL;
	}
	sector->characters = character;
}

static void removelink(
	struct as_map_module * mod,
	struct ap_character * c,
	struct as_map_character * cm)
{
	if (cm->sector->characters == c) {
		/* Character is the head of the list, replace 
		 * the head of the list with the next character 
		 * in the list */
		cm->sector->characters = cm->in_sector.next;
	}
	if (cm->in_sector.next) {
		struct as_map_character * next = 
			as_map_get_character_ad(mod, cm->in_sector.next);
		next->in_sector.prev = cm->in_sector.prev;
	}
	if (cm->in_sector.prev) {
		struct as_map_character * prev = 
			as_map_get_character_ad(mod, cm->in_sector.prev);
		prev->in_sector.next = cm->in_sector.next;
	}
}

static void appenddroplist(
	struct as_map_sector * s,
	struct as_map_item_drop *** list)
{
	struct as_map_item_drop * drop = s->item_drops;
	while (drop) {
		vec_push_back((void **)list, &drop);
		drop = drop->in_sector.next;
	}
}

static void getdroplist(
	struct as_map_module * mod,
	struct as_map_sector * sector, 
	struct as_map_item_drop *** list)
{
	int32_t i;
	vec_clear(*list);
	for (i = -1; i < 2; i++) {
		int32_t j;
		for (j = -1; j < 2; j++) {
			struct as_map_sector * s = getsector(mod, 
				sector->index_x + i, sector->index_z + j);
			appenddroplist(s, list);
		}
	}
}

static boolean finddropinlist(
	struct as_map_item_drop ** list, 
	uint32_t count,
	struct as_map_item_drop * drop)
{
	uint32_t i;
	for (i = 0; i < count; i++) {
		if (list[i] == drop)
			return TRUE;
	}
	return FALSE;
}

static void adddropsectorlink(
	struct as_map_sector * sector, 
	struct as_map_item_drop * drop)
{
	if (sector->item_drops) {
		/* If there is already an item drop in the list,
		 * link it with the new item drop. */
		struct as_map_item_drop * head = sector->item_drops;
		drop->in_sector.prev = NULL;
		drop->in_sector.next = head;
		head->in_sector.prev = drop;
	}
	else {
		drop->in_sector.prev = NULL;
		drop->in_sector.next = NULL;
	}
	drop->bound_sector = sector;
	sector->item_drops = drop;
}

static void removedropsectorlink(
	struct as_map_sector * sector, 
	struct as_map_item_drop * drop)
{
	if (sector->item_drops == drop) {
		/* Item drop is the head of the list, replace 
		 * the head of the list with the next item drop 
		 * in the list */
		sector->item_drops = drop->in_sector.next;
	}
	if (drop->in_sector.next) {
		struct as_map_item_drop * next = drop->in_sector.next;
		next->in_sector.prev = drop->in_sector.prev;
	}
	if (drop->in_sector.prev) {
		struct as_map_item_drop * prev = drop->in_sector.prev;
		prev->in_sector.next = drop->in_sector.next;
	}
	drop->bound_sector = NULL;
}

static void adddropworldlink(
	struct as_map_module * mod,
	struct as_map_item_drop * drop)
{
	if (mod->item_drops) {
		/* If there is already an item drop in the list,
		 * link it with the new item drop. */
		struct as_map_item_drop * head = mod->item_drops;
		drop->in_world.prev = NULL;
		drop->in_world.next = head;
		head->in_world.prev = drop;
	}
	else {
		drop->in_world.prev = NULL;
		drop->in_world.next = NULL;
	}
	mod->item_drops = drop;
}

static void removedropworldlink(
	struct as_map_module * mod,
	struct as_map_item_drop * drop)
{
	if (mod->item_drops == drop) {
		/* Item drop is the head of the list, replace 
		 * the head of the list with the next item drop 
		 * in the list */
		mod->item_drops = drop->in_world.next;
	}
	if (drop->in_world.next) {
		struct as_map_item_drop * next = drop->in_world.next;
		next->in_world.prev = drop->in_world.prev;
	}
	if (drop->in_world.prev) {
		struct as_map_item_drop * prev = drop->in_world.prev;
		prev->in_world.next = drop->in_world.next;
	}
}

static void addfreedropworldlink(
	struct as_map_module * mod,
	struct as_map_item_drop * drop)
{
	if (mod->free_item_drops) {
		/* If there is already an item drop in the list,
		 * link it with the new item drop. */
		struct as_map_item_drop * head = mod->free_item_drops;
		drop->in_world.prev = NULL;
		drop->in_world.next = head;
		head->in_world.prev = drop;
	}
	else {
		drop->in_world.prev = NULL;
		drop->in_world.next = NULL;
	}
	mod->free_item_drops = drop;
}

static void removefreedropworldlink(
	struct as_map_module * mod,
	struct as_map_item_drop * drop)
{
	if (mod->free_item_drops == drop) {
		/* Item drop is the head of the list, replace 
		 * the head of the list with the next item drop 
		 * in the list */
		mod->free_item_drops = drop->in_world.next;
	}
	if (drop->in_world.next) {
		struct as_map_item_drop * next = drop->in_world.next;
		next->in_world.prev = drop->in_world.prev;
	}
	if (drop->in_world.prev) {
		struct as_map_item_drop * prev = drop->in_world.prev;
		prev->in_world.next = drop->in_world.next;
	}
}

static struct as_map_item_drop * allocitemdrop(struct as_map_module * mod)
{
	struct as_map_item_drop * drop = mod->free_item_drops;
	if (drop) {
		removefreedropworldlink(mod, drop);
	}
	else {
		drop = alloc(sizeof(*drop));
		memset(drop, 0, sizeof(*drop));
	}
	return drop;
}

static void freeitemdrop(struct as_map_module * mod, struct as_map_item_drop * drop)
{
	addfreedropworldlink(mod, drop);
}

static boolean processobject(
	struct as_map_module * mod,
	struct as_map_sector * s,
	const char * file_name,
	struct ap_object * obj)
{
	struct ap_object ** entry = ap_admin_add_object_by_id(&mod->object_admin,
		obj->object_id);
	if (!entry) {
		WARN("Object id is not unique (%s:%u).", file_name, obj->object_id);
	}
	else {
		*entry = obj;
	}
	return TRUE;
}

static boolean eachobject(
	char * current_dir,
	size_t maxcount,
	const char * name,
	size_t size,
	struct as_map_module * mod)
{
	uint32_t division = 0;
	uint32_t sx = 0;
	uint32_t sz = 0;
	uint32_t x;
	uint32_t count = 0;
	if (sscanf(name, "obj%05d.ini", &division) != 1)
		return TRUE;
	if (!ap_scr_from_division_index(division, &sx, &sz))
		return TRUE;
	for (x = 0; x < 16; x++) {
		uint32_t z;
		for (z = 0; z < 16; z++) {
			struct as_map_sector * s = 
				getsector(mod, sx + x, sz + z);
			uint32_t c = vec_count(s->objects);
			uint32_t i;
			if (c != 0) {
				ERROR("Objects are already loaded for this sector (%s%s).", 
					current_dir, name);
				return FALSE;
			}
			if (!ap_object_load_sector(mod->ap_object, current_dir, 
					&s->objects, sx + x, sz + z)) {
				ERROR("Failed to load objects from (%s%s).", 
					current_dir, name);
				return FALSE;
			}
			c = vec_count(s->objects);
			for (i = 0; i < c; i++) {
				struct ap_object * obj = s->objects[i];
				if (!processobject(mod, s, name, obj)) {
					ERROR("Failed to process loaded object (%s%s:%u).",
						current_dir, name, obj->object_id);
					return FALSE;
				}
			}
			count += c;
		}
	}
	INFO("Loaded %5u objects (%s).", count, name);
	return TRUE;
}

static void onchangesector(
	struct as_map_module * mod,
	struct ap_character * c,
	struct as_map_sector * prev,
	struct as_map_sector * cur)
{
	const uint32_t view = 1;
	const uint32_t hide = 2;
	struct as_map_character * mc = as_map_get_character_ad(mod, c);
	uint32_t prevcount;
	uint32_t count;
	uint32_t i;
	struct as_server_conn * conn = NULL;
	/* We do not use a unified 'getcharlist' function in order 
	 * to reduce the redundant 'if' statements. */
	if (prev) {
		if (mc->sync_instance_id) {
			getcharlistwithfilter(mod, prev, mc->sync_instance_id,
				&mod->character_list);
		}
		else {
			getcharlist(mod, prev, &mod->character_list);
		}
		getdroplist(mod, prev, &mod->item_drop_lists[0]);
	}
	else {
		vec_clear(mod->character_list);
		vec_clear(mod->item_drop_lists[0]);
	}
	if (cur) {
		if (mc->instance_id) {
			getcharlistwithfilter(mod, cur, mc->instance_id,
				&mod->tmp_character_list);
		}
		else {
			getcharlist(mod, cur, &mod->tmp_character_list);
		}
		getdroplist(mod, cur, &mod->item_drop_lists[1]);
	}
	else {
		vec_clear(mod->tmp_character_list);
		vec_clear(mod->item_drop_lists[1]);
	}
	prevcount = vec_count(mod->character_list);
	count = vec_count(mod->tmp_character_list);
	if (c->char_type & AP_CHARACTER_TYPE_PC)
		conn = as_player_get_character_ad(mod->as_player, c)->conn;
	ap_packet_set_active_buffer(mod->ap_packet, view);
	ap_optimized_packet2_make_char_view_packet(mod->ap_optimized_packet2, c);
	ap_packet_set_active_buffer(mod->ap_packet, hide);
	ap_character_make_packet(mod->ap_character,
		AP_CHARACTER_PACKET_TYPE_REMOVE_FOR_VIEW, c->id);
	for (i = 0; i < prevcount; i++) {
		struct ap_character * other = mod->character_list[i];
		struct as_server_conn * otherconn = NULL;
		if (findinlist(mod->tmp_character_list, count, other))
			continue;
		if (c == other)
			continue;
		if (other->char_type & AP_CHARACTER_TYPE_PC)
			otherconn = as_player_get_character_ad(mod->as_player, other)->conn;
		if (otherconn) {
			ap_packet_set_active_buffer(mod->ap_packet, hide);
			as_server_send_packet(mod->as_server, otherconn);
		}
		if (conn) {
			ap_packet_set_active_buffer(mod->ap_packet, 0);
			ap_character_make_packet(mod->ap_character,
				AP_CHARACTER_PACKET_TYPE_REMOVE_FOR_VIEW, 
				other->id);
			as_server_send_packet(mod->as_server, conn);
		}
	}
	for (i = 0; i < count; i++) {
		struct ap_character * other = mod->tmp_character_list[i];
		struct as_server_conn * otherconn = NULL;
		if (findinlist(mod->character_list, prevcount, other))
			continue;
		if (c == other)
			continue;
		if (other->char_type & AP_CHARACTER_TYPE_PC)
			otherconn = as_player_get_character_ad(mod->as_player, other)->conn;
		/* If character is not invisible, inform 
		 * nearby players. */
		if (otherconn &&
			!(c->special_status & AP_CHARACTER_SPECIAL_STATUS_TRANSPARENT)) {
			ap_packet_set_active_buffer(mod->ap_packet, view);
			as_server_send_packet(mod->as_server, otherconn);
		}
		/* If nearby character is not invisible, inform 
		 * (if it is a player) the player. */
		if (conn && !(other->special_status & AP_CHARACTER_SPECIAL_STATUS_TRANSPARENT)) {
			ap_packet_set_active_buffer(mod->ap_packet, 0);
			ap_optimized_packet2_make_char_view_packet(mod->ap_optimized_packet2, other);
			as_server_send_packet(mod->as_server, conn);
		}
	}
	ap_packet_set_active_buffer(mod->ap_packet, 0);
	/* Following callbacks are only meaningful for player 
	 * characters and are relatively expensive so do not 
	 * trigger these callbacks for other character types 
	 * unless it becomes necessary. */
	if (!ap_character_is_pc(c))
		return;
	if (conn) {
		/* Handle item drop visibility. */
		prevcount = vec_count(mod->item_drop_lists[0]);
		count = vec_count(mod->item_drop_lists[1]);
		for (i = 0; i < prevcount; i++) {
			struct as_map_item_drop * other = mod->item_drop_lists[0][i];
			if (!finddropinlist(mod->item_drop_lists[1], count, other)) {
				ap_item_make_remove_packet(mod->ap_item, other->item);
				as_server_send_packet(mod->as_server, conn);
			}
		}
		for (i = 0; i < count; i++) {
			struct as_map_item_drop * other = mod->item_drop_lists[1][i];
			if (!finddropinlist(mod->item_drop_lists[0], prevcount, other)) {
				ap_item_make_add_packet(mod->ap_item, other->item);
				as_server_send_packet(mod->as_server, conn);
			}
		}
	}
	vec_clear(mod->sector_lists[0]);
	vec_clear(mod->sector_lists[1]);
	if (prev) {
		struct as_map_cb_leave_sector cb = { c, prev };
		int32_t x;
		ap_module_enum_callback(mod, 
			AS_MAP_CB_LEAVE_SECTOR, &cb);
		/* Query sectors that were previously visible. */
		for (x = -1; x < 2; x++) {
			int32_t z;
			for (z = -1; z < 2; z++) {
				struct as_map_sector * sector = getsector(mod, 
					prev->index_x + x, prev->index_z + z);
				vec_push_back((void **)&mod->sector_lists[0], 
					&sector);
			}
		}
	}
	if (cur) {
		struct as_map_cb_enter_sector cb = { c, cur };
		int32_t x;
		ap_module_enum_callback(mod, 
			AS_MAP_CB_ENTER_SECTOR, &cb);
		/* Query sectors that have become visible. */
		for (x = -1; x < 2; x++) {
			int32_t z;
			for (z = -1; z < 2; z++) {
				struct as_map_sector * sector = getsector(mod, 
					cur->index_x + x, cur->index_z + z);
				vec_push_back((void **)&mod->sector_lists[1], 
					&sector);
			}
		}
	}
	prevcount = vec_count(mod->sector_lists[0]);
	count  = vec_count(mod->sector_lists[1]);
	for (i = 0; i < prevcount; i++) {
		struct as_map_sector * sector = mod->sector_lists[0][i];
		if (!findinsectorlist(mod->sector_lists[1], 
				count, sector)) {
			struct as_map_cb_leave_sector_view cb = { c, sector };
			ap_module_enum_callback(mod, 
				AS_MAP_CB_LEAVE_SECTOR_VIEW, &cb);
		}
	}
	for (i = 0; i < count; i++) {
		struct as_map_sector * sector = mod->sector_lists[1][i];
		if (!findinsectorlist(mod->sector_lists[0], 
				prevcount, sector)) {
			struct as_map_cb_enter_sector_view cb = { c, sector };
			ap_module_enum_callback(mod, 
				AS_MAP_CB_ENTER_SECTOR_VIEW, &cb);
		}
	}
}

static void onchangeregion(
	struct as_map_module * mod,
	struct ap_character * c,
	struct as_map_region * prev,
	struct as_map_region * next)
{
	uint32_t i;
	uint32_t count;
	if (!ap_character_is_pc(c))
		return;
	/* TODO: If region is a safe area and there is a valid 
	 * binding, update character binding. */
	if (prev) {
		count = vec_count(prev->npcs);
		for (i = 0; i < count; i++) {
			struct ap_character * npc = prev->npcs[i];
			struct as_map_character * m = as_map_get_character_ad(mod, npc);
			as_player_send_custom_packet(mod->as_player, c, m->npc_remove_packet, 
				m->npc_remove_packet_len);
		}
	}
	if (next) {
		count = vec_count(next->npcs);
		for (i = 0; i < count; i++) {
			struct ap_character * npc = next->npcs[i];
			struct as_map_character * m = as_map_get_character_ad(mod, npc);
			as_player_send_custom_packet(mod->as_player, c, m->npc_view_packet, 
				m->npc_view_packet_len);
		}
		if (next->temp->type.props.safety_type == AP_MAP_ST_SAFE)
			c->bound_region_id = next->temp->id;
	}
}

static boolean chardtor(
	struct as_map_module * mod, 
	struct ap_character * character)
{
	struct as_map_character * cmap = 
		as_map_get_character_ad(mod, character);
	/* It is expected for characters to be removed from 
	 * map before being freed. */
	assert(cmap->sector == NULL);
	dealloc(cmap->npc_view_packet);
	cmap->npc_view_packet = NULL;
	dealloc(cmap->npc_remove_packet);
	cmap->npc_remove_packet = NULL;
	return TRUE;
}

static boolean cbcharmove(
	struct as_map_module * mod, 
	struct ap_character_cb_move * cb)
{
	struct ap_character * character = cb->character;
	struct as_map_sector * sector;
	struct as_map_sector * prev;
	struct as_map_character * cmap;
	struct as_map_region * pr;
	struct as_map_region * r;
	sector = getsectorbypos(mod, &character->pos);
	cmap = as_map_get_character_ad(mod, character);
	prev = cmap->sector;
	if (prev != sector) {
		removelink(mod, character, cmap);
		addlink(mod, sector, character, cmap);
		onchangesector(mod, character, prev, sector);
	}
	pr = cmap->region;
	r = as_map_get_region_at(mod, &cb->character->pos);
	if (pr != r) {
		cmap->region = r;
		onchangeregion(mod, character, pr, r);
	}
	return TRUE;
}

static boolean cbcharinitstatic(
	struct as_map_module * mod,
	struct ap_character *  c)
{
	struct as_map_region * r = as_map_get_region_at(mod, &c->pos);
	struct as_map_character * cmap;
	struct ap_character ** obj;
	if (!r) {
		ERROR("No region at static character position (%s).",
			c->name);
		return FALSE;
	}
	vec_push_back((void **)&r->npcs, &c);
	obj = ap_admin_add_object_by_id(&mod->npc_admin, c->id);
	if (!obj)
		return FALSE;
	*obj = c;
	cmap = as_map_get_character_ad(mod, c);
	cmap->region = r;
	cmap->npc_view_packet = alloc(8096);
	cmap->npc_remove_packet = alloc(128);
	ap_optimized_packet2_make_char_view_packet_buffer(mod->ap_optimized_packet2, c,
		cmap->npc_view_packet, &cmap->npc_view_packet_len);
	ap_character_make_packet_buffer(mod->ap_character,
		AP_CHARACTER_PACKET_TYPE_REMOVE_FOR_VIEW, c->id,
		cmap->npc_remove_packet, &cmap->npc_remove_packet_len);
	return TRUE;
}

static boolean cbcharupdatefactor(
	struct as_map_module * mod,
	struct ap_character_cb_update_factor * cb)
{
	struct as_map_character * mc = 
		as_map_get_character_ad(mod, cb->character);
	if (!mc->sector) {
		/* Character is not yet added to map.
		 * It is not necessary to broadcast stat changes. */
		return TRUE;
	}
	ap_character_make_update_packet(mod->ap_character, cb->character, 
		cb->update_flags);
	as_map_broadcast(mod, cb->character);
	return TRUE;
}

static boolean cbcharupdate(
	struct as_map_module * mod,
	struct ap_character_cb_update * cb)
{
	if (cb->is_private) {
		if (ap_character_is_pc(cb->character)) {
			ap_character_make_update_packet(mod->ap_character, cb->character, 
				cb->update_flags);
			as_player_send_packet(mod->as_player, cb->character);
		}
	}
	else {
		struct as_map_character * mc = 
			as_map_get_character_ad(mod, cb->character);
		if (!mc->sector) {
			/* Character is not yet added to map.
			 * It is not necessary to broadcast stat changes. */
			return TRUE;
		}
		ap_character_make_update_packet(mod->ap_character, cb->character, 
			cb->update_flags);
		as_map_broadcast(mod, cb->character);
	}
	return TRUE;
}

static boolean cbmapregioninit(
	struct as_map_module * mod,
	struct ap_map_region_template * temp)
{
	struct as_map_region * r = &mod->regions[temp->id];
	if (r->initialized) {
		ERROR("More than one region with same id (%u).", temp->id);
		return FALSE;
	}
	r->initialized = TRUE;
	r->temp = temp;
	r->npcs = vec_new(sizeof(*r->npcs));
	return TRUE;
}

static boolean onregister(
	struct as_map_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_teleport, AP_EVENT_TELEPORT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_map, AP_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_object, AP_OBJECT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_optimized_packet2, AP_OPTIMIZED_PACKET2_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_server, AS_SERVER_MODULE_NAME);
	mod->char_ad_offset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct as_map_character),
		mod, NULL, chardtor);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_MOVE, mod, cbcharmove);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_INIT_STATIC, mod, cbcharinitstatic);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_UPDATE_FACTOR, mod, cbcharupdatefactor);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_UPDATE, mod, cbcharupdate);
	ap_map_add_callback(mod->ap_map, AP_MAP_CB_INIT_REGION, mod, cbmapregioninit);
	return TRUE;
}

boolean oninitialize(struct as_map_module * mod)
{
	size_t size = ap_module_get_module_data_size(mod, AS_MAP_MDI_SECTOR) * 
		AP_SECTOR_WORLD_INDEX_WIDTH * AP_SECTOR_WORLD_INDEX_HEIGHT;
	uint32_t x;
	mod->sectors = alloc(size);
	if (!mod->sectors) {
		ERROR("Failed to allocate memory for map sectors.");
		return FALSE;
	}
	for (x = 0; x < AP_SECTOR_WORLD_INDEX_WIDTH; x++) {
		uint32_t z;
		for (z = 0; z < AP_SECTOR_WORLD_INDEX_HEIGHT; z++) {
			struct as_map_sector * s = getsector(mod, x, z);
			ap_module_construct_module_data(mod,
				AS_MAP_MDI_SECTOR, s);
			s->index_x = x;
			s->index_z = z;
			s->begin = (struct au_pos){ 
				AP_SECTOR_WORLD_START_X + x * AP_SECTOR_WIDTH,
				0.0f,
				AP_SECTOR_WORLD_START_Z + z * AP_SECTOR_WIDTH };
			s->end = (struct au_pos){ 
				AP_SECTOR_WORLD_START_X + (x + 1) * AP_SECTOR_WIDTH,
				0.0f,
				AP_SECTOR_WORLD_START_Z + (z + 1) * AP_SECTOR_WIDTH };
			s->objects = vec_new(sizeof(*s->objects));
		}
	}
	return TRUE;
}

static void onclose(struct as_map_module * mod)
{
	uint32_t x;
	uint32_t i;
	struct as_map_item_drop * itemdrop;
	for (x = 0; x < AP_SECTOR_WORLD_INDEX_WIDTH; x++) {
		uint32_t z;
		for (z = 0; z < AP_SECTOR_WORLD_INDEX_WIDTH; z++) {
			struct as_map_sector * s = getsector(mod, x, z);
			uint32_t i;
			uint32_t count = vec_count(s->objects);
			for (i = 0; i < count; i++)
				ap_object_destroy(mod->ap_object, s->objects[i]);
			vec_clear(s->objects);
			while (s->item_drops) {
				itemdrop = s->item_drops;
				ap_item_free(mod->ap_item, itemdrop->item);
				removedropsectorlink(s, itemdrop);
				removedropworldlink(mod, itemdrop);
				addfreedropworldlink(mod, itemdrop);
			}
			ap_module_destruct_module_data(mod,
				AS_MAP_MDI_SECTOR, s);
		}
	}
	itemdrop = mod->free_item_drops;
	while (itemdrop) {
		struct as_map_item_drop * next = itemdrop->in_world.next;
		dealloc(itemdrop);
		itemdrop = next;
	}
	mod->free_item_drops = NULL;
	for (i = 0; i < AP_MAP_MAX_REGION_COUNT; i++) {
		struct as_map_region * r = &mod->regions[i];
		uint32_t count;
		uint32_t j;
		if (!r->initialized)
			continue;
		count = vec_count(r->npcs);
		for (j = 0; j < count; j++) {
			struct ap_character * c = r->npcs[j];
			ap_admin_remove_object_by_id(&mod->npc_admin, c->id);
			ap_character_free(mod->ap_character, c);
		}
	}
}

static void onshutdown(struct as_map_module * mod)
{
	uint32_t x;
	size_t i = 0;
	struct ap_character ** obj = NULL;
	if (!mod)
		return;
	for (x = 0; x < AP_SECTOR_WORLD_INDEX_WIDTH; x++) {
		uint32_t z;
		for (z = 0; z < AP_SECTOR_WORLD_INDEX_WIDTH; z++) {
			struct as_map_sector * s = getsector(mod, x, z);
			vec_free(s->objects);
		}
	}
	/* Character lifetimes are controlled by other modules 
	 * and it is expected for all characters to have 
	 * been removed by this point. */
	while (ap_admin_iterate_id(&mod->character_admin,
			&i, (void **)&obj)) {
		struct ap_character * c = *obj;
		ERROR("Character was not removed from map at shutdown (id = %u, tid = %u, name = %s).",
			c->id, c->tid, c->name);
	}
	ap_admin_destroy(&mod->character_admin);
	i = 0;
	while (ap_admin_iterate_id(&mod->npc_admin,
			&i, (void **)&obj)) {
		struct ap_character * c = *obj;
		ERROR("NPC was not removed from map at shutdown (id = %u, name = %s).",
			c->id, c->name);
	}
	ap_admin_destroy(&mod->npc_admin);
	ap_admin_destroy(&mod->object_admin);
	vec_free(mod->character_list);
	vec_free(mod->tmp_character_list);
	vec_free(mod->item_drop_lists[0]);
	vec_free(mod->item_drop_lists[1]);
	vec_free(mod->sector_lists[0]);
	vec_free(mod->sector_lists[1]);
}

struct as_map_module * as_map_create_module()
{
	struct as_map_module * mod = ap_module_instance_new(AS_MAP_MODULE_NAME,
		sizeof(*mod), onregister, oninitialize, onclose, onshutdown);
	uint32_t x;
	ap_module_set_module_data(mod, AS_MAP_MDI_SECTOR, 
		sizeof(struct as_map_sector), NULL, NULL);
	ap_admin_init(&mod->character_admin, sizeof(struct ap_character *), 128);
	ap_admin_init(&mod->npc_admin, sizeof(struct ap_character *), 512);
	ap_admin_init(&mod->object_admin, sizeof(struct ap_object *), 2048);
	mod->character_list = vec_new_reserved(
		sizeof(*mod->character_list), 1024);
	mod->tmp_character_list = vec_new_reserved(
		sizeof(*mod->character_list), 1024);
	mod->item_drop_lists[0] = vec_new_reserved(
		sizeof(*mod->item_drop_lists), 128);
	mod->item_drop_lists[1] = vec_new_reserved(
		sizeof(*mod->item_drop_lists), 128);
	mod->sector_lists[0] = vec_new_reserved(
		sizeof(*mod->sector_lists[0]), 16);
	mod->sector_lists[1] = vec_new_reserved(
		sizeof(*mod->sector_lists[1]), 16);
	/* Set dummy sector segments as blocking. */
	for (x = 0; x < AP_SECTOR_DEFAULT_DEPTH; x++) {
		uint32_t z;
		for (z = 0; z < AP_SECTOR_DEFAULT_DEPTH; z++) {
			mod->void_sector.segments[x][z].tile.geometry_block = 
				AS_MAP_GB_GROUND | AS_MAP_GB_SKY;
		}
	}
	return mod;
}

boolean as_map_read_segments(struct as_map_module * mod)
{
	const char * file_path = ap_config_get(mod->ap_config, "ServerNodePath");
	size_t size = 0;
	void * data;
	uint32_t * cursor;
	assert(sizeof(struct as_map_segment) == 4);
	if (!get_file_size(file_path, &size) || !size) {
		ERROR("Failed to retrieve file size (%zu).", size);
		return FALSE;
	}
	data = alloc(size);
	if (!load_file(file_path, data, size)) {
		ERROR("Failed to load file (%zu).", size);
		dealloc(data);
		return FALSE;
	}
	cursor = data;
	while (size >= 1032) {
		uint32_t x = *cursor++;
		uint32_t z = *cursor++;
		assert(x < AP_SECTOR_WORLD_INDEX_WIDTH);
		assert(z < AP_SECTOR_WORLD_INDEX_HEIGHT);
		struct as_map_sector * sector = getsector(mod, x, z);
		uint32_t sz;
		for (sz = 0; sz < 16; sz++) {
			uint32_t sx;
			for (sx = 0; sx < 16; sx++) {
				memcpy(&sector->segments[sx][sz], cursor, 4);
				cursor++;
			}
		}
		size -= 1032;
	}
	dealloc(data);
	return TRUE;
}

boolean as_map_load_objects(struct as_map_module * mod)
{
	char path[512];
	const char * inidir = ap_config_get(mod->ap_config, "ServerIniDir");
	if (!inidir) {
		ERROR("Failed to retrieve ServerIniDir configuration.");
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/objects/", inidir)) {
		ERROR("Failed to create path (%s/objects/).", inidir);
		return FALSE;
	}
	if (!enum_dir(path, sizeof(path), FALSE, eachobject, mod)) {
		ERROR("Failed to enumerate objects directory.");
		return FALSE;
	}
	return TRUE;
}

void as_map_add_callback(
	struct as_map_module * mod,
	enum as_map_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t as_map_attach_data(
	struct as_map_module * mod,
	enum as_map_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

struct as_map_character * as_map_get_character_ad(
	struct as_map_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->char_ad_offset);
}

boolean as_map_add_character(
	struct as_map_module * mod,
	struct ap_character * character)
{
	struct as_map_sector * sector;
	struct ap_character ** obj = ap_admin_add_object_by_id(
		&mod->character_admin, character->id);
	struct as_map_character * cmap;
	if (!obj)
		return FALSE;
	sector = getsectorbypos(mod, &character->pos);
	cmap = as_map_get_character_ad(mod, character);
	addlink(mod, sector, character, cmap);
	cmap->region = as_map_get_region_at(mod, &character->pos);
	*obj = character;
	onchangesector(mod, character, NULL, sector);
	onchangeregion(mod, character, NULL, cmap->region);
	return TRUE;
}

struct ap_character * as_map_get_character(
	struct as_map_module * mod,
	uint32_t character_id)
{
	struct ap_character ** obj = ap_admin_get_object_by_id(
		&mod->character_admin, character_id);
	if (!obj)
		return NULL;
	return *obj;
}

boolean as_map_remove_character(
	struct as_map_module * mod,
	struct ap_character * character)
{
	struct as_map_character * cmap = as_map_get_character_ad(mod, character);
	assert(cmap->sector != NULL);
	ap_character_make_packet(mod->ap_character,
		AP_CHARACTER_PACKET_TYPE_REMOVE_FOR_VIEW, character->id);
	as_map_broadcast(mod, character);
	removelink(mod, character, cmap);
	cmap->sector = NULL;
	cmap->in_sector.prev = NULL;
	cmap->in_sector.next = NULL;
	cmap->region = NULL;
	return ap_admin_remove_object_by_id(&mod->character_admin, 
		character->id);
}

struct ap_character * as_map_iterate_characters(
	struct as_map_module * mod,
	size_t * index)
{
	struct ap_character ** c = NULL;
	if (!ap_admin_iterate_id(&mod->character_admin, index, (void **)&c))
		return NULL;
	return *c;
}

struct ap_character * as_map_get_npc(
	struct as_map_module * mod,
	uint32_t character_id)
{
	struct ap_character ** obj = ap_admin_get_object_by_id(
		&mod->npc_admin, character_id);
	if (!obj)
		return NULL;
	return *obj;
}

void as_map_get_characters(
	struct as_map_module * mod,
	const struct au_pos * pos, 
	struct ap_character *** list)
{
	struct as_map_sector * sector = getsectorbypos(mod, pos);
	if (sector->index_x < 1 || sector->index_z < 1) {
		vec_clear(*list);
		return;
	}
	getcharlist(mod, sector, list);
}

void as_map_get_characters_in_instance(
	struct as_map_module * mod,
	const struct au_pos * pos, 
	uint32_t instance_id,
	struct ap_character *** list)
{
	struct as_map_sector * sector = getsectorbypos(mod, pos);
	if (sector->index_x < 1 || sector->index_z < 1) {
		vec_clear(*list);
		return;
	}
	getcharlistwithfilter(mod, sector, instance_id, list);
}

void as_map_get_characters_in_radius(
	struct as_map_module * mod,
	struct ap_character * character,
	const struct au_pos * pos, 
	float radius,
	struct ap_character *** list)
{
	struct as_map_character * mc = as_map_get_character_ad(mod, character);
	uint32_t i;
	uint32_t count;
	if (mc->instance_id) {
		as_map_get_characters_in_instance(mod, pos, mc->instance_id,
			&mod->tmp_character_list);
	}
	else {
		as_map_get_characters(mod, pos, &mod->tmp_character_list);
	}
	vec_clear(*list);
	count = vec_count(mod->tmp_character_list);
	for (i = 0; i < count; i++) {
		struct ap_character * nearby = mod->tmp_character_list[i];
		if (au_distance2d(pos, &nearby->pos) <= radius)
			vec_push_back((void **)list, &nearby);
	}
}

static void setrect(
	struct rect * rect, 
	const struct au_pos * src, 
	const struct au_pos * dst, 
	float width, 
	float length)
{
	vec2 vec = { dst->x - src->x, dst->y - src->y };
	vec2 per = { vec[1], -1 * vec[0] };
	float l = sqrtf(per[0] * per[0] + per[1] * per[1]);
	vec2 per_norm = { per[0] / l, per[1] / l };
	vec2 norm = { vec[0] / l, vec[1] / l };
	vec2 end	= { src->x + (length * norm[0]), src->y + (length * norm[1]) };
	float dx = per_norm[0] * width / 2;
	float dy = per_norm[1] * width / 2;
	rect->a[0] = src->x + dx;
	rect->a[1] = src->y + dy;
	rect->b[0] = src->x - dx;
	rect->b[1] = src->y - dy;
	rect->c[0] = end[0] - dx;
	rect->c[1] = end[1] - dy;
	rect->d[0] = end[0] + dx;
	rect->d[1] = end[1] + dy;
}

static inline float dot(const vec2 v1, const vec2 v2)
{
	return (v1[0] * v2[0] + v1[1] * v2[1]);
}

static boolean intersects(
	const vec2 p1, 
	const vec2 p2, 
	const struct au_pos * center, 
	float r) {

	vec2 line = { p2[0] - p1[0], p2[1] - p1[1] };
	float length =  sqrtf(line[0] * line[0] + line[1] * line[1]);
	line[0] /= length;
	line[1] /= length;
	vec2 to_start = { p1[0] - center->x, p1[1] - center->z };
	float dist = dot(to_start, line);
	if (dist < 0) 
		dist = 0;
	else if (dist > length) 
		dist = length;
	vec2 closest = { p1[0] + line[0] * dist, p1[1] + line[1] * dist };
	vec2 to_point = { center->x - closest[0], center->z - closest[1] };
	dist = dot(to_point, to_point);
	return (dist <= r * r);
}

static boolean isinside(const struct rect * rect, const struct au_pos * point)
{
	vec2 v1	= { rect->d[0] - rect->a[0], rect->d[1] - rect->a[1] };
	vec2 v2	= { rect->b[0] - rect->a[0], rect->b[1] - rect->a[1] };
	vec2 v	= { point->x - rect->a[0], point->z - rect->a[1] };
	float d1 = dot(v, v1);
	float d1_ = dot(v1, v1);
	float d2 = dot(v, v2);
	float d2_ = dot(v2, v2);
	return ((0 <= d1 && d1 <= d1_) && (0 <= d2 && d2 <= d2_));
}

static boolean isinrect(
	const struct rect * rect, 
	const struct au_pos * point, 
	float radius)
{
	return (isinside(rect, point) ||
		intersects(rect->a, rect->b, point, radius) ||
		intersects(rect->b, rect->c, point, radius) ||
		intersects(rect->c, rect->d, point, radius) ||
		intersects(rect->d, rect->a, point, radius));
}

void as_map_get_characters_in_line(
	struct as_map_module * mod,
	struct ap_character * character,
	const struct au_pos * begin, 
	const struct au_pos * end, 
	float width,
	float length,
	struct ap_character *** list)
{
	struct as_map_character * mc = as_map_get_character_ad(mod, character);
	uint32_t i;
	uint32_t count;
	struct rect hitbox;
	if (mc->instance_id) {
		as_map_get_characters_in_instance(mod, begin, mc->instance_id,
			&mod->tmp_character_list);
	}
	else {
		as_map_get_characters(mod, begin, &mod->tmp_character_list);
	}
	vec_clear(*list);
	count = vec_count(mod->tmp_character_list);
	setrect(&hitbox, begin, end, width, length);
	for (i = 0; i < count; i++) {
		struct ap_character * nearby = mod->tmp_character_list[i];
		if (isinrect(&hitbox, &nearby->pos, (float)character->factor.attack.hit_range))
			vec_push_back((void **)list, &nearby);
	}
}

void as_map_broadcast(struct as_map_module * mod, struct ap_character * character)
{
	uint32_t count = 0;
	uint32_t i;
	struct as_map_character * mc = as_map_get_character_ad(mod, character);
	if (mc->sync_instance_id) {
		as_map_get_characters_in_instance(mod, &character->pos,
			mc->sync_instance_id, &mod->character_list);
	}
	else {
		as_map_get_characters(mod, &character->pos,
			&mod->character_list);
	}
	count = vec_count(mod->character_list);
	for (i = 0; i < count; i++) {
		struct ap_character * c = mod->character_list[i];
		struct as_player_character * pc = 
			as_player_get_character_ad(mod->as_player, c);
		if (pc->conn)
			as_server_send_packet(mod->as_server, pc->conn);
	}
}

void as_map_broadcast_with_exception(
	struct as_map_module * mod,
	struct ap_character * character,
	struct ap_character * exception)
{
	uint32_t count = 0;
	uint32_t i;
	struct as_map_character * mc = as_map_get_character_ad(mod, character);
	if (mc->sync_instance_id) {
		as_map_get_characters_in_instance(mod, &character->pos,
			mc->sync_instance_id, &mod->character_list);
	}
	else {
		as_map_get_characters(mod, &character->pos,
			&mod->character_list);
	}
	count = vec_count(mod->character_list);
	for (i = 0; i < count; i++) {
		struct ap_character * c = mod->character_list[i];
		struct as_player_character * pc;
		if (c == exception)
			continue;
		pc = as_player_get_character_ad(mod->as_player, c);
		if (pc->conn)
			as_server_send_packet(mod->as_server, pc->conn);
	}
}

void as_map_broadcast_around(
	struct as_map_module * mod, 
	const struct au_pos * position)
{
	uint32_t count = 0;
	uint32_t i;
	as_map_get_characters(mod, position, &mod->character_list);
	count = vec_count(mod->character_list);
	for (i = 0; i < count; i++) {
		struct ap_character * c = mod->character_list[i];
		struct as_player_character * pc = 
			as_player_get_character_ad(mod->as_player, c);
		if (pc->conn)
			as_server_send_packet(mod->as_server, pc->conn);
	}
}

void as_map_inform_nearby(
	struct as_map_module * mod,
	struct ap_character * character)
{
	struct as_map_character * mc = as_map_get_character_ad(mod, character);
	uint32_t count;
	uint32_t i;
	struct as_server_conn * conn = NULL;
	assert(character->char_type & AP_CHARACTER_TYPE_PC);
	if (mc->sync_instance_id) {
		as_map_get_characters_in_instance(mod, &character->pos,
			mc->sync_instance_id, &mod->character_list);
	}
	else {
		as_map_get_characters(mod, &character->pos,
			&mod->character_list);
	}
	if (character->char_type & AP_CHARACTER_TYPE_PC)
		conn = as_player_get_character_ad(mod->as_player, character)->conn;
	count = vec_count(mod->character_list);
	for (i = 0; i < count; i++) {
		struct ap_character * other = mod->character_list[i];
		struct as_server_conn * otherconn = NULL;
		if (character == other)
			continue;
		if (other->char_type & AP_CHARACTER_TYPE_PC)
			otherconn = as_player_get_character_ad(mod->as_player, other)->conn;
		/* If nearby character is not invisible, inform 
		 * (if it is a player) the player. */
		if (conn && !(other->special_status & AP_CHARACTER_SPECIAL_STATUS_TRANSPARENT)) {
			ap_optimized_packet2_make_char_view_packet(mod->ap_optimized_packet2, 
				other);
			as_server_send_packet(mod->as_server, conn);
		}
	}
	if (mc->region) {
		count = vec_count(mc->region->npcs);
		for (i = 0; i < count; i++) {
			struct ap_character * npc = mc->region->npcs[i];
			struct as_map_character * m = as_map_get_character_ad(mod, npc);
			as_player_send_custom_packet(mod->as_player, character, 
				m->npc_view_packet, m->npc_view_packet_len);
		}
	}
}

struct as_map_tile_info as_map_get_tile(
	struct as_map_module * mod,
	const struct au_pos * pos)
{
	struct as_map_segment * s = getsegmentbypos(mod, pos);
	return s->tile;
}

struct as_map_region * as_map_get_region_at(
	struct as_map_module * mod,
	const struct au_pos * pos)
{
	struct as_map_segment * s = getsegmentbypos(mod, pos);
	if (s->region_id > AP_MAP_MAX_REGION_ID)
		return NULL;
	if (!mod->regions[s->region_id].initialized)
		return NULL;
	return &mod->regions[s->region_id];
}

boolean as_map_respawn_at_region(
	struct as_map_module * mod,
	struct ap_character * character,
	uint32_t region_id)
{
	return FALSE;
}

struct as_map_sector * as_map_get_sector(
	struct as_map_module * mod,
	uint32_t x, 
	uint32_t z)
{
	return getsector(mod, x, z);
}

struct as_map_sector * as_map_get_sector_at(
	struct as_map_module * mod,
	const struct au_pos * pos)
{
	return getsectorbypos(mod, pos);
}

boolean as_map_is_in_field_of_vision(
	struct as_map_module * mod,
	struct ap_character * character1,
	struct ap_character * character2)
{
	struct as_map_character * mc = as_map_get_character_ad(mod, character1);
	uint32_t i;
	uint32_t count;
	if (mc->sync_instance_id) {
		as_map_get_characters_in_instance(mod, &character1->pos, 
			mc->sync_instance_id, &mod->tmp_character_list);
	}
	else {
		as_map_get_characters(mod, &character1->pos, &mod->tmp_character_list);
	}
	count = vec_count(mod->tmp_character_list);
	for (i = 0; i < count; i++) {
		if (mod->tmp_character_list[i] == character2)
			return TRUE;
	}
	return FALSE;
}

void as_map_add_item_drop(
	struct as_map_module * mod, 
	struct ap_item * item,
	uint64_t expire_in_ms)
{
	struct as_map_item_drop * drop = allocitemdrop(mod);
	uint64_t tick = ap_tick_get(mod->ap_tick);
	struct as_map_sector * sector = as_map_get_sector_at(mod, &item->position);
	assert(sector != NULL);
	drop->item_id = item->id;
	drop->item = item;
	drop->expire_tick = tick + expire_in_ms;
	drop->ownership_expire_tick = tick + 30000;
	adddropsectorlink(sector, drop);
	adddropworldlink(mod, drop);
	ap_item_make_add_packet(mod->ap_item, item);
	as_map_broadcast_around(mod, &item->position);
}

struct as_map_item_drop * as_map_find_item_drop(
	struct as_map_module * mod, 
	const struct au_pos * position,
	uint32_t item_id)
{
	struct as_map_sector * sector = as_map_get_sector_at(mod, position);
	int32_t i;
	assert(sector != NULL);
	for (i = -1; i < 2; i++) {
		int32_t j;
		for (j = -1; j < 2; j++) {
			struct as_map_sector * s = getsector(mod, 
				sector->index_x + i, sector->index_z + j);
			struct as_map_item_drop * drop = s->item_drops;
			while (drop) {
				if (drop->item_id == item_id)
					return drop;
				drop = drop->in_sector.next;
			}
		}
	}
	return NULL;
}

void as_map_remove_item_drop(
	struct as_map_module * mod, 
	struct as_map_item_drop * item_drop)
{
	struct ap_item * item = item_drop->item;
	ap_item_make_remove_packet(mod->ap_item, item);
	as_map_broadcast_around(mod, &item->position);
	ap_item_free(mod->ap_item, item);
	removedropsectorlink(item_drop->bound_sector, item_drop);
	removedropworldlink(mod, item_drop);
	addfreedropworldlink(mod, item_drop);
}

void as_map_clear_expired_item_drops(struct as_map_module * mod, uint64_t tick)
{
	struct as_map_item_drop * drop = mod->item_drops;
	while (drop) {
		struct as_map_item_drop * next = drop->in_world.next;
		if (tick >= drop->expire_tick)
			as_map_remove_item_drop(mod, drop);
		drop = next;
	}
}

struct ap_object * as_map_find_object(
	struct as_map_module * mod, 
	uint32_t id,
	const struct au_pos * position)
{
	if (position) {
		struct as_map_sector * sector = getsectorbypos(mod, position);
		int32_t i;
		if (sector->index_x < 1 || sector->index_z < 1)
			return NULL;
		for (i = -1; i < 2; i++) {
			int32_t j;
			for (j = -1; j < 2; j++) {
				struct as_map_sector * s = getsector(mod, 
					sector->index_x + i, sector->index_z + j);
				uint32_t k;
				uint32_t count = vec_count(s->objects);
				for (k = 0; k < count; k++) {
					if (s->objects[k]->object_id == id)
						return s->objects[k];
				}
			}
		}
		return NULL;
	}
	else {
		struct ap_object ** entry = ap_admin_get_object_by_id(&mod->object_admin, id);
		if (!entry)
			return NULL;
		return *entry;
	}
}
