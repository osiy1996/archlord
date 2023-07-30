#include "server/as_ride_process.h"

#include "core/log.h"

#include "public/ap_item.h"
#include "public/ap_module_instance.h"
#include "public/ap_random.h"
#include "public/ap_ride.h"

#include "server/as_map.h"
#include "server/as_player.h"

#include <assert.h>

struct as_ride_process_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_item_module * ap_item;
	struct ap_ride_module * ap_ride;
	struct as_map_module * as_map;
	struct as_player_module * as_player;
};

static boolean cbitemequip(
	struct as_ride_process_module * mod,
	struct ap_item_cb_equip * cb)
{
	if (!CHECK_BIT(cb->item->equip_flags, AP_ITEM_EQUIP_WITH_NO_STATS) &&
		!CHECK_BIT(cb->item->equip_flags, AP_ITEM_EQUIP_SILENT) &&
		cb->item->temp->equip.part == AP_ITEM_PART_RIDE) {
		ap_ride_make_packet(mod->ap_ride, AP_RIDE_PACKET_RIDE_ACK, 
			cb->character->id, cb->item->id);
		as_map_broadcast(mod->as_map, cb->character);
	}
	return TRUE;
}

static boolean cbitemunequip(
	struct as_ride_process_module * mod,
	struct ap_item_cb_unequip * cb)
{
	if (!CHECK_BIT(cb->item->equip_flags, AP_ITEM_EQUIP_WITH_NO_STATS) &&
		cb->item->temp->equip.part == AP_ITEM_PART_RIDE) {
		ap_ride_make_packet(mod->ap_ride, AP_RIDE_PACKET_DISMOUNT_ACK, 
			cb->character->id, cb->item->id);
		as_map_broadcast(mod->as_map, cb->character);
	}
	return TRUE;
}

static boolean cbreceive(
	struct as_ride_process_module * mod,
	struct ap_ride_cb_receive * cb)
{
	struct ap_character * character = cb->user_data;
	switch (cb->type) {
	}
	return TRUE;
}

static boolean onregister(
	struct as_ride_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_ride, AP_RIDE_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	ap_item_add_callback(mod->ap_item, AP_ITEM_CB_EQUIP, mod, cbitemequip);
	ap_item_add_callback(mod->ap_item, AP_ITEM_CB_UNEQUIP, mod, cbitemunequip);
	ap_ride_add_callback(mod->ap_ride, AP_RIDE_CB_RECEIVE, mod, cbreceive);
	return TRUE;
}

static void onclose(struct as_ride_process_module * mod)
{
}

static void onshutdown(struct as_ride_process_module * mod)
{
}

struct as_ride_process_module * as_ride_process_create_module()
{
	struct as_ride_process_module * mod = ap_module_instance_new(AS_RIDE_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	return mod;
}
