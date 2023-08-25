#include "server/as_service_npc.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_service_npc.h"
#include "public/ap_tick.h"

#include "server/as_account.h"
#include "server/as_character.h"
#include "server/as_database.h"
#include "server/as_player.h"

#include <assert.h>

#define DB_STREAM_MODULE_ID 5
#define MAKE_ID(ID) AS_DATABASE_MAKE_ID(DB_STREAM_MODULE_ID, ID)

struct as_service_npc_module {
	struct ap_module_instance instance;
	struct ap_service_npc_module * ap_service_npc;
	struct ap_tick_module * ap_tick;
	struct as_account_module * as_account;
	struct as_character_module * as_character;
	struct as_database_module * as_database;
	struct as_player_module * as_player;
	size_t character_database_attachment_offset;
};

static boolean cbchardecode(
	struct as_service_npc_module * mod, 
	struct as_character_cb_decode * cb)
{
	boolean result = TRUE;
	struct as_service_npc_character_database_attachment * db = 
		as_service_npc_get_character_database_attachment(mod, cb->character);
	if (cb->module_id != DB_STREAM_MODULE_ID)
		return TRUE;
	switch (cb->field_id) {
	case AS_SERVICE_NPC_CHARACTER_DATABASE_ATTACHMENT_RECEIVED_LEVEL_UP_REWARD_MILESTONE: {
		uint32_t level = 0;
		if (!as_database_read_decoded(cb->codec, &level, sizeof(level))) {
			ERROR("Failed to read received level up reward milestone.");
			return TRUE;
		}
		if (db->received_level_up_reward_milestone_count >= COUNT_OF(db->received_level_up_reward_milestones)) {
			WARN("Received level up reward milestone is ignored due to insufficient memory (character = %s, level = %u).",
				cb->character->name, level);
		}
		else {
			uint32_t index = db->received_level_up_reward_milestone_count++;
			db->received_level_up_reward_milestones[index] = level;
		}
		break;
	}
	default:
		assert(0);
		return TRUE;
	}
	return !result;
}

static boolean cbcharencode(
	struct as_service_npc_module * mod, 
	struct as_character_cb_encode * cb)
{
	boolean result = TRUE;
	const struct as_service_npc_character_database_attachment * db = 
		as_service_npc_get_character_database_attachment(mod, cb->character);
	uint32_t i;
	for (i = 0; i < db->received_level_up_reward_milestone_count; i++) {
		result &= as_database_encode(cb->codec, 
			MAKE_ID(AS_SERVICE_NPC_CHARACTER_DATABASE_ATTACHMENT_RECEIVED_LEVEL_UP_REWARD_MILESTONE),
			&db->received_level_up_reward_milestones[i], 
			sizeof(db->received_level_up_reward_milestones[i]));
	}
	return result;
}

static boolean cbcharcopy(
	struct as_service_npc_module * mod, 
	struct as_character_cb_copy * cb)
{
	const struct as_service_npc_character_database_attachment * src = 
		as_service_npc_get_character_database_attachment(mod, cb->src);
	struct as_service_npc_character_database_attachment * dst = 
		as_service_npc_get_character_database_attachment(mod, cb->dst);
	memcpy(dst, src, sizeof(*src));
	return TRUE;
}

static boolean onregister(
	struct as_service_npc_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_service_npc, AP_SERVICE_NPC_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_account, AS_ACCOUNT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_database, AS_DATABASE_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	mod->character_database_attachment_offset = as_character_attach_data(mod->as_character,
		AS_CHARACTER_MDI_DATABASE, sizeof(struct as_service_npc_character_database_attachment), 
		mod, NULL, NULL);
	if (!as_character_set_database_stream(mod->as_character, DB_STREAM_MODULE_ID,
			mod, cbchardecode, cbcharencode)) {
		ERROR("Failed to set character database stream.");
		return FALSE;
	}
	as_character_add_callback(mod->as_character, AS_CHARACTER_CB_COPY, mod, cbcharcopy);
	return TRUE;
}

void onclose(struct as_service_npc_module * mod)
{
}

struct as_service_npc_module * as_service_npc_create_module()
{
	struct as_service_npc_module * mod = ap_module_instance_new(AS_SERVICE_NPC_MODULE_NAME,
		sizeof(*mod), onregister, NULL, onclose, NULL);
	return mod;
}

void as_service_npc_add_callback(
	struct as_service_npc_module * mod,
	enum as_service_npc_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

struct as_service_npc_character_database_attachment * as_service_npc_get_character_database_attachment(
	struct as_service_npc_module * mod,
	struct as_character_db * character)
{
	return ap_module_get_attached_data(character, 
		mod->character_database_attachment_offset);
}

boolean as_service_npc_add_received_level_up_reward_milestone(
	struct as_service_npc_module * mod,
	struct ap_character * character,
	uint32_t level)
{
	struct as_character * c = as_character_get(mod->as_character, character);
	struct as_service_npc_character_database_attachment * attachment;
	uint32_t i;
	if (!c->db)
		return FALSE;
	attachment = as_service_npc_get_character_database_attachment(mod, c->db);
	if (attachment->received_level_up_reward_milestone_count >= AS_SERVICE_NPC_MAX_LEVEL_UP_REWARD_MILESTONE_COUNT)
		return FALSE;
	for (i = 0; i < attachment->received_level_up_reward_milestone_count; i++) {
		if (attachment->received_level_up_reward_milestones[i] == level)
			return FALSE;
	}
	i = attachment->received_level_up_reward_milestone_count++;
	attachment->received_level_up_reward_milestones[i] = level;
	return TRUE;
}

void as_service_npc_reset_received_level_up_rewards(
	struct as_service_npc_module * mod,
	struct ap_character * character)
{
	struct as_character * c = as_character_get(mod->as_character, character);
	if (c->db) {
		struct as_service_npc_character_database_attachment * attachment =
			as_service_npc_get_character_database_attachment(mod, c->db);
		attachment->received_level_up_reward_milestone_count = 0;
	}
}
