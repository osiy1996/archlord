#include "server/as_event_npc_dialog_process.h"

#include "core/log.h"
#include "core/vector.h"

#include "public/ap_event_npc_dialog.h"
#include "public/ap_tick.h"

#include "server/as_map.h"
#include "server/as_player.h"

#include <assert.h>

struct as_event_npc_dialog_process_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_event_npc_dialog_module * ap_event_npc_dialog;
	struct as_map_module * as_map;
	struct as_player_module * as_player;
	struct ap_character ** list;
	size_t character_ad;
};

static boolean cbcharstopaction(
	struct as_event_npc_dialog_process_module * mod,
	struct ap_character_cb_stop_action * cb)
{
	struct as_event_npc_dialog_process_character_attachment * cd = 
		as_event_npc_dialog_process_get_character_attachment(mod, cb->character);
	cd->target_npc = NULL;
	cd->event = NULL;
	return TRUE;
}

static boolean cbcharprocess(
	struct as_event_npc_dialog_process_module * mod,
	struct ap_character_cb_process * cb)
{
	struct as_event_npc_dialog_process_character_attachment * c = 
		as_event_npc_dialog_process_get_character_attachment(mod, cb->character);
	struct ap_event_npc_dialog_character_attachment * attachment;
	float distance;
	if (!c->target_npc)
		return TRUE;
	distance = au_distance2d(&c->target_npc->pos, 
		&cb->character->pos);
	if (distance > AP_EVENT_NPC_DIALOG_MAX_USE_RANGE)
		return TRUE;
	attachment = ap_event_npc_dialog_get_character_attachment(
		mod->ap_event_npc_dialog, c->target_npc);
	if (attachment->menu) {
		ap_event_npc_dialog_make_grant_menu_packet(mod->ap_event_npc_dialog,
			cb->character->id, c->target_npc->id, attachment->menu);
		as_player_send_packet(mod->as_player, cb->character);
	}
	else {
		ap_event_npc_dialog_make_packet(mod->ap_event_npc_dialog,
			AP_EVENT_NPC_DIALOG_PACKET_TYPE_GRANT,
			c->event, &cb->character->id);
		as_player_send_packet(mod->as_player, cb->character);
	}
	ap_character_stop_movement(mod->ap_character, cb->character);
	ap_character_stop_action(mod->ap_character, cb->character);
	return TRUE;
}

static boolean cbreceive(
	struct as_event_npc_dialog_process_module * mod,
	struct ap_event_npc_dialog_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	struct ap_character * npc = NULL;
	struct as_event_npc_dialog_process_character_attachment * cd;
	switch (cb->type) {
	case AP_EVENT_NPC_DIALOG_PACKET_TYPE_REQUEST: {
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
			AP_EVENT_MANAGER_FUNCTION_NPCDIALOG);
		if (!e)
			break;
		ap_character_stop_action(mod->ap_character, c);
		distance = au_distance2d(&npc->pos, &c->pos);
		if (distance > AP_EVENT_NPC_DIALOG_MAX_USE_RANGE) {
			vec2 dir = { npc->pos.x - c->pos.x, 
				npc->pos.z - c->pos.z };
			float len = distance - 
				0.80f * AP_EVENT_NPC_DIALOG_MAX_USE_RANGE;
			struct au_pos dst = { 0 };
			glm_vec2_normalize(dir);
			dst.x = c->pos.x + dir[0] * len;
			dst.y = c->pos.y;
			dst.z = c->pos.z + dir[1] * len;
			ap_character_set_movement(mod->ap_character, c, &dst, TRUE);
		}
		cd = as_event_npc_dialog_process_get_character_attachment(mod, c);
		cd->target_npc = npc;
		cd->event = e;
		break;
	}
	case AP_EVENT_NPC_DIALOG_PACKET_TYPE_REQUEST_EX: {
		const struct ap_event_npc_dialog_request_packet * request = cb->packet;
		struct ap_character * npc = as_map_get_npc(mod->as_map, request->npc_id);
		struct ap_event_npc_dialog_character_attachment * attachment;
		struct ap_event_npc_dialog_menu_item * item;
		if (!npc)
			break;
		if (au_distance2d(&npc->pos, &c->pos) > AP_EVENT_NPC_DIALOG_MAX_USE_RANGE)
			break;
		attachment  = ap_event_npc_dialog_get_character_attachment(
			mod->ap_event_npc_dialog, npc);
		if (!attachment->menu)
			break;
		item = ap_event_npc_dialog_find_menu_item(attachment->menu, request->event_index);
		if (!item)
			break;
		switch (item->type) {
		case AP_EVENT_NPC_DIALOG_MENU_ITEM_MENU:
			ap_event_npc_dialog_make_grant_menu_packet(mod->ap_event_npc_dialog,
				c->id, npc->id, item);
			as_player_send_packet(mod->as_player, c);
			break;
		case AP_EVENT_NPC_DIALOG_MENU_ITEM_BUTTON:
			if (item->data.button.callback) {
				struct ap_event_npc_dialog_menu_callback cb = { 0 };
				cb.character = c;
				cb.npc = npc;
				cb.callback_data = item->data.button.callback_data;
				item->data.button.callback(item->data.button.callback_module, &cb);
			}
			break;
		case AP_EVENT_NPC_DIALOG_MENU_ITEM_HIDDEN:
			if (item->data.hidden.callback) {
				struct ap_event_npc_dialog_menu_callback cb = { 0 };
				cb.character = c;
				cb.npc = npc;
				cb.callback_data = item->data.hidden.callback_data;
				item->data.hidden.callback(item->data.hidden.callback_module, &cb);
			}
			break;
		}
		break;
	}
	}
	return TRUE;
}

static boolean onregister(
	struct as_event_npc_dialog_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_npc_dialog, AP_EVENT_NPC_DIALOG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	ap_event_npc_dialog_add_callback(mod->ap_event_npc_dialog,
		AP_EVENT_NPC_DIALOG_CB_RECEIVE, mod, cbreceive);
	mod->character_ad = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct as_event_npc_dialog_process_character_attachment), 
		mod, NULL, NULL);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_STOP_ACTION, mod, cbcharstopaction);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_PROCESS, mod, cbcharprocess);
	return TRUE;
}

static void onshutdown(struct as_event_npc_dialog_process_module * mod)
{
	vec_free(mod->list);
}

struct as_event_npc_dialog_process_module * as_event_npc_dialog_process_create_module()
{
	struct as_event_npc_dialog_process_module * mod = ap_module_instance_new(AS_EVENT_NPC_DIALOG_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	mod->list = vec_new_reserved(sizeof(*mod->list), 128);
	return mod;
}

struct as_event_npc_dialog_process_character_attachment * as_event_npc_dialog_process_get_character_attachment(
	struct as_event_npc_dialog_process_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->character_ad);
}
