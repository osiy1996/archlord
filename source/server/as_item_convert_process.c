#include "server/as_item_convert_process.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/vector.h"

#include "public/ap_chat.h"
#include "public/ap_item.h"
#include "public/ap_item_convert.h"
#include "public/ap_module.h"
#include "public/ap_pvp.h"
#include "public/ap_random.h"
#include "public/ap_skill.h"
#include "public/ap_system_message.h"
#include "public/ap_tick.h"

#include "server/as_drop_item.h"
#include "server/as_event_binding.h"
#include "server/as_item.h"
#include "server/as_map.h"
#include "server/as_player.h"
#include "server/as_skill.h"

#include <assert.h>
#include <stdlib.h>

#define CHARACTER_RNG_MODULE_ID 2
#define MAKE_RNG_GUID(RNG_ID) \
	AS_CHARACTER_MAKE_RNG_GUID(CHARACTER_RNG_MODULE_ID, RNG_ID)

enum rng_group_id {
	RNG_GROUP_PHYSICAL_CONVERT = 1,
};

struct as_item_convert_process_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_chat_module * ap_chat;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct ap_random_module * ap_random;
	struct ap_pvp_module * ap_pvp;
	struct ap_skill_module * ap_skill;
	struct ap_system_message_module * ap_system_message;
	struct ap_tick_module * ap_tick;
	struct as_character_module * as_character;
	struct as_drop_item_module * as_drop_item;
	struct as_event_binding_module * as_event_binding;
	struct as_item_module * as_item;
	struct as_map_module * as_map;
	struct as_player_module * as_player;
	struct as_skill_module * as_skill;
	struct ap_character ** list;
};

static inline uint64_t makephysicalconvertguid(uint32_t rank)
{
	return MAKE_RNG_GUID(((uint64_t)RNG_GROUP_PHYSICAL_CONVERT << 32) | rank);
}

static void handleconvertrequest(
	struct as_item_convert_process_module * mod,
	struct ap_character * character,
	struct ap_item * item,
	struct ap_item_convert_item * attachment,
	struct ap_item * convert)
{
	switch (convert->temp->usable.usable_type) {
	case AP_ITEM_USABLE_TYPE_RUNE: {
		enum ap_item_convert_rune_result result = 
			ap_item_convert_is_suitable_for_rune_convert(mod->ap_item_convert,
				item, attachment, convert);
		if (result != AP_ITEM_CONVERT_RUNE_RESULT_SUCCESS) {
			ap_item_convert_make_response_rune_convert_packet(mod->ap_item_convert,
				item, result, convert->tid);
			as_player_send_packet(mod->as_player, character);
			break;
		}
		result = ap_item_convert_attempt_rune_convert(mod->ap_item_convert,
			item, attachment, convert);
		ap_item_convert_make_response_rune_convert_packet(mod->ap_item_convert,
			item, result, convert->tid);
		as_player_send_packet(mod->as_player, character);
		switch (result) {
		case AP_ITEM_CONVERT_RUNE_RESULT_FAILED_AND_DESTROY:
			as_item_delete(mod->as_item, character, item);
			break;
		default:
			ap_item_make_update_packet(mod->ap_item, item, AP_ITEM_UPDATE_FACTORS);
			as_player_send_packet(mod->as_player, character);
			break;
		}
		as_item_consume(mod->as_item, character, convert, 1);
		break;
	}
	case AP_ITEM_USABLE_TYPE_SPIRIT_STONE: {
		enum ap_item_convert_spirit_stone_result result = 
			ap_item_convert_is_suitable_for_spirit_stone(mod->ap_item_convert,
				item, attachment, convert);
		if (result != AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_SUCCESS) {
			ap_item_convert_make_response_spirit_stone_packet(mod->ap_item_convert,
				item, result, convert->tid);
			as_player_send_packet(mod->as_player, character);
			break;
		}
		result = ap_item_convert_attempt_spirit_stone(mod->ap_item_convert,
			item, attachment, convert);
		ap_item_convert_make_response_spirit_stone_packet(mod->ap_item_convert,
			item, result, convert->tid);
		as_player_send_packet(mod->as_player, character);
		if (result == AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_SUCCESS) {
			ap_item_make_update_packet(mod->ap_item, item, AP_ITEM_UPDATE_FACTORS);
			as_player_send_packet(mod->as_player, character);
		}
		as_item_consume(mod->as_item, character, convert, 1);
		break;
	}
	}
}

static boolean cbreceive(
	struct as_item_convert_process_module * mod,
	struct ap_item_convert_cb_receive * cb)
{
	struct ap_character * character = cb->user_data;
	struct ap_item * item = ap_item_find(mod->ap_item, character, cb->item_id,
		2, AP_ITEM_STATUS_INVENTORY, AP_ITEM_STATUS_SUB_INVENTORY);
	struct ap_item * convert = ap_item_find(mod->ap_item, character, cb->convert_id,
		3, AP_ITEM_STATUS_INVENTORY, AP_ITEM_STATUS_SUB_INVENTORY, AP_ITEM_STATUS_CASH_INVENTORY);
	struct ap_item_convert_item * attachment;
	if (!item || !convert || !convert->stack_count)
		return TRUE;
	if (item->temp->type != AP_ITEM_TYPE_EQUIP)
		return TRUE;
	if (convert->temp->type != AP_ITEM_TYPE_USABLE)
		return TRUE;
	attachment = ap_item_convert_get_item(mod->ap_item_convert, item);
	switch (cb->type) {
	case AP_ITEM_CONVERT_PACKET_TYPE_REQUEST_PHYSICAL_CONVERT: {
		struct as_character_db * characterdb;
		uint32_t rate;
		enum ap_item_convert_result result;
		if (convert->temp->usable.usable_type != AP_ITEM_USABLE_TYPE_CONVERT_CATALYST) {
			ap_item_convert_make_response_physical_convert_packet(mod->ap_item_convert,
				item, AP_ITEM_CONVERT_RESULT_INVALID_CATALYST);
			as_player_send_packet(mod->as_player, character);
			break;
		}
		if (!ap_item_convert_is_suitable_for_physical_convert(mod->ap_item_convert, 
				item, attachment)) {
			ap_item_convert_make_response_physical_convert_packet(mod->ap_item_convert,
				item, AP_ITEM_CONVERT_RESULT_INVALID_ITEM);
			as_player_send_packet(mod->as_player, character);
			break;
		}
		characterdb = as_character_get(mod->as_character, character)->db;
		if (!characterdb)
			break;
		as_item_consume(mod->as_item, character, convert, 1);
		rate = as_character_generate_random(characterdb, 
			makephysicalconvertguid(attachment->physical_convert_level + 1), 100);
		result = ap_item_convert_attempt_physical_convert(mod->ap_item_convert,
			item, attachment, rate);
		ap_item_convert_make_response_physical_convert_packet(mod->ap_item_convert,
			item, result);
		as_player_send_packet(mod->as_player, character);
		switch (result) {
		case AP_ITEM_CONVERT_RESULT_FAILED_AND_DESTROY:
			as_item_delete(mod->as_item, character, item);
			break;
		case AP_ITEM_CONVERT_RESULT_SUCCESS:
		case AP_ITEM_CONVERT_RESULT_FAILED_AND_INIT:
		case AP_ITEM_CONVERT_RESULT_FAILED_AND_INIT_SAME:
			ap_item_make_update_packet(mod->ap_item, item, AP_ITEM_UPDATE_FACTORS);
			as_player_send_packet(mod->as_player, character);
			break;
		}
		break;
	}
	case AP_ITEM_CONVERT_PACKET_TYPE_REQUEST_RUNE_CONVERT:
	case AP_ITEM_CONVERT_PACKET_TYPE_REQUEST_SPIRITSTONE_CONVERT:
		/* When spirit stones are used from pet inventory, client confuses 
		 * spirit stones for runes, so handle them together. */
		handleconvertrequest(mod, character, item, attachment, convert);
		break;
	case AP_ITEM_CONVERT_PACKET_TYPE_CHECK_CASH_RUNE_CONVERT: {
		enum ap_item_convert_rune_result result;
		if (!AP_ITEM_IS_CASH_ITEM(convert))
			break;
		if (convert->temp->usable.usable_type != AP_ITEM_USABLE_TYPE_RUNE)
			break;
		result = ap_item_convert_is_suitable_for_rune_convert(mod->ap_item_convert,
			item, ap_item_convert_get_item(mod->ap_item_convert, item), 
			convert);
		ap_item_convert_make_response_rune_check_result_packet(mod->ap_item_convert,
			character->id, item->id, convert->id, result);
		as_player_send_packet(mod->as_player, character);
		break;
	}
	}
	return TRUE;
}

static boolean onregister(
	struct as_item_convert_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_chat, AP_CHAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item_convert, AP_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_random, AP_RANDOM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_pvp, AP_PVP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_skill, AP_SKILL_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_system_message, AP_SYSTEM_MESSAGE_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_drop_item, AS_DROP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_event_binding, AS_EVENT_BINDING_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_item, AS_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_skill, AS_SKILL_MODULE_NAME);
	ap_item_convert_add_callback(mod->ap_item_convert, AP_ITEM_CONVERT_CB_RECEIVE, mod, cbreceive);
	return TRUE;
}

static void onshutdown(struct as_item_convert_process_module * mod)
{
	vec_free(mod->list);
}

struct as_item_convert_process_module * as_item_convert_process_create_module()
{
	struct as_item_convert_process_module * mod = ap_module_instance_new(AS_ITEM_CONVERT_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	mod->list = vec_new_reserved(sizeof(*mod->list), 128);
	return mod;
}
