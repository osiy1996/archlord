#include "server/as_private_trade_process.h"

#include "core/log.h"

#include "public/ap_bill_info.h"
#include "public/ap_chat.h"
#include "public/ap_item.h"
#include "public/ap_item_convert.h"
#include "public/ap_module_instance.h"
#include "public/ap_private_trade.h"

#include "server/as_item.h"
#include "server/as_player.h"

#include <assert.h>
#include <stdlib.h>

struct as_private_trade_process_module {
	struct ap_module_instance instance;
	struct ap_bill_info_module * ap_bill_info;
	struct ap_character_module * ap_character;
	struct ap_chat_module * ap_chat;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct ap_private_trade_module * ap_private_trade;
	struct as_character_module * as_character;
	struct as_item_module * as_item;
	struct as_player_module * as_player;
};

static boolean cbcharsetactionstatus(
	struct as_private_trade_process_module * mod,
	struct ap_character_cb_set_action_status * cb)
{
	struct ap_private_trade_character_attachment * attachment = 
		ap_private_trade_get_character_attachment(cb->character);
	if (attachment->target)
		ap_private_trade_cancel(mod->ap_private_trade, cb->character, TRUE);
	return TRUE;
}

static boolean cbplayerremove(
	struct as_private_trade_process_module * mod,
	struct as_player_cb_remove * cb)
{
	struct ap_character * character = cb->character;
	struct ap_private_trade_character_attachment * attachment = 
		ap_private_trade_get_character_attachment(character);
	if (attachment->target)
		ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
	return TRUE;
}

static void establishgrid(
	struct as_private_trade_process_module * mod,
	struct ap_character * character,
	struct ap_private_trade_character_attachment * attachment,
	struct ap_character * target)
{
	uint32_t i;
	struct ap_grid * grid = ap_item_get_character_grid(mod->ap_item, character, 
		AP_ITEM_STATUS_TRADE_GRID);
	for (i = 0; i < grid->item_count; i++) {
		struct ap_item * item = ap_grid_get_object_by_index(grid, i);
		if (!item)
			continue;
		ap_item_make_add_packet(mod->ap_item, item);
		as_player_send_packet(mod->as_player, character);
		as_player_send_packet(mod->as_player, target);
		if (item->temp->type == AP_ITEM_TYPE_EQUIP) {
			ap_item_convert_make_add_packet(mod->ap_item_convert, item);
			as_player_send_packet(mod->as_player, character);
			as_player_send_packet(mod->as_player, target);
		}
		if (item->status_flags & AP_ITEM_STATUS_FLAG_BIND_ON_OWNER)
			attachment->bound_item_count++;
		if (AP_ITEM_IS_CASH_ITEM(item))
			attachment->cash_item_count++;
		else
			attachment->inventory_item_count++;
	}
}

static void transferitem(
	struct as_private_trade_process_module * mod,
	struct ap_character * source,
	struct as_item_character_db * source_db,
	struct ap_character * target,
	struct as_item_character_db * target_db,
	struct ap_item * item,
	struct ap_grid * source_grid)
{
	enum ap_item_status targetstatus;
	struct ap_grid * targetgrid;
	struct as_item * itemattachment = as_item_get(mod->as_item, item);
	uint16_t layer = UINT16_MAX;
	uint16_t row = UINT16_MAX;
	uint16_t column = UINT16_MAX;
	assert(itemattachment->db != NULL);
	if (itemattachment->db) {
		uint32_t i;
		for (i = 0; i < source_db->item_count; i++) {
			if (source_db->items[i] == itemattachment->db) {
				memmove(&source_db->items[i], &source_db->items[i + 1],
					((size_t)--source_db->item_count * sizeof(source_db->items[0])));
				break;
			}
		}
	}
	ap_grid_set_empty(source_grid, item->grid_pos[AP_ITEM_GRID_POS_TAB],
		item->grid_pos[AP_ITEM_GRID_POS_ROW], item->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
	ap_item_make_remove_packet(mod->ap_item, item);
	as_player_send_packet(mod->as_player, source);
	as_player_send_packet(mod->as_player, target);
	if (AP_ITEM_IS_CASH_ITEM(item))
		targetstatus = AP_ITEM_STATUS_CASH_INVENTORY;
	else
		targetstatus = AP_ITEM_STATUS_INVENTORY;
	targetgrid = ap_item_get_character_grid(mod->ap_item, target, targetstatus);
	if (!ap_grid_get_empty(targetgrid, &layer, &row, &column)) {
		assert(0);
		ERROR("Inventory is full (status = %u).", targetstatus);
		if (itemattachment->db)
			as_item_free_db(mod->as_item, itemattachment->db);
		ap_item_free(mod->ap_item, item);
		return;
	}
	if (itemattachment->db) {
		assert(target_db->item_count < COUNT_OF(target_db->items));
		if (target_db->item_count < COUNT_OF(target_db->items))
			target_db->items[target_db->item_count++] = itemattachment->db;
	}
	item->character_id = target->id;
	ap_grid_add_item(targetgrid, layer, row, column, AP_GRID_ITEM_TYPE_ITEM, 
		item->id, item->tid, item);
	ap_item_set_status(item, targetstatus);
	ap_item_set_grid_pos(item, layer, row, column);
	ap_item_make_add_packet(mod->ap_item, item);
	as_player_send_packet(mod->as_player, target);
	if (item->temp->type == AP_ITEM_TYPE_EQUIP) {
		ap_item_convert_make_add_packet(mod->ap_item_convert, item);
		as_player_send_packet(mod->as_player, target);
	}
}

static boolean cbreceive(
	struct as_private_trade_process_module * mod,
	struct ap_private_trade_cb_receive * cb)
{
	struct ap_character * character = cb->user_data;
	struct ap_private_trade_character_attachment * attachment = 
		ap_private_trade_get_character_attachment(character);
	switch (cb->type) {
	case AP_PRIVATE_TRADE_PACKET_REQUEST_TRADE: {
		struct ap_character * target;
		struct ap_private_trade_character_attachment * targetattachment;
		if (character->action_status != AP_CHARACTER_ACTION_STATUS_NORMAL ||
			character->factor.char_status.murderer >= AP_CHARACTER_MURDERER_LEVEL2_POINT ||
			attachment->target) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		target = as_player_get_by_id(mod->as_player, cb->target_id);
		if (!target || 
			character == target || 
			target->action_status != AP_CHARACTER_ACTION_STATUS_NORMAL) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		targetattachment = ap_private_trade_get_character_attachment(target);
		if (targetattachment->target ||
			target->factor.char_status.murderer >= AP_CHARACTER_MURDERER_LEVEL2_POINT) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		ap_character_set_action_status(mod->ap_character, character, 
			AP_CHARACTER_ACTION_STATUS_TRADE);
		ap_character_set_action_status(mod->ap_character, target, 
			AP_CHARACTER_ACTION_STATUS_TRADE);
		attachment->target = target;
		attachment->awaiting_confirmation = TRUE;
		targetattachment->target = character;
		targetattachment->awaiting_confirmation = TRUE;
		ap_character_update(mod->ap_character, character, 
			AP_CHARACTER_BIT_ACTION_STATUS, FALSE);
		ap_character_update(mod->ap_character, target, 
			AP_CHARACTER_BIT_ACTION_STATUS, FALSE);
		ap_private_trade_make_packet(mod->ap_private_trade, 
			AP_PRIVATE_TRADE_PACKET_REQUEST_CONFIRM, 
			&character->id, &target->id, NULL);
		as_player_send_packet(mod->as_player, target);
		break;
	}
	case AP_PRIVATE_TRADE_PACKET_CONFIRM: {
		struct ap_character * target;
		struct ap_private_trade_character_attachment * targetattachment;
		if (!attachment->target) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		if (!attachment->awaiting_confirmation) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		assert(character->action_status == AP_CHARACTER_ACTION_STATUS_TRADE);
		target = attachment->target;
		assert(target->action_status == AP_CHARACTER_ACTION_STATUS_TRADE);
		targetattachment = ap_private_trade_get_character_attachment(target);
		assert(targetattachment->target == character);
		attachment->awaiting_confirmation = FALSE;
		targetattachment->awaiting_confirmation = FALSE;
		ap_private_trade_make_packet(mod->ap_private_trade, 
			AP_PRIVATE_TRADE_PACKET_START, 
			&character->id, &target->id, NULL);
		as_player_send_packet(mod->as_player, character);
		ap_private_trade_make_packet(mod->ap_private_trade, 
			AP_PRIVATE_TRADE_PACKET_START, 
			&target->id, &character->id, NULL);
		as_player_send_packet(mod->as_player, target);
		establishgrid(mod, character, attachment, target);
		establishgrid(mod, target, targetattachment, character);
		break;
	}
	case AP_PRIVATE_TRADE_PACKET_CANCEL:
		ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
		break;
	case AP_PRIVATE_TRADE_PACKET_ADD_TO_TRADE_GRID: {
		struct ap_character * target;
		struct ap_item * item;
		if (!attachment->target) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		if (attachment->awaiting_confirmation) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		if (attachment->locked)
			break;
		target = attachment->target;
		assert(character->action_status == AP_CHARACTER_ACTION_STATUS_TRADE);
		assert(target->action_status == AP_CHARACTER_ACTION_STATUS_TRADE);
		if (cb->arg == 0 || cb->arg == 1) {
			uint16_t layer = UINT16_MAX;
			uint16_t row = UINT16_MAX;
			uint16_t column = UINT16_MAX;
			struct ap_grid * sourcegrid;
			struct ap_grid * grid;
			item = ap_item_find(mod->ap_item, character, cb->item_id, 2,
				AP_ITEM_STATUS_INVENTORY, AP_ITEM_STATUS_CASH_INVENTORY);
			if (!item)
				break;
			grid = ap_item_get_character_grid(mod->ap_item, 
				character, AP_ITEM_STATUS_TRADE_GRID);
			if (!ap_grid_get_empty(grid, &layer, &row, &column))
				break;
			if (item->status_flags & AP_ITEM_STATUS_FLAG_CANNOT_BE_SOLD)
				break;
			if (item->status_flags & AP_ITEM_STATUS_FLAG_CANNOT_BE_TRADED)
				break;
			if (item->temp->is_event_item || item->expire_time || item->in_use)
				break;
			if (!as_item_get(mod->as_item, item)->db)
				break;
			if (item->status_flags & AP_ITEM_STATUS_FLAG_BIND_ON_OWNER) {
				/* Cannot trade bound items without optional item (Marvel Scroll). */
				if (!attachment->optional_item_id)
					break;
				/* Cannot trade more than one bound item at a time. */
				if (attachment->bound_item_count)
					break;
				attachment->bound_item_count++;
			}
			if (AP_ITEM_IS_CASH_ITEM(item))
				attachment->cash_item_count++;
			else
				attachment->inventory_item_count++;
			sourcegrid = ap_item_get_character_grid(mod->ap_item, 
				character, item->status);
			ap_grid_set_empty(sourcegrid, item->grid_pos[AP_ITEM_GRID_POS_TAB],
				item->grid_pos[AP_ITEM_GRID_POS_ROW], item->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
			ap_grid_add_item(grid, layer, row, column, AP_GRID_ITEM_TYPE_ITEM, 
				item->id, item->tid, item);
			ap_item_set_status(item, AP_ITEM_STATUS_TRADE_GRID);
			ap_item_set_grid_pos(item, layer, row, column);
			ap_item_make_update_packet(mod->ap_item, item, 
				AP_ITEM_UPDATE_STATUS | AP_ITEM_UPDATE_GRID_POS);
			as_player_send_packet(mod->as_player, character);
			ap_item_make_add_packet(mod->ap_item, item);
			as_player_send_packet(mod->as_player, target);
			if (item->temp->type == AP_ITEM_TYPE_EQUIP) {
				ap_item_convert_make_add_packet(mod->ap_item_convert, item);
				as_player_send_packet(mod->as_player, target);
			}
		}
		else if (cb->arg == 2) {
			enum ap_item_status status;
			uint16_t gridpos[AP_ITEM_GRID_POS_COUNT];
			if (attachment->optional_item_id)
				break;
			item = ap_item_find(mod->ap_item, character, cb->item_id, 1,
				AP_ITEM_STATUS_CASH_INVENTORY);
			if (!item)
				break;
			if (item->temp->type != AP_ITEM_TYPE_USABLE)
				break;
			if (item->temp->usable.usable_type != AP_ITEM_USABLE_TYPE_PRIVATE_TRADE_OPTION)
				break;
			attachment->optional_item_id = item->id;
			ap_item_make_remove_packet(mod->ap_item, item);
			as_player_send_packet(mod->as_player, character);
			status = item->status;
			memcpy(gridpos, item->grid_pos, sizeof(gridpos));
			item->status = AP_ITEM_STATUS_TRADE_OPTION_GRID;
			item->grid_pos[AP_ITEM_GRID_POS_TAB] = 1;
			item->grid_pos[AP_ITEM_GRID_POS_ROW] = 1;
			item->grid_pos[AP_ITEM_GRID_POS_COLUMN] = 1;
			ap_item_make_add_packet(mod->ap_item, item);
			as_player_send_packet(mod->as_player, character);
			/* Restore item status and position. */
			item->status = status;
			item->grid_pos[AP_ITEM_GRID_POS_TAB] = gridpos[AP_ITEM_GRID_POS_TAB];
			item->grid_pos[AP_ITEM_GRID_POS_ROW] = gridpos[AP_ITEM_GRID_POS_ROW];
			item->grid_pos[AP_ITEM_GRID_POS_COLUMN] = gridpos[AP_ITEM_GRID_POS_COLUMN];
		}
		break;
	}
	case AP_PRIVATE_TRADE_PACKET_REMOVE_FROM_TRADE_GRID: {
		struct ap_character * target;
		struct ap_item * item;
		uint16_t layer = UINT16_MAX;
		uint16_t row = UINT16_MAX;
		uint16_t column = UINT16_MAX;
		enum ap_item_status targetstatus;
		struct ap_grid * targetgrid;
		struct ap_grid * grid;
		if (!attachment->target) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		if (attachment->awaiting_confirmation) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		if (attachment->locked)
			break;
		target = attachment->target;
		assert(character->action_status == AP_CHARACTER_ACTION_STATUS_TRADE);
		assert(target->action_status == AP_CHARACTER_ACTION_STATUS_TRADE);
		item = ap_item_find(mod->ap_item, character, cb->item_id, 1,
			AP_ITEM_STATUS_TRADE_GRID);
		if (!item)
			break;
		if (AP_ITEM_IS_CASH_ITEM(item))
			targetstatus = AP_ITEM_STATUS_CASH_INVENTORY;
		else
			targetstatus = AP_ITEM_STATUS_INVENTORY;
		targetgrid = ap_item_get_character_grid(mod->ap_item, character, targetstatus);
		if (!ap_grid_get_empty(targetgrid, &layer, &row, &column))
			break;
		if (item->status_flags & AP_ITEM_STATUS_FLAG_BIND_ON_OWNER)
			attachment->bound_item_count--;
		if (targetstatus == AP_ITEM_STATUS_CASH_INVENTORY)
			attachment->cash_item_count--;
		else
			attachment->inventory_item_count--;
		grid = ap_item_get_character_grid(mod->ap_item, character, 
			AP_ITEM_STATUS_TRADE_GRID);
		ap_grid_set_empty(grid, item->grid_pos[AP_ITEM_GRID_POS_TAB],
			item->grid_pos[AP_ITEM_GRID_POS_ROW], item->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
		ap_grid_add_item(targetgrid, layer, row, column, AP_GRID_ITEM_TYPE_ITEM, 
			item->id, item->tid, item);
		ap_item_set_status(item, targetstatus);
		ap_item_set_grid_pos(item, layer, row, column);
		ap_item_make_update_packet(mod->ap_item, item, 
			AP_ITEM_UPDATE_STATUS | AP_ITEM_UPDATE_GRID_POS);
		as_player_send_packet(mod->as_player, character);
		ap_item_make_remove_packet(mod->ap_item, item);
		as_player_send_packet(mod->as_player, target);
		break;
	}
	case AP_PRIVATE_TRADE_PACKET_LOCK: {
		struct ap_character * target;
		struct ap_private_trade_character_attachment * targetattachment;
		if (!attachment->target) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		if (attachment->awaiting_confirmation) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		if (attachment->locked)
			break;
		target = attachment->target;
		assert(character->action_status == AP_CHARACTER_ACTION_STATUS_TRADE);
		assert(target->action_status == AP_CHARACTER_ACTION_STATUS_TRADE);
		attachment->locked = TRUE;
		targetattachment = ap_private_trade_get_character_attachment(target);
		ap_private_trade_make_packet(mod->ap_private_trade, 
			AP_PRIVATE_TRADE_PACKET_LOCK, NULL, NULL, NULL);
		as_player_send_packet(mod->as_player, character);
		ap_private_trade_make_packet(mod->ap_private_trade, 
			AP_PRIVATE_TRADE_PACKET_TARGET_LOCKED, NULL, NULL, NULL);
		as_player_send_packet(mod->as_player, target);
		if (targetattachment->locked) {
			/* Both sides are locked, ready to trade. */
			ap_private_trade_make_packet(mod->ap_private_trade, 
				AP_PRIVATE_TRADE_PACKET_ACTIVE_READY_TO_EXCHANGE, NULL, NULL, NULL);
			as_player_send_packet(mod->as_player, character);
			as_player_send_packet(mod->as_player, target);
		}
		break;
	}
	case AP_PRIVATE_TRADE_PACKET_UNLOCK: {
		struct ap_character * target;
		struct ap_private_trade_character_attachment * targetattachment;
		if (!attachment->target) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		if (attachment->awaiting_confirmation) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		if (!attachment->locked || attachment->ready)
			break;
		target = attachment->target;
		assert(character->action_status == AP_CHARACTER_ACTION_STATUS_TRADE);
		assert(target->action_status == AP_CHARACTER_ACTION_STATUS_TRADE);
		targetattachment = ap_private_trade_get_character_attachment(target);
		if (targetattachment->ready)
			break;
		attachment->locked = FALSE;
		ap_private_trade_make_packet(mod->ap_private_trade, 
			AP_PRIVATE_TRADE_PACKET_UNLOCK, NULL, NULL, NULL);
		as_player_send_packet(mod->as_player, character);
		ap_private_trade_make_packet(mod->ap_private_trade, 
			AP_PRIVATE_TRADE_PACKET_TARGET_UNLOCKED, NULL, NULL, NULL);
		as_player_send_packet(mod->as_player, target);
		break;
	}
	case AP_PRIVATE_TRADE_PACKET_READY_TO_EXCHANGE: {
		struct ap_character * target;
		struct ap_private_trade_character_attachment * targetattachment;
		struct ap_character * characters[2] = { 0 };
		struct ap_private_trade_character_attachment * attachments[2] = { 0 };
		struct as_account * accounts[2] = { 0 };
		struct as_item_character_db * itemdbs[2] = { 0 };
		uint32_t i;
		if (!attachment->target) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		if (attachment->awaiting_confirmation) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		if (!attachment->locked || attachment->ready)
			break;
		target = attachment->target;
		assert(character->action_status == AP_CHARACTER_ACTION_STATUS_TRADE);
		assert(target->action_status == AP_CHARACTER_ACTION_STATUS_TRADE);
		targetattachment = ap_private_trade_get_character_attachment(target);
		if (!targetattachment->locked)
			break;
		attachment->ready = TRUE;
		ap_private_trade_make_packet(mod->ap_private_trade, 
			AP_PRIVATE_TRADE_PACKET_READY_TO_EXCHANGE, NULL, NULL, NULL);
		as_player_send_packet(mod->as_player, character);
		ap_private_trade_make_packet(mod->ap_private_trade, 
			AP_PRIVATE_TRADE_PACKET_TARGET_READY_TO_EXCHANGE, NULL, NULL, NULL);
		as_player_send_packet(mod->as_player, target);
		if (!targetattachment->ready)
			break;
		characters[0] = character;
		characters[1] = target;
		attachments[0] = attachment;
		attachments[1] = targetattachment;
		accounts[0] = as_player_get_character_ad(mod->as_player, character)->account;
		accounts[1] = as_player_get_character_ad(mod->as_player, target)->account;
		assert(accounts[0] != NULL);
		assert(accounts[1] != NULL);
		itemdbs[0] = as_item_get_character_db(mod->as_item, 
			as_character_get(mod->as_character, character)->db);
		itemdbs[1] = as_item_get_character_db(mod->as_item, 
			as_character_get(mod->as_character, target)->db);
		for (i = 0; i < 2; i++) {
			uint32_t self = i;
			uint32_t peer = (i + 1) & 1;
			uint32_t emptycount;
			/* Confirm that gold amount is valid. */
			if (characters[self]->inventory_gold < attachments[self]->gold_amount ||
				characters[self]->inventory_gold + attachments[peer]->gold_amount > AP_CHARACTER_MAX_INVENTORY_GOLD) {
				ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
				return TRUE;
			}
			/* Confirm that chantra coin amount is valid. */
			if (accounts[self]->chantra_coins < attachments[self]->chantra_coin_amount) {
				ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
				return TRUE;
			}
			/* Confirm that there is enough inventory space. */
			emptycount = ap_item_get_empty_slot_count(mod->ap_item, 
				characters[self], AP_ITEM_STATUS_INVENTORY);
			if (attachments[peer]->inventory_item_count > emptycount) {
				ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
				return TRUE;
			}
			/* Confirm that there is enough cash inventory space. */
			emptycount = ap_item_get_empty_slot_count(mod->ap_item, 
				characters[self], AP_ITEM_STATUS_CASH_INVENTORY);
			if (attachments[peer]->cash_item_count > emptycount) {
				ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
				return TRUE;
			}
			/* Confirm that there is enough space for new items in database. */
			emptycount = COUNT_OF(itemdbs[self]->items) - itemdbs[self]->item_count;
			if (emptycount < attachments[peer]->inventory_item_count + attachments[peer]->cash_item_count) {
				ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
				return TRUE;
			}
		}
		for (i = 0; i < 2; i++) {
			uint32_t self = i;
			uint32_t peer = (i + 1) & 1;
			struct ap_grid * grid = ap_item_get_character_grid(mod->ap_item,
				characters[peer], AP_ITEM_STATUS_TRADE_GRID);
			uint32_t j;
			characters[self]->inventory_gold -= attachments[self]->gold_amount;
			characters[self]->inventory_gold += attachments[peer]->gold_amount;
			accounts[self]->chantra_coins -= attachments[self]->chantra_coin_amount;
			accounts[self]->chantra_coins += attachments[peer]->chantra_coin_amount;
			accounts[self]->commit_linked = TRUE;
			characters[self]->chantra_coins = accounts[self]->chantra_coins;
			ap_bill_info_make_cash_info_packet(mod->ap_bill_info, 
				characters[self]->id, 0, accounts[self]->chantra_coins);
			as_player_send_packet(mod->as_player, characters[self]);
			for (j = 0; j < grid->grid_count; j++) {
				struct ap_item * item = ap_grid_get_object_by_index(grid, j);
				if (item) {
					transferitem(mod, characters[peer], itemdbs[peer],
						characters[self], itemdbs[self], item, grid);
				}
			}
			if (attachments[self]->optional_item_id) {
				struct ap_item * item = ap_item_find(mod->ap_item, characters[self],
					attachments[self]->optional_item_id, 1, AP_ITEM_STATUS_CASH_INVENTORY);
				if (item) {
					if (item->stack_count > 1) {
						ap_item_make_remove_packet(mod->ap_item, item);
						as_player_send_packet(mod->as_player, characters[self]);
						ap_item_make_add_packet(mod->ap_item, item);
						as_player_send_packet(mod->as_player, characters[self]);
					}
					as_item_consume(mod->as_item, characters[self], item, 1);
				}
			}
			ap_private_trade_make_result_packet(mod->ap_private_trade, 
				characters[self]->id);
			as_player_send_packet(mod->as_player, characters[self]);
			characters[self]->action_status = AP_CHARACTER_ACTION_STATUS_NORMAL;
			ap_character_update(mod->ap_character, characters[self], 
				AP_CHARACTER_BIT_ACTION_STATUS | AP_CHARACTER_BIT_INVENTORY_GOLD |
				AP_CHARACTER_BIT_CHANTRA_COINS, FALSE);
		}
		ap_private_trade_clear(attachment);
		ap_private_trade_clear(targetattachment);
		break;
	}
	case AP_PRIVATE_TRADE_PACKET_UPDATE_MONEY: {
		struct ap_character * target;
		if (!attachment->target) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		if (attachment->awaiting_confirmation) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		if (attachment->locked)
			break;
		if (character->inventory_gold < cb->currency_amount)
			cb->currency_amount = character->inventory_gold;
		target = attachment->target;
		assert(character->action_status == AP_CHARACTER_ACTION_STATUS_TRADE);
		assert(target->action_status == AP_CHARACTER_ACTION_STATUS_TRADE);
		attachment->gold_amount = cb->currency_amount;
		ap_private_trade_make_packet(mod->ap_private_trade, 
			AP_PRIVATE_TRADE_PACKET_UPDATE_MONEY, 
			NULL, NULL, &cb->currency_amount);
		as_player_send_packet(mod->as_player, character);
		ap_private_trade_make_packet(mod->ap_private_trade, 
			AP_PRIVATE_TRADE_PACKET_TARGET_UPDATE_MONEY, 
			NULL, NULL, &cb->currency_amount);
		as_player_send_packet(mod->as_player, target);
		break;
	}
	case AP_PRIVATE_TRADE_PACKET_UPDATE_SELF_CC: {
		struct ap_character * target;
		struct as_account * account;
		if (!attachment->target) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		if (attachment->awaiting_confirmation) {
			ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
			break;
		}
		if (attachment->locked)
			break;
		account = as_player_get_character_ad(mod->as_player, character)->account;
		assert(account != NULL);
		if (account->chantra_coins < cb->currency_amount)
			cb->currency_amount = account->chantra_coins;
		target = attachment->target;
		assert(character->action_status == AP_CHARACTER_ACTION_STATUS_TRADE);
		assert(target->action_status == AP_CHARACTER_ACTION_STATUS_TRADE);
		attachment->chantra_coin_amount = cb->currency_amount;
		ap_private_trade_make_packet(mod->ap_private_trade, 
			AP_PRIVATE_TRADE_PACKET_UPDATE_SELF_CC, 
			NULL, NULL, &cb->currency_amount);
		as_player_send_packet(mod->as_player, character);
		ap_private_trade_make_packet(mod->ap_private_trade, 
			AP_PRIVATE_TRADE_PACKET_UPDATE_PEER_CC, 
			NULL, NULL, &cb->currency_amount);
		as_player_send_packet(mod->as_player, target);
		break;
	}
	case AP_PRIVATE_TRADE_PACKET_TRADE_REFUSE:
		ap_private_trade_cancel(mod->ap_private_trade, character, TRUE);
		break;
	}
	return TRUE;
}

static boolean cbtradecancel(
	struct as_private_trade_process_module * mod,
	struct ap_private_trade_cb_cancel * cb)
{
	struct ap_character * character = cb->character;
	struct ap_private_trade_character_attachment * attachment = cb->attachment;
	struct ap_grid * grid;
	uint32_t i;
	struct ap_character * target;
	if (!attachment->target) {
		ap_private_trade_make_packet(mod->ap_private_trade, 
			AP_PRIVATE_TRADE_PACKET_CANCEL, 
			&character->id, NULL, NULL);
		as_player_send_packet(mod->as_player, character);
		return TRUE;
	}
	target = attachment->target;
	assert(target != NULL);
	if (cb->recursive)
		ap_private_trade_cancel(mod->ap_private_trade, target, FALSE);
	grid = ap_item_get_character_grid(mod->ap_item, character, 
		AP_ITEM_STATUS_TRADE_GRID);
	for (i = 0; i < grid->grid_count; i++) {
		struct ap_item * item = ap_grid_get_object_by_index(grid, i);
		uint16_t layer = UINT16_MAX;
		uint16_t row = UINT16_MAX;
		uint16_t column = UINT16_MAX;
		enum ap_item_status targetstatus;
		struct ap_grid * targetgrid;
		if (!item)
			continue;
		ap_item_make_remove_packet(mod->ap_item, item);
		as_player_send_packet(mod->as_player, target);
		if (AP_ITEM_IS_CASH_ITEM(item))
			targetstatus = AP_ITEM_STATUS_CASH_INVENTORY;
		else
			targetstatus = AP_ITEM_STATUS_INVENTORY;
		targetgrid = ap_item_get_character_grid(mod->ap_item, character, targetstatus);
		if (!ap_grid_get_empty(targetgrid, &layer, &row, &column)) {
			as_player_send_packet(mod->as_player, character);
			continue;
		}
		ap_grid_set_empty(grid, item->grid_pos[AP_ITEM_GRID_POS_TAB],
			item->grid_pos[AP_ITEM_GRID_POS_ROW], item->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
		ap_grid_add_item(targetgrid, layer, row, column, AP_GRID_ITEM_TYPE_ITEM, 
			item->id, item->tid, item);
		ap_item_set_status(item, targetstatus);
		ap_item_set_grid_pos(item, layer, row, column);
		ap_item_make_update_packet(mod->ap_item, item, 
			AP_ITEM_UPDATE_STATUS | AP_ITEM_UPDATE_GRID_POS);
		as_player_send_packet(mod->as_player, character);
	}
	if (attachment->optional_item_id) {
		struct ap_item * item = ap_item_find(mod->ap_item, character, 
			attachment->optional_item_id, 1, AP_ITEM_STATUS_CASH_INVENTORY);
		if (item) {
			ap_item_make_remove_packet(mod->ap_item, item);
			as_player_send_packet(mod->as_player, character);
			ap_item_make_add_packet(mod->ap_item, item);
			as_player_send_packet(mod->as_player, character);
		}
	}
	ap_private_trade_clear(attachment);
	ap_private_trade_make_packet(mod->ap_private_trade, 
		AP_PRIVATE_TRADE_PACKET_CANCEL, 
		&character->id, NULL, NULL);
	as_player_send_packet(mod->as_player, character);
	if (character->action_status == AP_CHARACTER_ACTION_STATUS_TRADE) {
		character->action_status = AP_CHARACTER_ACTION_STATUS_NORMAL;
		ap_character_update(mod->ap_character, character, 
			AP_CHARACTER_BIT_ACTION_STATUS, FALSE);
	}
	return TRUE;
}

static boolean onregister(
	struct as_private_trade_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_bill_info, AP_BILL_INFO_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_chat, AP_CHAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item_convert, AP_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_private_trade, AP_PRIVATE_TRADE_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_item, AS_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_SET_ACTION_STATUS, mod, cbcharsetactionstatus);
	as_player_add_callback(mod->as_player, AS_PLAYER_CB_REMOVE, mod, cbplayerremove);
	ap_private_trade_add_callback(mod->ap_private_trade, AP_PRIVATE_TRADE_CB_RECEIVE, mod, cbreceive);
	ap_private_trade_add_callback(mod->ap_private_trade, AP_PRIVATE_TRADE_CB_CANCEL, mod, cbtradecancel);
	return TRUE;
}

static void onclose(struct as_private_trade_process_module * mod)
{
}

static void onshutdown(struct as_private_trade_process_module * mod)
{
}

struct as_private_trade_process_module * as_private_trade_process_create_module()
{
	struct as_private_trade_process_module * mod = ap_module_instance_new(AS_PRIVATE_TRADE_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	return mod;
}
