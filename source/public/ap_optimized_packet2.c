#include "public/ap_optimized_packet2.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_character.h"
#include "public/ap_event_manager.h"
#include "public/ap_define.h"
#include "public/ap_item.h"
#include "public/ap_item_convert.h"
#include "public/ap_module.h"
#include "public/ap_packet.h"
#include "public/ap_skill.h"

#include "utility/au_packet.h"

#include <stdlib.h>

#define MAXPACKETSIZE 32768

/* Temporary definition, move to guild module later. */
#define AGPMGUILD_MAX_GUILD_ID_LENGTH 32

struct ap_optimized_packet2_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_factors_module * ap_factors;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct ap_packet_module * ap_packet;
	struct ap_skill_module * ap_skill;
	struct au_packet packet;
	struct au_packet packet_char_view;
	struct au_packet packet_char_move;
	struct au_packet packet_char_action;
	struct au_packet packet_item_view;
};

static boolean additemtobuffer(
	struct ap_optimized_packet2_module * mod,
	const struct ap_item * item,
	void * buffer,
	uint16_t * length)
{
	void * buf = (void *)((uintptr_t)buffer + *length);
	uint16_t len = 0;
	ap_item_make_add_packet_buffer(mod->ap_item, item, buf, &len);
	if ((uint32_t)*length + len > MAXPACKETSIZE)
		return FALSE;
	*length += len;
	return TRUE;
}

static boolean additemconverttobuffer(
	struct ap_optimized_packet2_module * mod,
	struct ap_item * item,
	void * buffer,
	uint16_t * length)
{
	void * buf = (void *)((uintptr_t)buffer + *length);
	uint16_t len = 0;
	ap_item_convert_make_add_packet_buffer(mod->ap_item_convert, item, buf, &len);
	if ((uint32_t)*length + len > MAXPACKETSIZE)
		return FALSE;
	*length += len;
	return TRUE;
}

static boolean addskilltobuffer(
	struct ap_optimized_packet2_module * mod,
	struct ap_skill * skill,
	struct ap_character * character,
	void * buffer,
	uint16_t * length)
{
	void * buf = (void *)((uintptr_t)buffer + *length);
	uint16_t len = 0;
	ap_skill_make_add_packet_buffer(mod->ap_skill, skill, character, buf, &len);
	if ((uint32_t)*length + len > MAXPACKETSIZE)
		return FALSE;
	*length += len;
	return TRUE;
}

static void makeevent(
	struct ap_optimized_packet2_module * mod,
	struct ap_character * c, 
	void * buffer,
	uint16_t * length)
{
	struct ap_event_manager_attachment * a = 
		ap_event_manager_get_attachment(mod->ap_event_manager, c);
	uint32_t i;
	uint16_t total = 0;
	for (i = 0; i < a->event_count; i++) {
		struct ap_event_manager_event * e = &a->events[i];
		enum ap_event_manager_function fn = e->function;
		void * buf;
		uint16_t len = 0;
		switch (e->function) {
		case AP_EVENT_MANAGER_FUNCTION_TELEPORT:
		case AP_EVENT_MANAGER_FUNCTION_NPCTRADE:
		case AP_EVENT_MANAGER_FUNCTION_BANK:
		case AP_EVENT_MANAGER_FUNCTION_NPCDIALOG:
		case AP_EVENT_MANAGER_FUNCTION_ITEMCONVERT:
		case AP_EVENT_MANAGER_FUNCTION_GUILD:
		case AP_EVENT_MANAGER_FUNCTION_PRODUCT:
		case AP_EVENT_MANAGER_FUNCTION_SKILLMASTER:
		case AP_EVENT_MANAGER_FUNCTION_REFINERY:
		case AP_EVENT_MANAGER_FUNCTION_QUEST:
		case AP_EVENT_MANAGER_FUNCTION_AUCTION:
		case AP_EVENT_MANAGER_FUNCTION_CHAR_CUSTOMIZE:
		case AP_EVENT_MANAGER_FUNCTION_ITEM_REPAIR:
		case AP_EVENT_MANAGER_FUNCTION_REMISSION:
		case AP_EVENT_MANAGER_FUNCTION_WANTEDCRIMINAL:
		case AP_EVENT_MANAGER_FUNCTION_SIEGEWAR_NPC:
		case AP_EVENT_MANAGER_FUNCTION_TAX:
		case AP_EVENT_MANAGER_FUNCTION_GUILD_WAREHOUSE:
		case AP_EVENT_MANAGER_FUNCTION_ARCHLORD:
		case AP_EVENT_MANAGER_FUNCTION_GAMBLE:
		case AP_EVENT_MANAGER_FUNCTION_WORLD_CHAMPIONSHIP:
			buf = (void *)((uintptr_t)buffer + total);
			if (!ap_event_manager_make_event_data_packet(mod->ap_event_manager, e, 
					buf, &len) ||
				(uint32_t)total + len > MAXPACKETSIZE) {
				*length = 0;
				WARN("Event packet overflow (%s).", c->name);
				return;
			}
			total += len;
			break;
		}
	}
	*length = total;
}

static boolean onregister(
	struct ap_optimized_packet2_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_factors, AP_FACTORS_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item_convert, AP_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_skill, AP_SKILL_MODULE_NAME);
	return TRUE;
}

struct ap_optimized_packet2_module * ap_optimized_packet2_create_module()
{
	struct ap_optimized_packet2_module * mod = ap_module_instance_new(
		AP_OPTIMIZED_PACKET2_MODULE_NAME, sizeof(*mod), onregister, NULL, NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint16_t),
		AU_PACKET_TYPE_INT8, 1, /* operation */
		AU_PACKET_TYPE_PACKET, 1, /* character packet */
		AU_PACKET_TYPE_PACKET, AP_ITEM_PART_COUNT, /* item packet */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* buffed skill packet */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* event packet */
		/* Guild ID */
		AU_PACKET_TYPE_CHAR, AGPMGUILD_MAX_GUILD_ID_LENGTH + 1, 
		/* character all item packet */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1,
		/* character all item convert packet */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1,
		/* character all skill packet */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1,
		AU_PACKET_TYPE_INT32, 1, /* character id */
		AU_PACKET_TYPE_INT32, 1, /* GuildMarkTID */
		AU_PACKET_TYPE_INT32, 1, /* GuildMarkColor */
		AU_PACKET_TYPE_INT32, 1, /* IsWinner */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* title */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_char_view, sizeof(uint32_t),
		AU_PACKET_TYPE_INT32, 1, /* character id */
		AU_PACKET_TYPE_INT32, 1, /* character tid */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* character name */
		AU_PACKET_TYPE_INT8, 1, /* current action status */
		AU_PACKET_TYPE_INT8, 1, /* IsNewCharacter */
		AU_PACKET_TYPE_PACKET, 1, /* factor packet */
		AU_PACKET_TYPE_INT8, 1, /* move flag */
		AU_PACKET_TYPE_INT8, 1, /* move direction */
		AU_PACKET_TYPE_POS, 1, /* character current pos */
		AU_PACKET_TYPE_POS, 1, /* destination pos */
		AU_PACKET_TYPE_INT32, 1, /* follow target id */
		AU_PACKET_TYPE_UINT16, 1, /* follow distance */
		AU_PACKET_TYPE_INT16, 1, /* turn x */
		AU_PACKET_TYPE_INT16, 1, /* turn y */
		AU_PACKET_TYPE_UINT64, 1, /* character special status */
		AU_PACKET_TYPE_INT8, 1, /* is transform status, is ridable */
		AU_PACKET_TYPE_INT8, 1, /* face index */
		AU_PACKET_TYPE_INT8, 1, /* hair index */
		AU_PACKET_TYPE_INT8, 1, /* view options */
		AU_PACKET_TYPE_UINT16, 1, /* bit flag option */
		AU_PACKET_TYPE_INT8, 1, /* character criminal status							 */
		AU_PACKET_TYPE_INT32, 1, /* wanted criminal flag */
		/* nick name */
		AU_PACKET_TYPE_CHAR, AP_CHARACTER_MAX_NICKNAME_SIZE,
		AU_PACKET_TYPE_INT32, 1, /* npc display map */
		AU_PACKET_TYPE_INT32, 1, /* npc display minimap */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_char_move, sizeof(uint8_t),
		AU_PACKET_TYPE_INT32, 1, /* character id */
		AU_PACKET_TYPE_POS, 1, /* current position */
		AU_PACKET_TYPE_POS, 1, /* destination position */
		AU_PACKET_TYPE_INT32, 1, /* follow target */
		AU_PACKET_TYPE_UINT16, 1, /* follow distance */
		AU_PACKET_TYPE_INT8, 1, /* move flag */
		AU_PACKET_TYPE_INT8, 1, /* move direction */
		AU_PACKET_TYPE_INT8, 1, /* next action type */
		AU_PACKET_TYPE_INT32, 1, /* next action skill id */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_char_action, sizeof(uint16_t),
		AU_PACKET_TYPE_INT32, 1, /* action character id */
		AU_PACKET_TYPE_INT32, 1, /* target character id */
		AU_PACKET_TYPE_INT8, 1, /* attack result */
		AU_PACKET_TYPE_PACKET, 1, /* factor packet */
		AU_PACKET_TYPE_INT32, 1, /* target hp */
		AU_PACKET_TYPE_INT8, 1, /* combo info */
		AU_PACKET_TYPE_INT8, 1, /* force attack */
		AU_PACKET_TYPE_UINT32, 1, /* Additional Effect */
		AU_PACKET_TYPE_UINT8, 1, /* Hit Index */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_item_view, sizeof(uint32_t),
		AU_PACKET_TYPE_INT32, 1, /* item id */
		AU_PACKET_TYPE_INT32, 1, /* item template id */
		AU_PACKET_TYPE_INT16, 1, /* stack count */
		AU_PACKET_TYPE_INT8, 1, /* # of physical convert */
		AU_PACKET_TYPE_INT8, 1, /* # of socket */
		AU_PACKET_TYPE_INT8, 1, /* # of converted socket */
		AU_PACKET_TYPE_INT32, 1, /* convert tid 1 */
		AU_PACKET_TYPE_INT32, 1, /* convert tid 2 */
		AU_PACKET_TYPE_INT32, 1, /* convert tid 3 */
		AU_PACKET_TYPE_INT32, 1, /* convert tid 4 */
		AU_PACKET_TYPE_INT32, 1, /* convert tid 5 */
		AU_PACKET_TYPE_INT32, 1, /* convert tid 6 */
		AU_PACKET_TYPE_INT32, 1, /* convert tid 7 */
		AU_PACKET_TYPE_INT32, 1, /* convert tid 8 */
		AU_PACKET_TYPE_INT32, 1, /* item status flag */
		AU_PACKET_TYPE_UINT16, 1, /* option tid 1 */
		AU_PACKET_TYPE_UINT16, 1, /* option tid 2 */
		AU_PACKET_TYPE_UINT16, 1, /* option tid 3 */
		AU_PACKET_TYPE_UINT16, 1, /* option tid 4 */
		AU_PACKET_TYPE_UINT16, 1, /* option tid 5 */
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_optimized_packet2_add_callback(
	struct ap_optimized_packet2_module * mod,
	enum ap_optimized_packet2_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

boolean ap_optimized_packet2_on_receive_char_view(
	struct ap_optimized_packet2_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_optimized_packet2_cb_receive_char_view cb = { 0 };
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.packet_type,
			NULL, /* character packet */
			NULL, /* item packet */
			NULL, NULL, /* buffed skill packet */
			NULL, NULL, /* event packet */
			NULL, /* Guild ID */
			NULL, NULL, /* character all item packet */
			NULL, NULL, /* character all item convert packet */
			NULL, NULL, /* character all skill packet */
			&cb.character_id,
			NULL, /* GuildMarkTID */
			NULL, /* GuildMarkColor */
			NULL, /* IsWinner */
			NULL, NULL)) { /* title */
		return FALSE;
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, 
		AP_OPTIMIZED_PACKET2_CB_RECEIVE_CHAR_VIEW, &cb);
}

boolean ap_optimized_packet2_on_receive_char_move(
	struct ap_optimized_packet2_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_optimized_packet2_cb_receive_char_move cb = { 0 };
	if (!au_packet_get_field(&mod->packet_char_move, 
			TRUE, data, length,
			&cb.character_id,
			&cb.pos,
			&cb.dst_pos,
			NULL, /* follow target */
			NULL, /* follow distance */
			&cb.move_flags,
			&cb.move_direction,
			&cb.next_action_type,
			&cb.next_action_skill_id)) {
		return FALSE;
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, 
		AP_OPTIMIZED_PACKET2_CB_RECEIVE_CHAR_MOVE, &cb);
}

boolean ap_optimized_packet2_on_receive_char_action(
	struct ap_optimized_packet2_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_optimized_packet2_cb_receive_char_action cb = { 0 };
	if (!au_packet_get_field(&mod->packet_char_action, 
			TRUE, data, length,
			&cb.character_id, /* action character id */
			&cb.target_id, /* target character id */
			NULL, /* attack result */
			NULL, /* factor packet */
			NULL, /* target hp */
			NULL, /* combo info */
			&cb.force_attack, /* force attack */
			NULL, /* Additional Effect */
			NULL)) { /* Hit Index */
		return FALSE;
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, 
		AP_OPTIMIZED_PACKET2_CB_RECEIVE_CHAR_ACTION, &cb);
}

void ap_optimized_packet2_make_char_view_packet(
	struct ap_optimized_packet2_module * mod,
	struct ap_character * character)
{
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	ap_optimized_packet2_make_char_view_packet_buffer(mod, character,
		buffer, &length);
	ap_packet_set_length(mod->ap_packet, length);
}

static void makeitemviewpacket(
	struct ap_optimized_packet2_module * mod,
	void * buffer,
	struct ap_item * item)
{
	struct ap_item_convert_item * convert = 
		ap_item_convert_get_item(mod->ap_item_convert, item);
	switch (convert->convert_count) {
	case 0:
		au_packet_make_packet(
			&mod->packet_item_view, 
			buffer, FALSE, NULL, 0,
			&item->id, /* item id */
			&item->tid, /* item template id */
			&item->stack_count, /* stack count */
			&convert->physical_convert_level, /* # of physical convert */
			&convert->socket_count, /* # of socket */
			&convert->convert_count, /* # of converted socket */
			NULL, /* convert tid 1 */
			NULL, /* convert tid 2 */
			NULL, /* convert tid 3 */
			NULL, /* convert tid 4 */
			NULL, /* convert tid 5 */
			NULL, /* convert tid 6 */
			NULL, /* convert tid 7 */
			NULL, /* convert tid 8 */
			NULL, /* item status flag */
			NULL, /* option tid 1 */
			NULL, /* option tid 2 */
			NULL, /* option tid 3 */
			NULL, /* option tid 4 */
			NULL); /* option tid 5 */
		break;
	case 1:
		au_packet_make_packet(
			&mod->packet_item_view, 
			buffer, FALSE, NULL, 0,
			&item->id, /* item id */
			&item->tid, /* item template id */
			&item->stack_count, /* stack count */
			&convert->physical_convert_level, /* # of physical convert */
			&convert->socket_count, /* # of socket */
			&convert->convert_count, /* # of converted socket */
			&convert->socket_attr[0].tid, /* convert tid 1 */
			NULL, /* convert tid 2 */
			NULL, /* convert tid 3 */
			NULL, /* convert tid 4 */
			NULL, /* convert tid 5 */
			NULL, /* convert tid 6 */
			NULL, /* convert tid 7 */
			NULL, /* convert tid 8 */
			NULL, /* item status flag */
			NULL, /* option tid 1 */
			NULL, /* option tid 2 */
			NULL, /* option tid 3 */
			NULL, /* option tid 4 */
			NULL); /* option tid 5 */
		break;
	case 2:
		au_packet_make_packet(
			&mod->packet_item_view, 
			buffer, FALSE, NULL, 0,
			&item->id, /* item id */
			&item->tid, /* item template id */
			&item->stack_count, /* stack count */
			&convert->physical_convert_level, /* # of physical convert */
			&convert->socket_count, /* # of socket */
			&convert->convert_count, /* # of converted socket */
			&convert->socket_attr[0].tid, /* convert tid 1 */
			&convert->socket_attr[1].tid, /* convert tid 2 */
			NULL, /* convert tid 3 */
			NULL, /* convert tid 4 */
			NULL, /* convert tid 5 */
			NULL, /* convert tid 6 */
			NULL, /* convert tid 7 */
			NULL, /* convert tid 8 */
			NULL, /* item status flag */
			NULL, /* option tid 1 */
			NULL, /* option tid 2 */
			NULL, /* option tid 3 */
			NULL, /* option tid 4 */
			NULL); /* option tid 5 */
		break;
	case 3:
		au_packet_make_packet(
			&mod->packet_item_view, 
			buffer, FALSE, NULL, 0,
			&item->id, /* item id */
			&item->tid, /* item template id */
			&item->stack_count, /* stack count */
			&convert->physical_convert_level, /* # of physical convert */
			&convert->socket_count, /* # of socket */
			&convert->convert_count, /* # of converted socket */
			&convert->socket_attr[0].tid, /* convert tid 1 */
			&convert->socket_attr[1].tid, /* convert tid 2 */
			&convert->socket_attr[2].tid, /* convert tid 3 */
			NULL, /* convert tid 4 */
			NULL, /* convert tid 5 */
			NULL, /* convert tid 6 */
			NULL, /* convert tid 7 */
			NULL, /* convert tid 8 */
			NULL, /* item status flag */
			NULL, /* option tid 1 */
			NULL, /* option tid 2 */
			NULL, /* option tid 3 */
			NULL, /* option tid 4 */
			NULL); /* option tid 5 */
		break;
	case 4:
		au_packet_make_packet(
			&mod->packet_item_view, 
			buffer, FALSE, NULL, 0,
			&item->id, /* item id */
			&item->tid, /* item template id */
			&item->stack_count, /* stack count */
			&convert->physical_convert_level, /* # of physical convert */
			&convert->socket_count, /* # of socket */
			&convert->convert_count, /* # of converted socket */
			&convert->socket_attr[0].tid, /* convert tid 1 */
			&convert->socket_attr[1].tid, /* convert tid 2 */
			&convert->socket_attr[2].tid, /* convert tid 3 */
			&convert->socket_attr[3].tid, /* convert tid 4 */
			NULL, /* convert tid 5 */
			NULL, /* convert tid 6 */
			NULL, /* convert tid 7 */
			NULL, /* convert tid 8 */
			NULL, /* item status flag */
			NULL, /* option tid 1 */
			NULL, /* option tid 2 */
			NULL, /* option tid 3 */
			NULL, /* option tid 4 */
			NULL); /* option tid 5 */
		break;
	case 5:
		au_packet_make_packet(
			&mod->packet_item_view, 
			buffer, FALSE, NULL, 0,
			&item->id, /* item id */
			&item->tid, /* item template id */
			&item->stack_count, /* stack count */
			&convert->physical_convert_level, /* # of physical convert */
			&convert->socket_count, /* # of socket */
			&convert->convert_count, /* # of converted socket */
			&convert->socket_attr[0].tid, /* convert tid 1 */
			&convert->socket_attr[1].tid, /* convert tid 2 */
			&convert->socket_attr[2].tid, /* convert tid 3 */
			&convert->socket_attr[3].tid, /* convert tid 4 */
			&convert->socket_attr[4].tid, /* convert tid 5 */
			NULL, /* convert tid 6 */
			NULL, /* convert tid 7 */
			NULL, /* convert tid 8 */
			NULL, /* item status flag */
			NULL, /* option tid 1 */
			NULL, /* option tid 2 */
			NULL, /* option tid 3 */
			NULL, /* option tid 4 */
			NULL); /* option tid 5 */
		break;
	case 6:
		au_packet_make_packet(
			&mod->packet_item_view, 
			buffer, FALSE, NULL, 0,
			&item->id, /* item id */
			&item->tid, /* item template id */
			&item->stack_count, /* stack count */
			&convert->physical_convert_level, /* # of physical convert */
			&convert->socket_count, /* # of socket */
			&convert->convert_count, /* # of converted socket */
			&convert->socket_attr[0].tid, /* convert tid 1 */
			&convert->socket_attr[1].tid, /* convert tid 2 */
			&convert->socket_attr[2].tid, /* convert tid 3 */
			&convert->socket_attr[3].tid, /* convert tid 4 */
			&convert->socket_attr[4].tid, /* convert tid 5 */
			&convert->socket_attr[5].tid, /* convert tid 6 */
			NULL, /* convert tid 7 */
			NULL, /* convert tid 8 */
			NULL, /* item status flag */
			NULL, /* option tid 1 */
			NULL, /* option tid 2 */
			NULL, /* option tid 3 */
			NULL, /* option tid 4 */
			NULL); /* option tid 5 */
		break;
	case 7:
		au_packet_make_packet(
			&mod->packet_item_view, 
			buffer, FALSE, NULL, 0,
			&item->id, /* item id */
			&item->tid, /* item template id */
			&item->stack_count, /* stack count */
			&convert->physical_convert_level, /* # of physical convert */
			&convert->socket_count, /* # of socket */
			&convert->convert_count, /* # of converted socket */
			&convert->socket_attr[0].tid, /* convert tid 1 */
			&convert->socket_attr[1].tid, /* convert tid 2 */
			&convert->socket_attr[2].tid, /* convert tid 3 */
			&convert->socket_attr[3].tid, /* convert tid 4 */
			&convert->socket_attr[4].tid, /* convert tid 5 */
			&convert->socket_attr[5].tid, /* convert tid 6 */
			&convert->socket_attr[6].tid, /* convert tid 7 */
			NULL, /* convert tid 8 */
			NULL, /* item status flag */
			NULL, /* option tid 1 */
			NULL, /* option tid 2 */
			NULL, /* option tid 3 */
			NULL, /* option tid 4 */
			NULL); /* option tid 5 */
		break;
	case 8:
		au_packet_make_packet(
			&mod->packet_item_view, 
			buffer, FALSE, NULL, 0,
			&item->id, /* item id */
			&item->tid, /* item template id */
			&item->stack_count, /* stack count */
			&convert->physical_convert_level, /* # of physical convert */
			&convert->socket_count, /* # of socket */
			&convert->convert_count, /* # of converted socket */
			&convert->socket_attr[0].tid, /* convert tid 1 */
			&convert->socket_attr[1].tid, /* convert tid 2 */
			&convert->socket_attr[2].tid, /* convert tid 3 */
			&convert->socket_attr[3].tid, /* convert tid 4 */
			&convert->socket_attr[4].tid, /* convert tid 5 */
			&convert->socket_attr[5].tid, /* convert tid 6 */
			&convert->socket_attr[6].tid, /* convert tid 7 */
			&convert->socket_attr[7].tid, /* convert tid 8 */
			NULL, /* item status flag */
			NULL, /* option tid 1 */
			NULL, /* option tid 2 */
			NULL, /* option tid 3 */
			NULL, /* option tid 4 */
			NULL); /* option tid 5 */
		break;
	}
}

void ap_optimized_packet2_make_char_view_packet_buffer(
	struct ap_optimized_packet2_module * mod,
	struct ap_character * character,
	void * buffer,
	uint16_t * length)
{
	uint8_t type = AP_OPTIMIZED_PACKET2_ADD_CHARACTER_VIEW;
	void * movepacket = ap_packet_get_temp_buffer(mod->ap_packet);
	void * factorpacket = ap_packet_get_temp_buffer(mod->ap_packet);
	void * viewpacket =  ap_packet_get_temp_buffer(mod->ap_packet);
	void * eventpacket = ap_packet_get_temp_buffer(mod->ap_packet);
	uint16_t eventlen = 0;
	uint16_t namelen = (uint16_t)strlen(character->name);
	uint8_t moveflag = 0;
	uint8_t transformflags = 0;
	int16_t rotx = (int16_t)character->rotation_x;
	int16_t roty = (int16_t)character->rotation_y;
	static uint32_t buffedlist[AP_SKILL_MAX_SKILL_BUFF][4] = { 0 };
	uint32_t buffedlistcount = 0;
	uint32_t i;
	struct ap_skill_character * skillattachment = 
		ap_skill_get_character(mod->ap_skill, character);
	struct ap_item_character * itemattachment = 
		ap_item_get_character(mod->ap_item, character);
	struct ap_grid * grid = itemattachment->equipment;
	uint32_t equipcount = 0;
	void * equippacket[AP_ITEM_PART_COUNT] = { 0 };
	if (character->move_direction != AP_CHARACTER_MD_NONE)
		moveflag |= AP_CHARACTER_MOVE_FLAG_DIRECTION;
	if (character->is_path_finding)
		moveflag |= AP_CHARACTER_MOVE_FLAG_PATHFINDING;
	if (!character->is_moving)
		moveflag |= AP_CHARACTER_MOVE_FLAG_STOP;
	if (character->is_syncing)
		moveflag |= AP_CHARACTER_MOVE_FLAG_SYNC;
	if (character->is_moving_fast)
		moveflag |= AP_CHARACTER_MOVE_FLAG_FAST;
	if (character->is_following)
		moveflag |= AP_CHARACTER_MOVE_FLAG_FOLLOW;
	if (character->is_moving_horizontal)
		moveflag |= AP_CHARACTER_MOVE_FLAG_HORIZONTAL;
	au_packet_make_packet(
		&mod->packet_char_move, 
		movepacket, FALSE, NULL, 0,
		&character->id,
		&character->pos,
		character->is_moving ? &character->dst_pos : 
			&character->pos,
		NULL, /* Follow Target ID */
		NULL, /* Follow Distance */
		&moveflag,
		&character->move_direction,
		NULL, /* Next Action Type */
		NULL); /* Next Action Skill Id */
	ap_factors_make_char_view_packet(mod->ap_factors, factorpacket, 
		&character->factor);
	if (character->is_transformed)
		transformflags |= AP_CHARACTER_FLAG_TRANSFORM;
	if (character->is_ridable)
		transformflags |= AP_CHARACTER_FLAG_RIDABLE;
	if (character->is_evolved)
		transformflags |= AP_CHARACTER_FLAG_EVOLUTION;
	au_packet_make_packet(
		&mod->packet_char_view, 
		viewpacket, FALSE, NULL, 0,
		&character->id,
		&character->tid,
		character->name, &namelen,
		&character->action_status,
		NULL, /* Is New Character? */
		factorpacket,
		&moveflag,
		&character->move_direction,
		&character->pos,
		character->is_moving ? &character->dst_pos : NULL,
		NULL, /* Follow Target Id */
		NULL, /* Follow Distance */
		character->is_moving ? NULL : &rotx,
		character->is_moving ? NULL : &roty,
		&character->special_status,
		&transformflags,
		&character->face_index,
		&character->hair_index,
		NULL, /* Option View Helmet */
		&character->event_status_flags,
		&character->criminal_status,
		NULL, /* Is Wanted Criminal? */
		character->nickname,
		&character->npc_display_for_map,
		&character->npc_display_for_nameboard);
	for (i = 0; i < grid->grid_count && equipcount < AP_ITEM_PART_COUNT; i++) {
		struct ap_item * item = ap_grid_get_object_by_index(grid, i);
		if (!item || item->temp->equip.kind == AP_ITEM_EQUIP_KIND_RING)
			continue;
		equippacket[equipcount] = ap_packet_get_temp_buffer(mod->ap_packet);
		makeitemviewpacket(mod, equippacket[equipcount], item);
		equipcount++;;
	}
	for (i = 0; i < skillattachment->buff_count; i++) {
		struct ap_skill_buff_list * buff = &skillattachment->buff_list[i];
		if (!(buff->temp->attribute & AP_SKILL_ATTRIBUTE_PASSIVE)) {
			uint32_t index = buffedlistcount++;
			buffedlist[index][0] = buff->skill_tid;
			buffedlist[index][1] = buff->caster_tid;
			buffedlist[index][2] = 0;
			buffedlist[index][3] = 0;
		}
	}
	buffedlistcount *= 4 * sizeof(uint32_t);
	makeevent(mod, character, eventpacket, &eventlen);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, length,
		AP_OPTIMIZEDVIEW_PACKET_TYPE, 
		&type,
		viewpacket,
		equipcount ? equippacket : NULL, /* Item Packet */
		buffedlist, &buffedlistcount, /* Buffed Skill */
		eventpacket, &eventlen, /* NPC Events */
		NULL, /* Guild Name*/
		NULL, /* All Item */
		NULL, /* All Item Convert */
		NULL, /* All Skill */
		NULL, /* Character ID */
		NULL, /* Guild Mark TID */
		NULL, /* Guild Mark Color */
		NULL, /* Is Winner? */
		NULL); /* Title */
	ap_packet_pop_temp_buffers(mod->ap_packet, 4 + equipcount);
}

void ap_optimized_packet2_make_char_move_packet(
	struct ap_optimized_packet2_module * mod,
	struct ap_character * character,
	const struct au_pos * pos,
	const struct au_pos * dst_pos,
	uint8_t move_flags,
	uint8_t move_direction)
{
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	if (character->is_following) {
		move_flags |= AP_CHARACTER_MOVE_FLAG_FOLLOW;
		au_packet_make_packet(
			&mod->packet_char_move, 
			buffer, TRUE, &length, AP_OPTIMIZEDCHARMOVE_PACKET_TYPE,
			&character->id,
			pos,
			dst_pos,
			&character->follow_id, /* Follow Target ID */
			&character->follow_distance, /* Follow Distance */
			&move_flags,
			&move_direction,
			character->next_action ? 
				&character->next_action : NULL, /* Next Action Type */
			NULL); /* Next Skill Id */
	}
	else {
		au_packet_make_packet(
			&mod->packet_char_move, 
			buffer, TRUE, &length, AP_OPTIMIZEDCHARMOVE_PACKET_TYPE,
			&character->id,
			pos,
			dst_pos,
			NULL, /* Follow Target ID */
			NULL, /* Follow Distance */
			&move_flags,
			&move_direction,
			NULL, /* Next Action Type */
			NULL); /* Next Skill Id */
	}
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_optimized_packet2_make_char_all_item_skill(
	struct ap_optimized_packet2_module * mod,
	struct ap_character * character)
{
	uint8_t type = AP_OPTIMIZED_PACKET2_ADD_CHARACTER;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * itempacket = ap_packet_get_temp_buffer(mod->ap_packet);
	uint16_t itemlen = 0;
	void * convertpacket = ap_packet_get_temp_buffer(mod->ap_packet);
	uint16_t convertlen = 0;
	void * skillpacket = ap_packet_get_temp_buffer(mod->ap_packet);
	uint16_t skilllen = 0;
	uint32_t i = 0;
	struct ap_item_character * itemchar = 
		ap_item_get_character(mod->ap_item, character);
	struct ap_skill_character * skillchar = 
		ap_skill_get_character(mod->ap_skill, character);
	struct ap_grid * grids[] = {
		itemchar->inventory,
		itemchar->equipment,
		itemchar->cash_inventory,
		itemchar->sub_inventory,
		itemchar->bank };
	for (i = 0; i < COUNT_OF(grids); i++) {
		uint32_t j;
		for (j = 0; j < grids[i]->grid_count; j++) {
			struct ap_item * item = 
				ap_grid_get_object_by_index(grids[i], j);
			if (!item)
				continue;
			if (!additemtobuffer(mod, item, itempacket, &itemlen)) {
				WARN("Item packet buffer has overflowed.");
			}
			if (item->temp->type != AP_ITEM_TYPE_EQUIP)
				continue;
			if (!additemconverttobuffer(mod, item, convertpacket, 
					&convertlen)) {
				WARN("Item convert packet buffer has overflowed.");
			}
		}
	}
	for (i = 0; i < skillchar->skill_count; i++) {
		struct ap_skill * skill = skillchar->skill[i];
		if (!addskilltobuffer(mod, skill, character, 
				skillpacket, &skilllen)) {
			WARN("Skill packet buffer has overflowed.");
			break;
		}
	}
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_OPTIMIZEDVIEW_PACKET_TYPE, 
		&type,
		NULL, /* Character View Packet */
		NULL, /* Item Packet */
		NULL, /* Buffed Skill */
		NULL, /* NPC Events */
		NULL, /* Guild Name*/
		itempacket, &itemlen, /* All Item */
		convertpacket, &convertlen, /* All Item Convert */
		skillpacket, &skilllen, /* All Skill */
		NULL, /* Character ID */
		NULL, /* Guild Mark TID */
		NULL, /* Guild Mark Color */
		NULL, /* Is Winner? */
		NULL); /* Title */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}

void ap_optimized_packet2_make_char_action_packet(
	struct ap_optimized_packet2_module * mod,
	uint32_t character_id,
	uint32_t target_id,
	const struct ap_character_action_info * info)
{
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * factorpacket = NULL;
	boolean combo = TRUE;
	boolean force = TRUE;
	if (info->broadcast_factor) {
		factorpacket = ap_packet_get_temp_buffer(mod->ap_packet);
		ap_factors_make_damage_packet(mod->ap_factors, factorpacket, &info->target);
	}
	au_packet_make_packet(
		&mod->packet_char_action, 
		buffer, TRUE, &length, AP_OPTIMIZEDCHARACTION_PACKET_TYPE,
		&character_id, /* action character id */
		&target_id, /* target character id */
		&info->result, /* attack result */
		factorpacket, /* factor packet */
		NULL, /* target hp */
		&combo, /* combo info */
		&force, /* force attack */
		info->additional_effect ? 
			&info->additional_effect : 
			NULL,
		NULL); /* Hit Index */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}
