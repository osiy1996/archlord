#include "server/as_game_admin.h"

#include "core/log.h"
#include "core/malloc.h"

#include "public/ap_admin.h"
#include "public/ap_bill_info.h"
#include "public/ap_cash_mall.h"
#include "public/ap_chat.h"
#include "public/ap_config.h"
#include "public/ap_define.h"
#include "public/ap_event_bank.h"
#include "public/ap_event_guild.h"
#include "public/ap_event_item_convert.h"
#include "public/ap_event_npc_dialog.h"
#include "public/ap_event_npc_trade.h"
#include "public/ap_event_refinery.h"
#include "public/ap_event_skill_master.h"
#include "public/ap_event_teleport.h"
#include "public/ap_guild.h"
#include "public/ap_item.h"
#include "public/ap_item_convert.h"
#include "public/ap_module.h"
#include "public/ap_optimized_packet2.h"
#include "public/ap_party.h"
#include "public/ap_pvp.h"
#include "public/ap_refinery.h"
#include "public/ap_skill.h"
#include "public/ap_tick.h"
#include "public/ap_ui_status.h"

#include "server/as_guild.h"
#include "server/as_item.h"
#include "server/as_login.h"
#include "server/as_map.h"
#include "server/as_player.h"
#include "server/as_server.h"

#include <assert.h>

enum stage {
	STAGE_ANY,
	STAGE_AWAIT_AUTH_KEY,
	STAGE_AWAIT_ENTER_GAME,
	STAGE_IN_GAME,
	STAGE_DETACHED,
	STAGE_DISCONNECTED
};

struct conn_ad {
	enum stage stage;
	struct as_account * account;
	struct ap_character * character;
};

struct as_game_admin_module {
	struct ap_module_instance instance;
	struct ap_bill_info_module * ap_bill_info;
	struct ap_character_module * ap_character;
	struct ap_cash_mall_module * ap_cash_mall;
	struct ap_chat_module * ap_chat;
	struct ap_config_module * ap_config;
	struct ap_event_bank_module * ap_event_bank;
	struct ap_event_guild_module * ap_event_guild;
	struct ap_event_item_convert_module * ap_event_item_convert;
	struct ap_event_npc_dialog_module * ap_event_npc_dialog;
	struct ap_event_npc_trade_module * ap_event_npc_trade;
	struct ap_event_refinery_module * ap_event_refinery;
	struct ap_event_skill_master_module * ap_event_skill_master;
	struct ap_event_teleport_module * ap_event_teleport;
	struct ap_guild_module * ap_guild;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct ap_optimized_packet2_module * ap_optimized_packet2;
	struct ap_party_module * ap_party;
	struct ap_pvp_module * ap_pvp;
	struct ap_refinery_module * ap_refinery;
	struct ap_skill_module * ap_skill;
	struct ap_tick_module * ap_tick;
	struct ap_ui_status_module * ap_ui_status;
	struct as_account_module * as_account;
	struct as_character_module * as_character;
	struct as_guild_module * as_guild;
	struct as_item_module * as_item;
	struct as_login_module * as_login;
	struct as_map_module * as_map;
	struct as_player_module * as_player;
	struct as_server_module * as_server;
	enum stage req_stage[UINT8_MAX];
	size_t conn_ad_offset;
};

static struct conn_ad * get_conn_ad(
	struct as_game_admin_module * mod,
	struct as_server_conn * c)
{
	return ap_module_get_attached_data(c, mod->conn_ad_offset);
}

static void detachplayer(
	struct conn_ad * ad,
	struct as_player_character * pc)
{
	ad->account = NULL;
	ad->character = NULL;
	ad->stage = STAGE_DETACHED;
	pc->conn = NULL;
}

static boolean kickclient(
	struct as_game_admin_module * mod,
	struct as_server_conn * conn)
{
	/* Disconnecting previous connection 
	 * will trigger disconnect callback, 
	 * which may remove player character.
	 * 
	 * So prior to that we need to detach 
	 * the player from previous connection. */
	struct conn_ad * ad;
	ad = get_conn_ad(mod, conn);
	detachplayer(ad, as_player_get_character_ad(mod->as_player, ad->character));
	as_server_disconnect(mod->as_server, conn);
	return TRUE;
}

/*
 * This function assumes that player is already 
 * detached from connection.
 */
static void removeplayer(
	struct as_game_admin_module * mod,
	struct ap_character * character,
	struct as_account * account)
{
	if (!as_player_remove(mod->as_player, character)) {
		assert(0);
		ERROR("Failed to remove player (%s).", character->name);
	}
	as_map_remove_character(mod->as_map, character);
	as_character_reflect(mod->as_character, character);
	ap_character_free(mod->ap_character, character);
	as_account_release(account);
}

static void dcsameaccount(
	struct as_game_admin_module * mod,
	struct as_account * account)
{
	size_t index = 0;
	while (TRUE) {
		struct ap_character * c = as_player_iterate(mod->as_player, &index);
		struct as_player_character * pc;
		if (!c)
			break;
		pc = as_player_get_character_ad(mod->as_player, c);
		if (pc->account == account) {
			struct as_account * account = pc->account;
			if (pc->conn)
				kickclient(mod, pc->conn);
			removeplayer(mod, c, account);
		}
	}
}

static void informbufflist(
	struct as_game_admin_module * mod,
	struct ap_character * character)
{
	const struct ap_skill_character * attachment = 
		ap_skill_get_character(mod->ap_skill, character);
	uint32_t i;
	for (i = 0; i < attachment->buff_count; i++) {
		const struct ap_skill_buff_list * buff = &attachment->buff_list[i];
		if (buff->temp->attribute & AP_SKILL_ATTRIBUTE_PASSIVE)
			continue;
		ap_skill_make_add_buffed_list_packet(mod->ap_skill, character, buff);
		as_player_send_packet(mod->as_player, character);
	}
}

static void constructbank(
	struct as_game_admin_module * mod,
	struct ap_character * character,
	struct as_account * account)
{
	struct ap_item_character * attachment = 
		ap_item_get_character(mod->ap_item, character);
	struct as_item_account_attachment * accountattachment =
		as_item_get_account_attachment(mod->as_item, account);
	uint32_t i;
	for (i = 0; i < accountattachment->item_count; i++) {
		struct as_item_db * idb = accountattachment->items[i];
		struct ap_item * item = as_item_from_db(mod->as_item, idb);
		struct ap_grid * grid;
		if (!item) {
			ERROR("Failed to create database character item (character = %s, item_tid = %u).",
				character->name, idb->tid);
			memmove(&accountattachment->items[i], &accountattachment->items[i + 1], 
				(--accountattachment->item_count - i) * sizeof(accountattachment->items[0]));
			i--;
			continue;
		}
		switch (item->status) {
		case AP_ITEM_STATUS_BANK:
			grid = attachment->bank;
			break;
		default:
			assert(0);
			ERROR("Invalid database character item status (character = %s, item_tid = %u, item_status = %d).",
				character->name, idb->tid, idb->status);
			ap_item_free(mod->ap_item, item);
			continue;
		}
		if (!ap_grid_is_free(grid, idb->layer, idb->row, idb->column)) {
			assert(0);
			ERROR("Grid is already occupied by another item (character = %s, item_tid = %u, item_status = %d).",
				character->name, idb->tid, idb->status);
			ap_item_free(mod->ap_item, item);
			continue;
		}
		item->character_id = character->id;
		ap_grid_add_item(grid, idb->layer, idb->row, 
			idb->column, AP_GRID_ITEM_TYPE_ITEM, item->id, 
			item->tid, item);
	}
}

static void constructquickbelt(
	struct as_game_admin_module * mod,
	struct ap_character * character)
{
	uint32_t i;
	struct ap_ui_status_character * attachment = 
		ap_ui_status_get_character(mod->ap_ui_status, character);
	for (i = 0; i < COUNT_OF(attachment->items); i++) {
		struct ap_ui_status_item * quickitem = &attachment->items[i];
		switch (quickitem->base.type) {
		case AP_BASE_TYPE_ITEM: {
			struct ap_item * item = ap_item_find_usable_item_without_status_flag(mod->ap_item, 
				character, quickitem->tid, AP_ITEM_STATUS_FLAG_CASH_PPCARD, 3, 
				AP_ITEM_STATUS_INVENTORY, AP_ITEM_STATUS_SUB_INVENTORY, 
				AP_ITEM_STATUS_CASH_INVENTORY);
			if (item)
				quickitem->base.id = item->id;
			else
				memset(quickitem, 0, sizeof(*quickitem));
			break;
		}
		case AP_BASE_TYPE_SKILL: {
			struct ap_skill * skill = ap_skill_find_by_tid(mod->ap_skill, 
				character, quickitem->tid);
			if (skill)
				quickitem->base.id = skill->id;
			else
				memset(quickitem, 0, sizeof(*quickitem));
			break;
		}
		}
	}
}

static boolean handle_ac_character(
	struct as_game_admin_module * mod,
	struct as_server_conn * conn,
	struct conn_ad * ad,
	const void * data,
	uint16_t length)
{
	enum ac_character_packet_type type = 0xff;
	char cname[AP_CHARACTER_MAX_NAME_LENGTH + 1] = "";
	uint32_t authkey = 0;
	if (!ap_character_parse_client_packet(mod->ap_character, data, length, &type,
			cname, &authkey)) {
		return FALSE;
	}
	switch (type) {
	case AC_CHARACTER_PACKET_REQUEST_CLIENT_CONNECT: {
		const char * accountid;
		struct as_account * account;
		struct as_character_db * cdb = NULL;
		struct ap_character * c;
		struct as_player_character * pc;
		uint32_t i;
		boolean addtomap = FALSE;
		if (ad->stage != STAGE_AWAIT_AUTH_KEY)
			return FALSE;
		c = as_player_get_by_name(mod->as_player, cname);
		if (c) {
			/* Player character remains online, either 
			 * accessed by a client or idle without a 
			 * client connection. */
			if (!as_login_confirm_auth_key(mod->as_login, cname, authkey))
				return FALSE;
			pc = as_player_get_character_ad(mod->as_player, c);
			if (pc->conn && !kickclient(mod, pc->conn)) {
				ERROR("Failed to kick previous client (%s).",
					cname);
				assert(0);
				return FALSE;
			}
			pc->conn = conn;
			account = pc->account;
			TRACE("Takeover player character (%s).", cname);
		}
		else {
			accountid = as_character_get_account_id(mod->as_character, cname);
			if (!accountid)
				return FALSE;
			account = as_account_load_from_cache(mod->as_account, accountid, TRUE);
			if (!account)
				return FALSE;
			if (!as_login_confirm_auth_key(mod->as_login, cname, authkey)) {
				as_account_release(account);
				return FALSE;
			}
			for (i = 0; i < account->character_count; i++) {
				if (strcmp(account->characters[i]->name, cname) == 0) {
					cdb = account->characters[i];
					break;
				}
			}
			if (!cdb) {
				assert(0);
				as_account_release(account);
				return FALSE;
			}
			dcsameaccount(mod, account);
			c = as_character_from_db(mod->as_character, cdb);
			c->bank_gold = account->bank_gold;
			pc = as_player_get_character_ad(mod->as_player, c);
			pc->conn = conn;
			pc->account = account;
			pc->session = as_player_get_session(mod->as_player, cdb->name);
			constructbank(mod, c, account);
			constructquickbelt(mod, c);
			as_player_add(mod->as_player, c);
			ap_character_update_factor(mod->ap_character, c, 
				AP_FACTORS_BIT_MOVEMENT_SPEED |
				AP_FACTORS_BIT_PHYSICAL_DMG |
				AP_FACTORS_BIT_ATTACK_RATING |
				AP_FACTORS_BIT_DEFENSE_RATING);
			addtomap = TRUE;
			TRACE("Insert player character (%s).", cname);
		}
		ad->stage = STAGE_AWAIT_ENTER_GAME;
		ad->account = account;
		ad->character = c;
		ap_config_make_packet(mod->ap_config, FALSE, TRUE, FALSE);
		as_server_send_packet(mod->as_server, conn);
		ap_character_make_event_effect_packet(mod->ap_character, c, 0x4C);
		as_server_send_packet(mod->as_server, conn);
		ap_character_make_client_packet(mod->ap_character, 
			AC_CHARACTER_PACKET_CHARACTER_POSITION, NULL,
			NULL, NULL, &c->id, &c->pos, NULL);
		as_server_send_packet(mod->as_server, conn);
		if (addtomap) {
			/* Adding character to map will trigger various 
			 * packets (things on map such as other characters, 
			 * NPCs, items, etc.) to be sent, if these packets 
			 * are sent before the packets above, 
			 * they will be ignored.
			 * 
			 * So adding character to map (if needed) is delayed 
			 * until this point. */
			as_map_add_character(mod->as_map, c);
		}
		else {
			as_map_inform_nearby(mod->as_map, c);
		}
		break;
	}
	case AC_CHARACTER_PACKET_LOADING_COMPLETE: {
		struct ap_character * c;
		struct ap_guild_character * gc;
		if (ad->stage != STAGE_AWAIT_ENTER_GAME)
			return FALSE;
		ad->stage = STAGE_IN_GAME;
		c = ad->character;
		c->login_status = AP_CHARACTER_STATUS_IN_GAME_WORLD;
		ap_character_make_add_packet(mod->ap_character, c);
		as_server_send_packet(mod->as_server, conn);
		ap_optimized_packet2_make_char_all_item_skill(mod->ap_optimized_packet2, c);
		as_server_send_packet(mod->as_server, conn);
		ap_character_make_client_packet(mod->ap_character, 
			AC_CHARACTER_PACKET_SETTING_CHARACTER_OK, NULL,
			NULL, NULL, &c->id, NULL, NULL);
		as_server_send_packet(mod->as_server, conn);
		ap_character_update(mod->ap_character, c, 
			AP_FACTORS_BIT_CHAR_STATUS | 
			AP_FACTORS_BIT_DAMAGE |
			AP_FACTORS_BIT_DEFENSE |
			AP_FACTORS_BIT_SKILL_POINT |
			AP_FACTORS_BIT_HEROIC_POINT |
			AP_CHARACTER_BIT_INVENTORY_GOLD, TRUE);
		informbufflist(mod, c);
		ap_ui_status_make_add_packet(mod->ap_ui_status, c);
		as_server_send_packet(mod->as_server, conn);
		gc = ap_guild_get_character(mod->ap_guild, c);
		if (gc->guild) {
			ap_guild_make_add_packet(mod->ap_guild, gc->guild);
			as_player_send_packet(mod->as_player, c);
			ap_guild_make_char_data_packet(mod->ap_guild, gc->guild, 
				gc->member, c->id);
			as_player_send_packet(mod->as_player, c);
			as_guild_broadcast_members(mod->as_guild, gc->guild, c);
		}
		ap_bill_info_make_cash_info_packet(mod->ap_bill_info, c->id, 0,
			as_player_get_character_ad(mod->as_player, c)->account->chantra_coins);
		as_player_send_packet(mod->as_player, c);
		ap_pvp_make_info_packet(mod->ap_pvp, c);
		as_player_send_packet(mod->as_player, c);
		if (ap_tick_get(mod->ap_tick) >= c->combat_end_tick) {
			ap_character_special_status_on(mod->ap_character, c, 
				AP_CHARACTER_SPECIAL_STATUS_INVINCIBLE, 10000);
		}
		break;
	}
	default:
		return FALSE;
	}
	return TRUE;
}

static boolean cbreceive(struct as_game_admin_module * mod, void * data)
{
	struct as_server_cb_receive * d = data;
	struct conn_ad * ad;
	if (d->conn->server_type != AS_SERVER_GAME)
		return TRUE;
	ad = get_conn_ad(mod, d->conn);
	if (mod->req_stage[d->packet_type] != STAGE_ANY &&
		mod->req_stage[d->packet_type] != ad->stage) {
		return FALSE;
	}
	switch (d->packet_type) {
	case AC_CHARACTER_PACKET_TYPE:
		return handle_ac_character(mod, d->conn, ad, d->data, d->length);
	case AP_CHARACTER_PACKET_TYPE:
		ap_character_on_receive(mod->ap_character, d->data, d->length, ad->character);
		break;
	case AP_CASH_MALL_PACKET_TYPE:
		ap_cash_mall_on_receive(mod->ap_cash_mall, d->data, d->length, ad->character);
		break;
	case AP_CHATTING_PACKET_TYPE:
		ap_chat_on_receive(mod->ap_chat, d->data, d->length, ad->character);
		break;
	case AP_EVENT_BANK_PACKET_TYPE:
		ap_event_bank_on_receive(mod->ap_event_bank, d->data, d->length, ad->character);
		break;
	case AP_EVENT_GUILD_PACKET_TYPE:
		ap_event_guild_on_receive(mod->ap_event_guild, d->data, d->length, ad->character);
		break;
	case AP_EVENT_NPCTRADE_PACKET_TYPE:
		ap_event_npc_trade_on_receive(mod->ap_event_npc_trade, d->data, d->length, ad->character);
		break;
	case AP_EVENT_NPCDIALOG_PACKET_TYPE:
		ap_event_npc_dialog_on_receive(mod->ap_event_npc_dialog, d->data, d->length, ad->character);
		break;
	case AP_EVENT_ITEMCONVERT_PACKET_TYPE:
		ap_event_item_convert_on_receive(mod->ap_event_item_convert, d->data, d->length, ad->character);
		break;
	case AP_EVENT_REFINERY_PACKET_TYPE:
		ap_event_refinery_on_receive(mod->ap_event_refinery, d->data, d->length, ad->character);
		break;
	case AP_EVENT_SKILLMASTER_PACKET_TYPE:
		ap_event_skill_master_on_receive(mod->ap_event_skill_master, d->data, d->length, ad->character);
		break;
	case AP_EVENT_TELEPORT_PACKET_TYPE:
		ap_event_teleport_on_receive(mod->ap_event_teleport, d->data, d->length, ad->character);
		break;
	case AP_GUILD_PACKET_TYPE:
		ap_guild_on_receive(mod->ap_guild, d->data, d->length, ad->character);
		break;
	case AP_ITEM_PACKET_TYPE:
		ap_item_on_receive(mod->ap_item, d->data, d->length, ad->character);
		break;
	case AP_ITEM_CONVERT_PACKET_TYPE:
		ap_item_convert_on_receive(mod->ap_item_convert, d->data, d->length, ad->character);
		break;
	case AP_OPTIMIZEDCHARMOVE_PACKET_TYPE:
		ap_optimized_packet2_on_receive_char_move(mod->ap_optimized_packet2, d->data, d->length, ad->character);
		break;
	case AP_OPTIMIZEDCHARACTION_PACKET_TYPE:
		ap_optimized_packet2_on_receive_char_action(mod->ap_optimized_packet2, d->data, d->length, ad->character);
		break;
	case AP_PARTY_PACKET_TYPE:
		ap_party_on_receive(mod->ap_party, d->data, d->length, ad->character);
		break;
	case AP_PARTY_MANAGEMENT_PACKET_TYPE:
		ap_party_on_receive_management(mod->ap_party, d->data, d->length, ad->character);
		break;
	case AP_REFINERY_PACKET_TYPE:
		return ap_refinery_on_receive(mod->ap_refinery, d->data, d->length, ad->character);
	case AP_SKILL_PACKET_TYPE:
		return ap_skill_on_receive(mod->ap_skill, d->data, d->length, ad->character);
	case AP_UISTATUS_PACKET_TYPE:
		ap_ui_status_on_receive(mod->ap_ui_status, d->data, d->length, ad->character);
		break;
	default:
		//TRACE("Unhandled packet type (%02X).", d->packet_type);
		break;
	}
	return TRUE;
}

static boolean cbdisconnect(
	struct as_game_admin_module * mod,
	struct as_server_conn * conn)
{
	struct conn_ad * ad;
	if (conn->server_type != AS_SERVER_GAME)
		return TRUE;
	ad = get_conn_ad(mod, conn);
	if (ad->stage == STAGE_AWAIT_ENTER_GAME|| 
		ad->stage == STAGE_IN_GAME) {
		struct ap_character * c = ad->character;
		struct as_account * account = ad->account;
		assert(ad->stage == STAGE_IN_GAME);
		detachplayer(ad, as_player_get_character_ad(mod->as_player, ad->character));
		/* Here, we can decide whether to remove player 
		 * character from the world or not.
		 * 
		 * An example is, if player is in combat, 
		 * we can schedule for it to be removed 
		 * in 20 seconds or so.
		 * 
		 * Another example is the arena.
		 * If client disconnects for any reason, we can 
		 * retain the character in the world until the 
		 * end of the arena game. 
		 * 
		 * For testing purposes, it is defaulted to retain 
		 * the character in the world for the time being. */
		if (FALSE)
			removeplayer(mod, c, account);
	}
	ad->stage = STAGE_DISCONNECTED;
	return TRUE;
}

static boolean cblogin(
	struct as_game_admin_module * mod, 
	struct as_login_cb_logged_in * d)
{
	return TRUE;
}

static boolean connctor(struct as_game_admin_module * mod, void * data)
{
	struct as_server_conn * conn = data;
	struct conn_ad * ad;
	ad = get_conn_ad(mod, conn);
	ad->stage = STAGE_AWAIT_AUTH_KEY;
	return TRUE;
}

static boolean conndtor(struct as_game_admin_module * mod, void * data)
{
	struct as_server_conn * conn = data;
	struct conn_ad * ad;
	if (conn->server_type != AS_SERVER_GAME)
		return TRUE;
	ad = get_conn_ad(mod, conn);
	return TRUE;
}

static boolean onregister(
	struct as_game_admin_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_bill_info, AP_BILL_INFO_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_cash_mall, AP_CASH_MALL_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_chat, AP_CHAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_bank, AP_EVENT_BANK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_guild, AP_EVENT_GUILD_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_item_convert, AP_EVENT_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_npc_dialog, AP_EVENT_NPC_DIALOG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_npc_trade, AP_EVENT_NPC_TRADE_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_refinery, AP_EVENT_REFINERY_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_skill_master, AP_EVENT_SKILL_MASTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_teleport, AP_EVENT_TELEPORT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_guild, AP_GUILD_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item_convert, AP_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_optimized_packet2, AP_OPTIMIZED_PACKET2_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_party, AP_PARTY_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_pvp, AP_PVP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_refinery, AP_REFINERY_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_skill, AP_SKILL_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_ui_status, AP_UI_STATUS_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_account, AS_ACCOUNT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_guild, AS_GUILD_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_item, AS_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_login, AS_LOGIN_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_server, AS_SERVER_MODULE_NAME);
	mod->conn_ad_offset = as_server_attach_data(mod->as_server, 
		AS_SERVER_MDI_CONNECTION, sizeof(struct conn_ad),
		mod, connctor, conndtor);
	if (mod->conn_ad_offset == SIZE_MAX) {
		ERROR("Failed to attach server connection data.");
		return FALSE;
	}
	as_server_add_callback(mod->as_server, AS_SERVER_CB_RECEIVE, mod, cbreceive);
	as_server_add_callback(mod->as_server, AS_SERVER_CB_DISCONNECT, mod, cbdisconnect);
	as_login_add_callback(mod->as_login, AS_LOGIN_CB_LOGGED_IN, mod, cblogin);
	return TRUE;
}

static void onclose(struct as_game_admin_module * mod)
{
	size_t index = 0;
	while (TRUE) {
		struct ap_character * c = as_player_iterate(mod->as_player, &index);
		struct as_player_character * pc;
		struct as_account * account;
		if (!c)
			break;
		pc = as_player_get_character_ad(mod->as_player, c);
		account = pc->account;
		if (pc->conn)
			detachplayer(get_conn_ad(mod, pc->conn), pc);
		removeplayer(mod, c, account);
		index = 0;
	}
}

struct as_game_admin_module * as_game_admin_create_module()
{
	struct as_game_admin_module * mod = ap_module_instance_new(AS_GAME_ADMIN_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, onclose, NULL);
	uint32_t i;
	for (i = 0; i < COUNT_OF(mod->req_stage); i++)
		mod->req_stage[i] = STAGE_IN_GAME;
	mod->req_stage[AC_CHARACTER_PACKET_TYPE] = STAGE_ANY;
	return mod;
}
