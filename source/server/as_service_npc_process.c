#include "server/as_service_npc_process.h"

#include "core/log.h"

#include "public/ap_chat.h"
#include "public/ap_drop_item.h"
#include "public/ap_event_manager.h"
#include "public/ap_event_binding.h"
#include "public/ap_event_npc_dialog.h"
#include "public/ap_event_teleport.h"
#include "public/ap_item.h"
#include "public/ap_item_convert.h"
#include "public/ap_service_npc.h"
#include "public/ap_tick.h"

#include "server/as_drop_item.h"
#include "server/as_event_binding.h"
#include "server/as_map.h"
#include "server/as_player.h"
#include "server/as_service_npc.h"
#include "server/as_skill.h"

#include <assert.h>

#define CHAOTICFRONTIERNPCTID 1973
#define EVENTNPCTID 426

struct as_service_npc_process_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_chat_module * ap_chat;
	struct ap_drop_item_module * ap_drop_item;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_event_npc_dialog_module * ap_event_npc_dialog;
	struct ap_event_teleport_module * ap_event_teleport;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct ap_service_npc_module * ap_service_npc;
	struct ap_tick_module * ap_tick;
	struct as_drop_item_module * as_drop_item;
	struct as_event_binding_module * as_event_binding;
	struct as_map_module * as_map;
	struct as_player_module * as_player;
	struct as_service_npc_module * as_service_npc;
	struct as_skill_module * as_skill;
};

static boolean cbmenureturn(
	struct as_service_npc_process_module * mod,
	struct ap_event_npc_dialog_menu_callback * cb)
{
	const struct au_pos * destination;
	if (ap_tick_get(mod->ap_tick) < cb->character->combat_end_tick)
		return TRUE;
	destination = as_event_binding_find_by_type(mod->as_event_binding, 
		cb->character->bound_region_id, AP_EVENT_BINDING_TYPE_RESURRECTION, NULL);
	if (!destination)
		return TRUE;
	ap_event_teleport_start(mod->ap_event_teleport, cb->character, destination);
	return TRUE;
}

static void teleportchaoticfrontier(
	struct as_service_npc_process_module * mod,
	struct ap_character * character,
	const struct au_pos * destination)
{
	if (ap_tick_get(mod->ap_tick) < character->combat_end_tick)
		return;
	as_skill_reset_all_buffs(mod->as_skill, character);
	ap_character_special_status_on(mod->ap_character, character, 
		AP_CHARACTER_SPECIAL_STATUS_INVINCIBLE, 10000);
	ap_event_teleport_start(mod->ap_event_teleport, character, destination);
}

static boolean cbmenuforestofscream(
	struct as_service_npc_process_module * mod,
	struct ap_event_npc_dialog_menu_callback * cb)
{
	struct au_pos destination = { -270336.0f, 0.0f, -431891.0f };
	teleportchaoticfrontier(mod, cb->character, &destination);
	return TRUE;
}

static boolean cbmenulandofice(
	struct as_service_npc_process_module * mod,
	struct ap_event_npc_dialog_menu_callback * cb)
{
	struct au_pos destination = { -383700.0f, 0.0f, -497962.0f };
	teleportchaoticfrontier(mod, cb->character, &destination);
	return TRUE;
}

static boolean cbmenudungeon1f(
	struct as_service_npc_process_module * mod,
	struct ap_event_npc_dialog_menu_callback * cb)
{
	struct au_pos destination = { -379397.0f, 0.0f, -449824.0f };
	teleportchaoticfrontier(mod, cb->character, &destination);
	return TRUE;
}

static boolean cbmenudungeon2f(
	struct as_service_npc_process_module * mod,
	struct ap_event_npc_dialog_menu_callback * cb)
{
	struct au_pos destination = { -342331.0f, 0.0f, -424552.0f };
	teleportchaoticfrontier(mod, cb->character, &destination);
	return TRUE;
}

static boolean cbmenuforestofpain(
	struct as_service_npc_process_module * mod,
	struct ap_event_npc_dialog_menu_callback * cb)
{
	struct au_pos destination = { -476833.0f, 0.0f, -480014.0f };
	teleportchaoticfrontier(mod, cb->character, &destination);
	return TRUE;
}

static boolean cbmenutowerofdespair(
	struct as_service_npc_process_module * mod,
	struct ap_event_npc_dialog_menu_callback * cb)
{
	struct au_pos destination = { -431327.0f, 0.0f, -435672.0f };
	teleportchaoticfrontier(mod, cb->character, &destination);
	return TRUE;
}

static struct ap_item * cbgeneratereward(
	struct as_service_npc_process_module * mod,
	const struct ap_item_template * temp,
	uint32_t stack_count,
	const struct ap_service_npc_level_up_reward * reward)
{
	struct ap_item * item = ap_item_create(mod->ap_item, temp->tid);
	if (!item)
		return NULL;
	item->update_flags = AP_ITEM_UPDATE_ALL;
	item->stack_count = stack_count;
	if (temp->type == AP_ITEM_TYPE_EQUIP) {
		struct ap_item_convert_item * attachment;
		uint32_t i;
		if (temp->option_count) {
			uint32_t i;
			item->option_count = temp->option_count;
			for (i = 0; i < temp->option_count; i++) {
				item->option_tid[i] = temp->option_tid[i];
				item->options[i] = temp->options[i];
			}
		}
		else {
			ap_drop_item_generate_options(mod->ap_drop_item, item);
		}
		if (reward->expire_sec)
			item->expire_time = time(NULL) + reward->expire_sec;
		if (temp->status_flags & AP_ITEM_STATUS_FLAG_BIND_ON_ACQUIRE)
			item->status_flags |= AP_ITEM_STATUS_FLAG_BIND_ON_OWNER;
		attachment = ap_item_convert_get_item(mod->ap_item_convert, item);
		attachment->socket_count = MAX(1, MIN(reward->socket_count, 
			temp->max_socket_count));
		attachment->update_flags = AP_ITEM_CONVERT_UPDATE_ALL;
		for (i = 0; i < COUNT_OF(reward->convert_tid); i++) {
			struct ap_item_template * converttemp;
			uint32_t index;
			if (!reward->convert_tid[i])
				break;
			if (attachment->convert_count >= attachment->socket_count)
				break;
			converttemp = ap_item_get_template(mod->ap_item, reward->convert_tid[i]);
			if (!converttemp)
				continue;
			if (converttemp->type != AP_ITEM_TYPE_USABLE)
				continue;
			if (converttemp->usable.usable_type != AP_ITEM_USABLE_TYPE_RUNE)
				continue;
			index = attachment->convert_count++;
			attachment->socket_attr[index].is_spirit_stone = FALSE;
			attachment->socket_attr[index].tid = reward->convert_tid[i];
			attachment->socket_attr[index].item_template = converttemp;
		}
	}
	return item;
}


static boolean cbmenulevelupreward(
	struct as_service_npc_process_module * mod,
	struct ap_event_npc_dialog_menu_callback * cb)
{
	struct ap_character * character = cb->character;
	const struct ap_service_npc_level_up_reward * reward = cb->callback_data;
	uint32_t level = reward->level;
	uint32_t count = 0;
	const struct ap_service_npc_level_up_reward * rewards;
	uint32_t beginindex = UINT32_MAX;
	uint32_t i;
	uint32_t rewardcount = 0;
	if (ap_character_get_absolute_level(cb->character) < reward->level) {
		ap_event_npc_dialog_make_grant_msg_box_packet(mod->ap_event_npc_dialog,
			character->id, cb->npc->id, 0, AP_EVENT_NPC_DIALOG_MSG_BOX_OK,
			"Character level is too low.");
		as_player_send_packet(mod->as_player, character);
		return TRUE;
	}
	rewards = ap_service_npc_get_level_up_rewards(mod->ap_service_npc, &count);
	for (i = 0; i < count; i++) {
		if (rewards[i].level == level) {
			beginindex = i;
			break;
		}
	}
	for (i = beginindex; i < count; i++) {
		reward = &rewards[i];
		if (reward->level != level)
			break;
		if (reward->race && reward->race != character->factor.char_type.race)
			continue;
		if (reward->cs && reward->cs != character->factor.char_type.cs)
			continue;
		rewardcount++;
	}
	if (rewardcount > ap_item_get_empty_slot_count(mod->ap_item, character, AP_ITEM_STATUS_INVENTORY) ||
		rewardcount > ap_item_get_empty_slot_count(mod->ap_item, character, AP_ITEM_STATUS_CASH_INVENTORY)) {
		ap_event_npc_dialog_make_grant_msg_box_packet(mod->ap_event_npc_dialog,
			character->id, cb->npc->id, 0, AP_EVENT_NPC_DIALOG_MSG_BOX_OK,
			"Inventory is full.");
		as_player_send_packet(mod->as_player, character);
		return TRUE;
	}
	if (!as_service_npc_add_received_level_up_reward_milestone(mod->as_service_npc,
			character, level)) {
		ap_event_npc_dialog_make_grant_msg_box_packet(mod->ap_event_npc_dialog,
			character->id, cb->npc->id, 0, AP_EVENT_NPC_DIALOG_MSG_BOX_OK,
			"Level up rewards have already been received.");
		as_player_send_packet(mod->as_player, character);
		return TRUE;
	}
	for (i = beginindex; i < count; i++) {
		reward = &rewards[i];
		if (reward->level != level)
			break;
		if (reward->race && reward->race != character->factor.char_type.race)
			continue;
		if (reward->cs && reward->cs != character->factor.char_type.cs)
			continue;
		as_drop_item_distribute_exclusive(mod->as_drop_item, character, 
			reward->item_tid, reward->stack_count, 
			mod, (void *)reward, cbgeneratereward);
		ap_item_make_custom_pick_up_item_result_packet(mod->ap_item,
			reward->item_tid, reward->stack_count, AP_ITEM_PICK_UP_RESULT_SUCCESS);
		as_player_send_packet(mod->as_player, character);
	}
	ap_event_npc_dialog_make_grant_msg_box_packet(mod->ap_event_npc_dialog,
		character->id, cb->npc->id, 0, AP_EVENT_NPC_DIALOG_MSG_BOX_OK,
		"Level up rewards have been received.");
	as_player_send_packet(mod->as_player, character);
	return TRUE;
}

static struct ap_event_npc_dialog_menu_item * buildeventnpcmenu(
	struct as_service_npc_process_module * mod,
	struct ap_character * character)
{
	struct ap_event_npc_dialog_menu_item * menu;
	struct ap_event_npc_dialog_menu_item * rewardmenu;
	uint32_t currentlevel = UINT32_MAX;
	uint32_t i;
	uint32_t levelcount = 0;
	uint32_t count = 0;
	const struct ap_service_npc_level_up_reward * rewards = 
		ap_service_npc_get_level_up_rewards(mod->ap_service_npc, &count);
	menu = ap_event_npc_dialog_add_menu(mod->ap_event_npc_dialog, 
		NULL, NULL, "Event Manager", "List of ongoing events.");
	rewardmenu = ap_event_npc_dialog_add_menu(mod->ap_event_npc_dialog, menu,
		"Level Up Rewards", "Level Up Rewards", "Select a level to receive rewards.");
	for (i = 0; i < count; i++) {
		uint32_t level = rewards[i].level;
		char text[AP_EVENT_NPC_DIALOG_MAX_BUTTON_TEXT_LENGTH + 1];
		if (level == currentlevel)
			continue;
		currentlevel = level;
		if (levelcount && !(levelcount % 7)) {
			struct ap_event_npc_dialog_menu_item * next =
				ap_event_npc_dialog_add_menu(mod->ap_event_npc_dialog, rewardmenu,
					"Next Page", "Level Up Rewards", "Select a level to receive rewards.");
			ap_event_npc_dialog_add_button(mod->ap_event_npc_dialog, rewardmenu,
				"Exit", NULL, NULL, NULL);
			rewardmenu = next;
		}
		snprintf(text, sizeof(text), "Lv %u", level);
		ap_event_npc_dialog_add_button(mod->ap_event_npc_dialog, rewardmenu,
			text, mod, cbmenulevelupreward, (void *)&rewards[i]);
		levelcount++;
	}
	ap_event_npc_dialog_add_button(mod->ap_event_npc_dialog, rewardmenu,
		"Exit", NULL, NULL, NULL);
	ap_event_npc_dialog_add_button(mod->ap_event_npc_dialog, menu,
		"Exit", NULL, NULL, NULL);
	return menu;
}

static boolean cbcharinitstatic(
	struct as_service_npc_process_module * mod,
	struct ap_character_cb_init_static * cb)
{
	struct ap_character * character = cb->character;
	struct ap_event_npc_dialog_menu_item * menu = NULL;
	switch (character->tid) {
	case CHAOTICFRONTIERNPCTID:
		if (strstr(character->name, "Portal to Town")) {
			menu = ap_event_npc_dialog_add_menu(mod->ap_event_npc_dialog, 
				NULL, NULL, "Chaotic Frontier Portal", "Return to town.");
			ap_event_npc_dialog_add_button(mod->ap_event_npc_dialog, menu,
				"Return", mod, cbmenureturn, NULL);
			ap_event_npc_dialog_add_button(mod->ap_event_npc_dialog, menu,
				"Exit", NULL, NULL, NULL);
		}
		else {
			menu = ap_event_npc_dialog_add_menu(mod->ap_event_npc_dialog, 
				NULL, NULL, "Chaotic Frontier Portal", "Please select your destination.");
			ap_event_npc_dialog_add_button(mod->ap_event_npc_dialog, menu,
				"Forest of Scream", mod, cbmenuforestofscream, NULL);
			ap_event_npc_dialog_add_button(mod->ap_event_npc_dialog, menu,
				"Land of Ice", mod, cbmenulandofice, NULL);
			ap_event_npc_dialog_add_button(mod->ap_event_npc_dialog, menu,
				"Dungeon 1F", mod, cbmenudungeon1f, NULL);
			ap_event_npc_dialog_add_button(mod->ap_event_npc_dialog, menu,
				"Dungeon 2F", mod, cbmenudungeon2f, NULL);
			ap_event_npc_dialog_add_button(mod->ap_event_npc_dialog, menu,
				"Forest of Pain", mod, cbmenuforestofpain, NULL);
			ap_event_npc_dialog_add_button(mod->ap_event_npc_dialog, menu,
				"Tower of Despair", mod, cbmenutowerofdespair, NULL);
			ap_event_npc_dialog_add_button(mod->ap_event_npc_dialog, menu,
				"Exit", NULL, NULL, NULL);
		}
		break;
	case EVENTNPCTID:
		menu = buildeventnpcmenu(mod, character);
		break;
	}
	if (menu) {
		struct ap_event_npc_dialog_character_attachment * attachment = 
			ap_event_npc_dialog_get_character_attachment(mod->ap_event_npc_dialog, character);
		attachment->menu = menu;
	}
	return TRUE;
}

static void cbchatresetleveluprewards(
	struct as_service_npc_process_module * mod,
	struct ap_character * c,
	uint32_t argc,
	const char * const * argv)
{
	as_service_npc_reset_received_level_up_rewards(mod->as_service_npc, c);
}

static boolean onregister(
	struct as_service_npc_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_chat, AP_CHAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_drop_item, AP_DROP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_npc_dialog, AP_EVENT_NPC_DIALOG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_teleport, AP_EVENT_TELEPORT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item_convert, AP_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_service_npc, AP_SERVICE_NPC_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_drop_item, AS_DROP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_event_binding, AS_EVENT_BINDING_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_service_npc, AS_SERVICE_NPC_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_skill, AS_SKILL_MODULE_NAME);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_INIT_STATIC, mod, cbcharinitstatic);
	ap_chat_add_command(mod->ap_chat, "/resetleveluprewards", mod, cbchatresetleveluprewards);
	return TRUE;
}

static void onshutdown(struct as_service_npc_process_module * mod)
{
}

struct as_service_npc_process_module * as_service_npc_process_create_module()
{
	struct as_service_npc_process_module * mod = ap_module_instance_new(AS_SERVICE_NPC_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	return mod;
}
