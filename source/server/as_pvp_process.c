#include "server/as_pvp_process.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/vector.h"

#include "public/ap_chat.h"
#include "public/ap_guild.h"
#include "public/ap_module.h"
#include "public/ap_party.h"
#include "public/ap_pvp.h"
#include "public/ap_tick.h"

#include "server/as_character.h"
#include "server/as_map.h"
#include "server/as_party.h"
#include "server/as_player.h"

#include <assert.h>

struct as_pvp_process_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_chat_module * ap_chat;
	struct ap_guild_module * ap_guild;
	struct ap_party_module * ap_party;
	struct ap_pvp_module * ap_pvp;
	struct ap_tick_module * ap_tick;
	struct as_character_module * as_character;
	struct as_map_module * as_map;
	struct as_party_module * as_party;
	struct as_player_module * as_player;
	struct ap_character ** list;
};

static boolean cbcharprocess(
	struct as_pvp_process_module * mod,
	struct ap_character_cb_process * cb)
{
	struct ap_character * c = cb->character;
	return TRUE;
}

static boolean cbcharisvalidattacktarget(
	struct as_pvp_process_module * mod,
	struct ap_character_cb_is_valid_attack_target * cb)
{
	if (!ap_pvp_is_target_enemy(mod->ap_pvp, cb->character, cb->target)) {
		if (!ap_pvp_is_target_natural_enemy(mod->ap_pvp, cb->character, cb->target)) {
			if (cb->force_attack) {
				ap_pvp_add_enemy(mod->ap_pvp, cb->character, cb->target, 
					AP_PVP_MODE_FREE);
				return TRUE;
			}
			else {
				return FALSE;
			}
		}
		else {
			return TRUE;
		}
	}
	else {
		return TRUE;
	}
}

static boolean cbchardeath(
	struct as_pvp_process_module * mod,
	struct ap_character_cb_death * cb)
{
	struct ap_pvp_character_attachment * attachment = 
		ap_pvp_get_character_attachment(mod->ap_pvp, cb->character);
	while (vec_count(attachment->enemies)) {
		ap_pvp_remove_enemy_by_index(mod->ap_pvp, cb->character, attachment, 0, 
			FALSE, AP_PVP_END_RESULT_LOSE);
	}
	return TRUE;
}

static boolean cbcharspecialstatuson(
	struct as_pvp_process_module * mod,
	struct ap_character_cb_special_status_on * cb)
{
	if (cb->special_status & AP_CHARACTER_SPECIAL_STATUS_INVINCIBLE) {
		ap_pvp_make_system_message_packet(mod->ap_pvp, 
			AP_PVP_SYSTEM_CODE_START_INVINCIBLE, UINT32_MAX, UINT32_MAX, NULL, NULL);
		as_player_send_packet(mod->as_player, cb->character);
	}
	return TRUE;
}

static boolean cbcharspecialstatusoff(
	struct as_pvp_process_module * mod,
	struct ap_character_cb_special_status_on * cb)
{
	if (cb->special_status & AP_CHARACTER_SPECIAL_STATUS_INVINCIBLE) {
		ap_pvp_make_system_message_packet(mod->ap_pvp, 
			AP_PVP_SYSTEM_CODE_END_INVINCIBLE, UINT32_MAX, UINT32_MAX, NULL, NULL);
		as_player_send_packet(mod->as_player, cb->character);
	}
	return TRUE;
}


static boolean cbcharload(
	struct as_pvp_process_module * mod,
	struct as_character_cb_load * cb)
{
	struct ap_pvp_character_attachment * attachment = 
		ap_pvp_get_character_attachment(mod->ap_pvp, cb->character);
	attachment->win_count = cb->db->pvp_win;
	attachment->lose_count = cb->db->pvp_lose;
	return TRUE;
}

static boolean cbcharreflect(
	struct as_pvp_process_module * mod,
	struct as_character_cb_reflect * cb)
{
	struct ap_pvp_character_attachment * attachment = 
		ap_pvp_get_character_attachment(mod->ap_pvp, cb->character);
	cb->db->pvp_win = attachment->win_count;
	cb->db->pvp_lose = attachment->lose_count;
	return TRUE;
}

static boolean cbreceive(
	struct as_pvp_process_module * mod,
	struct ap_pvp_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	return TRUE;
}

static boolean cbaddenemy(
	struct as_pvp_process_module * mod,
	struct ap_pvp_cb_add_enemy * cb)
{
	if (vec_count(cb->character_attachment->enemies) == 1) {
		/* First enemy. */
		ap_pvp_make_system_message_packet(mod->ap_pvp, 
			AP_PVP_SYSTEM_CODE_NOW_PVP_STATUS, UINT32_MAX, UINT32_MAX, NULL, NULL);
		as_player_send_packet(mod->as_player, cb->character);
	}
	ap_pvp_make_add_enemy_packet(mod->ap_pvp, cb->character->id, cb->target->id, 
		cb->enemy_info->pvp_mode);
	as_player_send_packet(mod->as_player, cb->character);
	if (cb->enemy_info->pvp_mode == AP_PVP_MODE_FREE &&
		cb->target->criminal_status == AP_CHARACTER_CRIMINAL_STATUS_INNOCENT &&
		cb->target->factor.char_status.murderer < AP_CHARACTER_MURDERER_LEVEL1_POINT) {
		struct as_map_region * region = 
			as_map_get_character_ad(mod->as_map, cb->target)->region;
		if (region && region->temp->type.props.safety_type == AP_MAP_ST_FREE) {
			cb->character->criminal_status = AP_CHARACTER_CRIMINAL_STATUS_CRIMINAL_FLAGGED;
			ap_character_set_criminal_duration(cb->character, 20 * 60 * 1000);
		}
	}
	return TRUE;
}

static boolean cbremoveenemy(
	struct as_pvp_process_module * mod,
	struct ap_pvp_cb_remove_enemy * cb)
{
	switch (cb->end_result) {
	case AP_PVP_END_RESULT_TIMEOUT:
		break;
	case AP_PVP_END_RESULT_WIN: {
		cb->character_attachment->win_count++;
		ap_pvp_make_info_packet(mod->ap_pvp, cb->character);
		as_player_send_packet(mod->as_player, cb->character);
		ap_pvp_make_system_message_packet(mod->ap_pvp, AP_PVP_SYSTEM_CODE_KILL_PLAYER,
			UINT32_MAX, UINT32_MAX, cb->target_name, NULL);
		as_player_send_packet(mod->as_player, cb->character);
		break;
	}
	case AP_PVP_END_RESULT_LOSE:
		cb->character_attachment->lose_count++;
		ap_pvp_make_info_packet(mod->ap_pvp, cb->character);
		as_player_send_packet(mod->as_player, cb->character);
		ap_pvp_make_system_message_packet(mod->ap_pvp, AP_PVP_SYSTEM_CODE_DEAD,
			UINT32_MAX, UINT32_MAX, cb->target_name, NULL);
		as_player_send_packet(mod->as_player, cb->character);
		break;
	}
	ap_pvp_make_remove_enemy_packet(mod->ap_pvp, cb->character->id, cb->target_id);
	as_player_send_packet(mod->as_player, cb->character);
	if (!vec_count(cb->character_attachment->enemies)) {
		/* No longer have enemies. */
		ap_pvp_make_system_message_packet(mod->ap_pvp, AP_PVP_SYSTEM_CODE_NONE_PVP_STATUS,
			UINT32_MAX, UINT32_MAX, NULL, NULL);
		as_player_send_packet(mod->as_player, cb->character);
	}
	if (!cb->is_reactionary) {
		struct ap_character * target = as_map_get_character(mod->as_map, 
			cb->target_id);
		assert(target != NULL);
		ap_pvp_remove_enemy(mod->ap_pvp, target, cb->character->id, 
			TRUE, ap_pvp_get_opposing_end_result(cb->end_result));
	}
	return TRUE;
}

static boolean cbmakeinvincible(
	struct as_pvp_process_module * mod,
	struct ap_pvp_cb_make_invincible * cb)
{
	return TRUE;
}

static boolean onregister(
	struct as_pvp_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_chat, AP_CHAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_guild, AP_GUILD_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_party, AP_PARTY_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_pvp, AP_PVP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_party, AS_PARTY_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_PROCESS, mod, cbcharprocess);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_IS_VALID_ATTACK_TARGET, mod, cbcharisvalidattacktarget);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_DEATH, mod, cbchardeath);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_SPECIAL_STATUS_ON, mod, cbcharspecialstatuson);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_SPECIAL_STATUS_OFF, mod, cbcharspecialstatusoff);
	as_character_add_callback(mod->as_character, AS_CHARACTER_CB_LOAD, mod, cbcharload);
	as_character_add_callback(mod->as_character, AS_CHARACTER_CB_REFLECT, mod, cbcharreflect);
	ap_pvp_add_callback(mod->ap_pvp, AP_PVP_CB_RECEIVE, mod, cbreceive);
	ap_pvp_add_callback(mod->ap_pvp, AP_PVP_CB_ADD_ENEMY, mod, cbaddenemy);
	ap_pvp_add_callback(mod->ap_pvp, AP_PVP_CB_REMOVE_ENEMY, mod, cbremoveenemy);
	return TRUE;
}

static void onshutdown(struct as_pvp_process_module * mod)
{
	vec_free(mod->list);
}

struct as_pvp_process_module * as_pvp_process_create_module()
{
	struct as_pvp_process_module * mod = ap_module_instance_new(AS_PVP_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	mod->list = vec_new_reserved(sizeof(*mod->list), 128);
	return mod;
}
