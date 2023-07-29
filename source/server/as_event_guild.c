#include <assert.h>

#include "core/log.h"
#include "core/malloc.h"
#include "core/vector.h"

#include "public/ap_event_guild.h"
#include "public/ap_tick.h"

#include "server/as_event_guild.h"
#include "server/as_map.h"
#include "server/as_player.h"

struct as_event_guild_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_event_guild_module * ap_event_guild;
	struct ap_event_manager_module * ap_event_manager;
	struct as_map_module * as_map;
	struct as_player_module * as_player;
	struct ap_character ** list;
};

static boolean cbreceive(
	struct as_event_guild_module * mod,
	struct ap_event_guild_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	struct ap_character * npc = NULL;
	switch (cb->type) {
	case AP_EVENT_GUILD_PACKET_TYPE_REQUEST: {
		struct ap_event_manager_event * e;
		float distance;
		if (!cb->event || 
			cb->event->source_type != AP_BASE_TYPE_CHARACTER) {
			break;
		}
		npc = as_map_get_npc(mod->as_map, cb->event->source_id);
		if (!npc)
			break;
		e = ap_event_manager_get_function(mod->ap_event_manager, npc, 
			AP_EVENT_MANAGER_FUNCTION_GUILD);
		if (!e)
			break;
		distance = au_distance2d(&c->pos, &npc->pos);
		if (distance > AP_EVENT_GUILD_MAX_USE_RANGE)
			break;
		ap_character_stop_action(mod->ap_character, c);
		ap_event_guild_make_packet(mod->ap_event_guild, 
			AP_EVENT_GUILD_PACKET_TYPE_GRANT, e, &c->id);
		as_player_send_packet(mod->as_player, c);
		break;
	}
	}
	return TRUE;
}

static boolean onregister(
	struct as_event_guild_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_guild, AP_EVENT_GUILD_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	ap_event_guild_add_callback(mod->ap_event_guild, AP_EVENT_GUILD_CB_RECEIVE, 
		mod, cbreceive);
	return TRUE;
}

static void onshutdown(struct as_event_guild_module * mod)
{
	vec_free(mod->list);
}

struct as_event_guild_module * as_event_guild_create_module()
{
	struct as_event_guild_module * mod = ap_module_instance_new(AS_EVENT_GUILD_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	mod->list = vec_new_reserved(sizeof(*mod->list), 128);
	return mod;
}
