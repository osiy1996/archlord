#include "server/as_auction_process.h"

#include "core/log.h"

#include "public/ap_auction.h"

#include "server/as_drop_item.h"
#include "server/as_item.h"
#include "server/as_map.h"
#include "server/as_player.h"

struct as_auction_process_module {
	struct ap_module_instance instance;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_auction_module * ap_auction;
	struct ap_item_module * ap_item;
	struct as_character_module * as_character;
	struct as_drop_item_module * as_drop_item;
	struct as_item_module * as_item;
	struct as_map_module * as_map;
	struct as_player_module * as_player;
};

static struct ap_character * findnpc(
	struct as_auction_process_module * mod,
	struct ap_event_manager_base_packet * packet)
{
	if (!packet || packet->source_type != AP_BASE_TYPE_CHARACTER)
		return NULL;
	return as_map_get_npc(mod->as_map, packet->source_id);
}

static struct ap_event_manager_event * findevent(
	struct as_auction_process_module * mod,
	struct ap_character * npc)
{
	return ap_event_manager_get_function(mod->ap_event_manager, npc, 
		AP_EVENT_MANAGER_FUNCTION_AUCTION);
}

static boolean cbreceive(
	struct as_auction_process_module * mod,
	struct ap_auction_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	struct ap_character * npc = NULL;
	switch (cb->type) {
	case AP_AUCTION_PACKET_EVENT_REQUEST: {
		struct ap_character * npc = findnpc(mod, cb->event);
		struct ap_event_manager_event * e;
		float distance;
		if (!npc)
			break;
		e = findevent(mod, npc);
		if (!e)
			break;
		distance = au_distance2d(&npc->pos, &c->pos);
		if (distance > AP_AUCTION_MAX_USE_RANGE)
			break;
		break;
	}
	}
	return TRUE;
}

static boolean onregister(
	struct as_auction_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_auction, AP_AUCTION_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_drop_item, AS_DROP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_item, AS_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	ap_auction_add_callback(mod->ap_auction, AP_AUCTION_CB_RECEIVE, mod, cbreceive);
	return TRUE;
}

static void onshutdown(struct as_auction_process_module * mod)
{
}

struct as_auction_process_module * as_auction_process_create_module()
{
	struct as_auction_process_module * mod = ap_module_instance_new(AS_AUCTION_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	return mod;
}
