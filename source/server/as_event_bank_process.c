#include "server/as_event_bank_process.h"

#include "core/log.h"

#include "public/ap_event_bank.h"

#include "server/as_map.h"
#include "server/as_player.h"

struct as_event_bank_process_module {
	struct ap_module_instance instance;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_event_bank_module * ap_event_bank;
	struct as_map_module * as_map;
	struct as_player_module * as_player;
};

static struct ap_character * findnpc(
	struct as_event_bank_process_module * mod,
	struct ap_event_manager_base_packet * packet)
{
	if (!packet || packet->source_type != AP_BASE_TYPE_CHARACTER)
		return NULL;
	return as_map_get_npc(mod->as_map, packet->source_id);
}

static struct ap_event_manager_event * findevent(
	struct as_event_bank_process_module * mod,
	struct ap_character * npc)
{
	return ap_event_manager_get_function(mod->ap_event_manager, npc, 
		AP_EVENT_MANAGER_FUNCTION_BANK);
}

static boolean cbreceive(
	struct as_event_bank_process_module * mod,
	struct ap_event_bank_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	struct ap_character * npc = NULL;
	switch (cb->type) {
	case AP_EVENT_BANK_PACKET_REQUEST: {
		struct ap_character * npc = findnpc(mod, cb->event);
		struct ap_event_manager_event * e;
		float distance;
		if (!npc)
			break;
		e = findevent(mod, npc);
		if (!e)
			break;
		distance = au_distance2d(&npc->pos, &c->pos);
		if (distance > AP_EVENT_BANK_MAX_USE_RANGE)
			break;
		ap_event_bank_make_grant_packet(mod->ap_event_bank, e, c->id);
		as_player_send_packet(mod->as_player, c);
		break;
	}
	}
	return TRUE;
}

static boolean onregister(
	struct as_event_bank_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_bank, AP_EVENT_BANK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	ap_event_bank_add_callback(mod->ap_event_bank, AP_EVENT_BANK_CB_RECEIVE, mod, cbreceive);
	return TRUE;
}

static void onshutdown(struct as_event_bank_process_module * mod)
{
}

struct as_event_bank_process_module * as_event_bank_process_create_module()
{
	struct as_event_bank_process_module * mod = ap_module_instance_new(AS_EVENT_BANK_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	return mod;
}
