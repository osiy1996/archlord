#include "server/as_event_npc_trade_process.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/vector.h"

#include "public/ap_event_npc_trade.h"

#include "server/as_drop_item.h"
#include "server/as_item.h"
#include "server/as_map.h"
#include "server/as_player.h"

struct as_event_npc_trade_process_module {
	struct ap_module_instance instance;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_event_npc_trade_module * ap_event_npc_trade;
	struct ap_item_module * ap_item;
	struct as_character_module * as_character;
	struct as_drop_item_module * as_drop_item;
	struct as_item_module * as_item;
	struct as_map_module * as_map;
	struct as_player_module * as_player;
};

static struct ap_character * findnpc(
	struct as_event_npc_trade_process_module * mod,
	struct ap_event_manager_base_packet * packet)
{
	if (!packet || packet->source_type != AP_BASE_TYPE_CHARACTER)
		return NULL;
	return as_map_get_npc(mod->as_map, packet->source_id);
}

static struct ap_event_manager_event * findevent(
	struct as_event_npc_trade_process_module * mod,
	struct ap_character * npc)
{
	return ap_event_manager_get_function(mod->ap_event_manager, npc, 
		AP_EVENT_MANAGER_FUNCTION_NPCTRADE);
}

static boolean cbreceive(
	struct as_event_npc_trade_process_module * mod,
	struct ap_event_npc_trade_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	struct ap_character * npc = NULL;
	switch (cb->type) {
	case AP_EVENT_NPC_TRADE_PACKET_REQUEST_ITEM_LIST: {
		struct ap_character * npc = findnpc(mod, cb->event);
		struct ap_event_manager_event * e;
		const struct ap_event_npc_trade_data * tradedata;
		uint32_t i;
		float distance;
		if (!npc)
			break;
		e = findevent(mod, npc);
		if (!e)
			break;
		distance = au_distance2d(&npc->pos, &c->pos);
		if (distance > AP_EVENT_NPC_TRADE_MAX_USE_RANGE)
			break;
		if (c->factor.char_status.murderer >= AP_CHARACTER_MURDERER_LEVEL2_POINT) {
			ap_event_npc_trade_make_buy_result_packet(mod->ap_event_npc_trade,
				c->id, AP_EVENT_NPC_TRADE_RESULT_NO_SELL_TO_MURDERER);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		tradedata = ap_event_npc_trade_get_data(e);
		for (i = 0; i < tradedata->item_grid->grid_count; i++) {
			struct ap_item * item = 
				ap_grid_get_object_by_index(tradedata->item_grid, i);
			if (item) {
				ap_item_make_add_packet(mod->ap_item, item);
				as_player_send_packet(mod->as_player, c);
				ap_event_npc_trade_make_response_item_list_packet(
					mod->ap_event_npc_trade, e, item);
				as_player_send_packet(mod->as_player, c);
			}
		}
		ap_event_npc_trade_make_send_all_item_info_packet(mod->ap_event_npc_trade, c->id);
		as_player_send_packet(mod->as_player, c);
		break;
	}
	case AP_EVENT_NPC_TRADE_PACKET_BUY_ITEM: {
		struct ap_character * npc = findnpc(mod, cb->event);
		struct ap_event_manager_event * e;
		const struct ap_event_npc_trade_data * tradedata;
		struct ap_item * item;
		float distance;
		uint64_t price;
		if (!npc)
			break;
		e = findevent(mod, npc);
		if (!e)
			break;
		distance = au_distance2d(&npc->pos, &c->pos);
		if (distance > AP_EVENT_NPC_TRADE_MAX_USE_RANGE)
			break;
		if (c->factor.char_status.murderer >= AP_CHARACTER_MURDERER_LEVEL2_POINT) {
			ap_event_npc_trade_make_buy_result_packet(mod->ap_event_npc_trade,
				c->id, AP_EVENT_NPC_TRADE_RESULT_NO_SELL_TO_MURDERER);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		tradedata = ap_event_npc_trade_get_data(e);
		item = ap_grid_get_object_by_id(tradedata->item_grid, cb->item_id);
		if (!item) {
			ap_event_npc_trade_make_buy_result_packet(mod->ap_event_npc_trade,
				c->id, AP_EVENT_NPC_TRADE_RESULT_INVALID_ITEMID);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		price = item->factor.price.npc_price * item->stack_count;
		if (!ap_character_has_gold(c, price)) {
			ap_event_npc_trade_make_buy_result_packet(mod->ap_event_npc_trade,
				c->id, AP_EVENT_NPC_TRADE_RESULT_BUY_NOT_ENOUGH_MONEY);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		if (!as_drop_item_distribute(mod->as_drop_item, c, item->tid, item->stack_count)) {
			ap_event_npc_trade_make_buy_result_packet(mod->ap_event_npc_trade,
				c->id, AP_EVENT_NPC_TRADE_RESULT_BUY_FULL_INVENTORY);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		as_character_consume_gold(mod->as_character, c, price);
		ap_event_npc_trade_make_buy_result_packet(mod->ap_event_npc_trade,
			c->id, AP_EVENT_NPC_TRADE_RESULT_BUY_SUCCEEDED);
		as_player_send_packet(mod->as_player, c);
		break;
	}
	case AP_EVENT_NPC_TRADE_PACKET_SELL_ITEM: {
		struct ap_character * npc = findnpc(mod, cb->event);
		struct ap_event_manager_event * e;
		struct ap_item * item;
		float distance;
		uint64_t price;
		if (!npc)
			break;
		e = findevent(mod, npc);
		if (!e)
			break;
		distance = au_distance2d(&npc->pos, &c->pos);
		if (distance > AP_EVENT_NPC_TRADE_MAX_USE_RANGE)
			break;
		if (c->factor.char_status.murderer >= AP_CHARACTER_MURDERER_LEVEL2_POINT) {
			ap_event_npc_trade_make_sell_result_packet(mod->ap_event_npc_trade,
				c->id, AP_EVENT_NPC_TRADE_RESULT_NO_SELL_TO_MURDERER);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		item = ap_item_find(mod->ap_item, c, cb->item_id, 2, 
			AP_ITEM_STATUS_INVENTORY, AP_ITEM_STATUS_SUB_INVENTORY);
		if (!item) {
			ap_event_npc_trade_make_sell_result_packet(mod->ap_event_npc_trade,
				c->id, AP_EVENT_NPC_TRADE_RESULT_INVALID_ITEMID);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		if (item->status_flags & AP_ITEM_STATUS_FLAG_CANNOT_BE_SOLD) {
			ap_event_npc_trade_make_sell_result_packet(mod->ap_event_npc_trade,
				c->id, AP_EVENT_NPC_TRADE_RESULT_INVALID_ITEMID);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		price = item->temp->factor.price.pc_price * item->stack_count;
		if (!as_character_deposit_gold(mod->as_character, c, price)) {
			ap_event_npc_trade_make_sell_result_packet(mod->ap_event_npc_trade,
				c->id, AP_EVENT_NPC_TRADE_RESULT_SELL_EXCEED_MONEY);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		as_item_delete(mod->as_item, c, item);
		ap_event_npc_trade_make_sell_result_packet(mod->ap_event_npc_trade,
			c->id, AP_EVENT_NPC_TRADE_RESULT_SELL_SUCCEEDED);
		as_player_send_packet(mod->as_player, c);
		break;
	}
	}
	return TRUE;
}

static boolean onregister(
	struct as_event_npc_trade_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_npc_trade, AP_EVENT_NPC_TRADE_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_drop_item, AS_DROP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_item, AS_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	ap_event_npc_trade_add_callback(mod->ap_event_npc_trade,
		AP_EVENT_NPC_TRADE_CB_RECEIVE, mod, cbreceive);
	return TRUE;
}

static void onshutdown(struct as_event_npc_trade_process_module * mod)
{
}

struct as_event_npc_trade_process_module * as_event_npc_trade_process_create_module()
{
	struct as_event_npc_trade_process_module * mod = ap_module_instance_new(AS_EVENT_NPC_TRADE_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	return mod;
}
