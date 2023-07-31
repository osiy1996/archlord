#include "server/as_cash_mall_process.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_bill_info.h"
#include "public/ap_cash_mall.h"
#include "public/ap_chat.h"
#include "public/ap_module.h"

#include "server/as_drop_item.h"
#include "server/as_map.h"
#include "server/as_player.h"

struct as_cash_mall_process_module {
	struct ap_module_instance instance;
	struct ap_bill_info_module * ap_bill_info;
	struct ap_cash_mall_module * ap_cash_mall;
	struct ap_character_module * ap_character;
	struct ap_chat_module * ap_chat;
	struct as_drop_item_module * as_drop_item;
	struct as_player_module * as_player;
};

static boolean cbreceive(
	struct as_cash_mall_process_module * mod,
	struct ap_cash_mall_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	switch (cb->type) {
	case AP_CASH_MALL_PACKET_REQUEST_MALL_PRODUCT_LIST: {
		struct ap_cash_mall_tab * tab = 
			ap_cash_mall_get_tab(mod->ap_cash_mall, cb->tab_id);
		if (!tab)
			break;
		ap_cash_mall_make_mall_list_packet(mod->ap_cash_mall);
		as_player_send_packet(mod->as_player, c);
		ap_cash_mall_make_product_list_packet(mod->ap_cash_mall, tab);
		as_player_send_packet(mod->as_player, c);
		break;
	}
	case AP_CASH_MALL_PACKET_REQUEST_BUY_ITEM: {
		uint32_t tabcount;
		struct ap_cash_mall_tab * tabs = 
			ap_cash_mall_get_tab_list(mod->ap_cash_mall, &tabcount);
		uint32_t i;
		struct ap_cash_mall_item * mallitem = NULL;
		struct as_account * account;
		for (i = 0; i < tabcount; i++) {
			struct ap_cash_mall_tab * tab = &tabs[i];
			uint32_t j;
			uint32_t count = vec_count(tab->items);
			for (j = 0; j < count; j++) {
				if (tab->items[j].id == cb->product_id) {
					mallitem = &tab->items[j];
					break;
				}
			}
		}
		if (!mallitem) {
			ap_cash_mall_make_buy_result_packet(mod->ap_cash_mall, 
				AP_CASH_MALL_BUY_RESULT_NEED_NEW_ITEM_LIST);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		account = as_player_get_character_ad(mod->as_player, c)->account;
		if (account->chantra_coins < mallitem->price) {
			ap_cash_mall_make_buy_result_packet(mod->ap_cash_mall, 
				AP_CASH_MALL_BUY_RESULT_NOT_ENOUGH_CASH);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		if (!as_drop_item_distribute(mod->as_drop_item, c, mallitem->tid, 
				mallitem->stack_count)) {
			ap_cash_mall_make_buy_result_packet(mod->ap_cash_mall, 
				AP_CASH_MALL_BUY_RESULT_INVENTORY_FULL);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		account->chantra_coins -= mallitem->price;
		c->chantra_coins = account->chantra_coins;
		ap_cash_mall_make_buy_result_packet(mod->ap_cash_mall, 
			AP_CASH_MALL_BUY_RESULT_SUCCESS);
		as_player_send_packet(mod->as_player, c);
		ap_bill_info_make_cash_info_packet(mod->ap_bill_info, c->id, 0, 
			as_player_get_character_ad(mod->as_player, c)->account->chantra_coins);
		as_player_send_packet(mod->as_player, c);
		ap_character_update(mod->ap_character, c, AP_CHARACTER_BIT_CHANTRA_COINS, TRUE);
		break;
	}
	case AP_CASH_MALL_PACKET_REFRESH_CASH:
		ap_bill_info_make_cash_info_packet(mod->ap_bill_info, c->id, 0, 
			as_player_get_character_ad(mod->as_player, c)->account->chantra_coins);
		as_player_send_packet(mod->as_player, c);
		break;
	}
	return TRUE;
}

static void cbchataddcc(
	struct as_cash_mall_process_module * mod,
	struct ap_character * c,
	uint32_t argc,
	const char * const * argv)
{
	struct as_account * account = NULL;
	struct ap_character * target = NULL;
	int32_t amount = 0;
	switch (argc) {
	case 1:
		target = c;
		amount = strtoul(argv[0], NULL, 10);
		break;
	case 2:
		target = as_player_get_by_name(mod->as_player, argv[0]);
		if (!target)
			return;
		amount = strtoul(argv[1], NULL, 10);
		break;
	default:
		return;
	}
	account = as_player_get_character_ad(mod->as_player, target)->account;
	assert(account != NULL);
	if (amount < 0) {
		if ((uint64_t)-amount > account->chantra_coins)
			return;
	}
	else if ((uint64_t)amount >= UINT64_MAX - account->chantra_coins) {
		return;
	}
	account->chantra_coins += amount;
	ap_bill_info_make_cash_info_packet(mod->ap_bill_info, target->id, 0, 
		account->chantra_coins);
	as_player_send_packet(mod->as_player, target);
	ap_character_update(mod->ap_character, c, AP_CHARACTER_BIT_CHANTRA_COINS, TRUE);
}

static boolean onregister(
	struct as_cash_mall_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_bill_info, AP_BILL_INFO_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_cash_mall, AP_CASH_MALL_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_chat, AP_CHAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_drop_item, AS_DROP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	ap_cash_mall_add_callback(mod->ap_cash_mall, AP_CASH_MALL_CB_RECEIVE, mod, cbreceive);
	ap_chat_add_command(mod->ap_chat, "/addcc", mod, cbchataddcc);
	return TRUE;
}

static void onshutdown(struct as_cash_mall_process_module * mod)
{
}

struct as_cash_mall_process_module * as_cash_mall_process_create_module()
{
	struct as_cash_mall_process_module * mod = ap_module_instance_new(AS_CASH_MALL_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	return mod;
}
