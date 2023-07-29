#include "public/ap_pvp.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_guild.h"
#include "public/ap_packet.h"
#include "public/ap_summons.h"
#include "public/ap_tick.h"

#include "utility/au_packet.h"

#include <assert.h>

struct ap_pvp_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_packet_module * ap_packet;
	struct ap_summons_module * ap_summons;
	struct ap_tick_module * ap_tick;
	size_t character_offset;
	struct au_packet packet;
};

static boolean cbcharctor(
	struct ap_pvp_module * mod,
	struct ap_character * character)
{
	struct ap_pvp_character_attachment * attachment =
		ap_pvp_get_character_attachment(mod, character);
	attachment->friends = vec_new(sizeof(*attachment->friends));
	attachment->enemies = vec_new(sizeof(*attachment->friends));
	return TRUE;
}

static boolean cbchardtor(
	struct ap_pvp_module * mod,
	struct ap_character * character)
{
	struct ap_pvp_character_attachment * attachment =
		ap_pvp_get_character_attachment(mod, character);
	vec_free(attachment->friends);
	while (!vec_is_empty(attachment->enemies))
		ap_pvp_remove_enemy_by_index(mod, character, attachment, 0, FALSE, FALSE);
	vec_free(attachment->enemies);
	return TRUE;
}

static boolean onregister(
	struct ap_pvp_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_summons, AP_SUMMONS_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	mod->character_offset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct ap_pvp_character_attachment),
		mod, cbcharctor, cbchardtor);
	return TRUE;
}

static void onclose(struct ap_pvp_module * mod)
{
}

static void onshutdown(struct ap_pvp_module * mod)
{
}

struct ap_pvp_module * ap_pvp_create_module()
{
	struct ap_pvp_module * mod = ap_module_instance_new(AP_PVP_MODULE_NAME,
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	au_packet_init(&mod->packet, sizeof(uint16_t),
		AU_PACKET_TYPE_INT8, 1, /* Operation */
		AU_PACKET_TYPE_INT32, 1, /* CID */
		AU_PACKET_TYPE_INT32, 1, /* Target CID */
		AU_PACKET_TYPE_INT32, 1, /* Win */
		AU_PACKET_TYPE_INT32, 1, /* Lose */
		AU_PACKET_TYPE_INT8, 1, /* PvPMode */
		AU_PACKET_TYPE_INT8, 1, /* PvPStatus */
		AU_PACKET_TYPE_INT8, 1, /* Win */
		AU_PACKET_TYPE_CHAR, AP_GUILD_MAX_ID_LENGTH + 1, /* GuildID */
		AU_PACKET_TYPE_CHAR, AP_CHARACTER_MAX_NAME_LENGTH + 1, /* For System Message */
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_pvp_add_callback(
	struct ap_pvp_module * mod,
	enum ap_pvp_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t ap_pvp_attach_data(
	struct ap_pvp_module * mod,
	enum ap_pvp_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

struct ap_pvp_character_attachment * ap_pvp_get_character_attachment(
	struct ap_pvp_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->character_offset);
}

struct ap_pvp_char_info * ap_pvp_get_enemy(
	struct ap_pvp_module * mod, 
	struct ap_character * character, 
	struct ap_character * target)
{
	struct ap_pvp_character_attachment * attachment = 
		ap_pvp_get_character_attachment(mod, character);
	uint32_t i;
	uint32_t count = vec_count(attachment->enemies);
	assert(ap_character_is_pc(character));
	assert(ap_character_is_pc(target));
	for (i = 0; i < count; i++) {
		if (attachment->enemies[i].character_id == target->id)
			return &attachment->enemies[i];
	}
	return NULL;
}

struct ap_pvp_char_info * ap_pvp_add_enemy(
	struct ap_pvp_module * mod, 
	struct ap_character * character, 
	struct ap_character * target,
	enum ap_pvp_mode pvp_mode)
{
	struct ap_pvp_cb_add_enemy cb = { 0 };
	struct ap_pvp_character_attachment * attachment;
	struct ap_pvp_char_info * enemy;
	struct ap_pvp_char_info * targetenemy;
	character = ap_summons_get_root(mod->ap_summons, character);
	target = ap_summons_get_root(mod->ap_summons, target);
	attachment = ap_pvp_get_character_attachment(mod, character);
	enemy = vec_add_empty(&attachment->enemies);
	enemy->character_id = target->id;
	strlcpy(enemy->character_name, target->name, sizeof(enemy->character_name));
	enemy->pvp_mode = pvp_mode;
	cb.character = character;
	cb.character_attachment = attachment;
	cb.target = target;
	cb.enemy_info = enemy;
	ap_module_enum_callback(mod, AP_PVP_CB_ADD_ENEMY, &cb);
	attachment = ap_pvp_get_character_attachment(mod, target);
	targetenemy = vec_add_empty(&attachment->enemies);
	targetenemy->character_id = character->id;
	strlcpy(targetenemy->character_name, character->name, 
		sizeof(targetenemy->character_name));
	targetenemy->pvp_mode = pvp_mode;
	cb.character = target;
	cb.character_attachment = attachment;
	cb.target = character;
	cb.enemy_info = targetenemy;
	ap_module_enum_callback(mod, AP_PVP_CB_ADD_ENEMY, &cb);
	return enemy;
}

void ap_pvp_remove_enemy(
	struct ap_pvp_module * mod, 
	struct ap_character * character, 
	uint32_t target_id,
	boolean is_reactionary,
	enum ap_pvp_end_result end_result)
{
	struct ap_pvp_character_attachment * attachment = 
		ap_pvp_get_character_attachment(mod, character);
	uint32_t i;
	uint32_t count = vec_count(attachment->enemies);
	assert(ap_character_is_pc(character));
	for (i = 0; i < count; i++) {
		struct ap_pvp_char_info * info = &attachment->enemies[i];
		if (info->character_id == target_id) {
			ap_pvp_remove_enemy_by_index(mod, character, attachment, i, 
				is_reactionary, end_result);
			return;
		}
	}
	/* Unreachable! */
	assert(0);
}

uint32_t ap_pvp_remove_enemy_by_index(
	struct ap_pvp_module * mod, 
	struct ap_character * character, 
	struct ap_pvp_character_attachment * attachment, 
	uint32_t index,
	boolean is_reactionary,
	enum ap_pvp_end_result end_result)
{
	struct ap_pvp_cb_remove_enemy cb = { 0 };
	struct ap_pvp_char_info * info = &attachment->enemies[index];
	assert(ap_character_is_pc(character));
	cb.character = character;
	cb.character_attachment = attachment;
	cb.target_id = info->character_id;
	strlcpy(cb.target_name, info->character_name, sizeof(cb.target_name));
	cb.is_reactionary = is_reactionary;
	cb.end_result = end_result;
	index = vec_erase_iterator(attachment->enemies, index);
	ap_module_enum_callback(mod, AP_PVP_CB_REMOVE_ENEMY, &cb);
	return index;
}

boolean ap_pvp_is_target_enemy(
	struct ap_pvp_module * mod, 
	struct ap_character * character, 
	struct ap_character * target)
{
	struct ap_pvp_character_attachment * attachment;
	uint32_t i;
	uint32_t count;
	character = ap_summons_get_root(mod->ap_summons, character);
	target = ap_summons_get_root(mod->ap_summons, target);
	if (!ap_character_is_pc(character) || !ap_character_is_pc(target))
		return TRUE;
	attachment = ap_pvp_get_character_attachment(mod, character);
	count = vec_count(attachment->enemies);
	for (i = 0; i < count; i++) {
		if (attachment->enemies[i].character_id == target->id)
			return TRUE;
	}
	return FALSE;
}

boolean ap_pvp_is_target_natural_enemy(
	struct ap_pvp_module * mod, 
	struct ap_character * character, 
	struct ap_character * target)
{
	struct ap_pvp_cb_is_target_natural_enemy cb = { 0 };
	struct ap_pvp_character_attachment * attachment;
	/* Find root summoner. */
	character = ap_summons_get_root(mod->ap_summons, character);
	target = ap_summons_get_root(mod->ap_summons, target);
	if (!ap_character_is_pc(character) || !ap_character_is_pc(target)) {
		/* Non-player characters are enemies by default,
		 * at least in the context of pvp module. */
		return TRUE;
	}
	attachment = ap_pvp_get_character_attachment(mod, character);
	/** \todo
	 * Check:
	 * - Siege war type (from characterdatatable.txt).
	 * - If in battleground, race type.
	 * - If guilds are hostile.
	 * - If castle is under siege, attacker/defender guild situation. */
	cb.character = character;
	cb.target = target;
	ap_module_enum_callback(mod, AP_PVP_CB_IS_TARGET_NATURAL_ENEMY, &cb);
	return cb.is_enemy;
}

void ap_pvp_extend_duration(
	struct ap_pvp_module * mod, 
	struct ap_character * character, 
	struct ap_character * target,
	uint64_t duration_ms)
{
	struct ap_pvp_char_info * enemy;
	uint64_t endtick;
	character = ap_summons_get_root(mod->ap_summons, character);
	target = ap_summons_get_root(mod->ap_summons, target);
	if (!ap_character_is_pc(character) || !ap_character_is_pc(target))
		return;
	endtick = ap_tick_get(mod->ap_tick) + duration_ms;
	enemy = ap_pvp_get_enemy(mod, character, target);
	enemy->end_pvp_tick = endtick;
	enemy = ap_pvp_get_enemy(mod, target, character);
	enemy->end_pvp_tick = endtick;
}

boolean ap_pvp_on_receive(
	struct ap_pvp_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_pvp_cb_receive cb = { 0 };
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			NULL, /* Operation */
			NULL, /* CID */
			NULL, /* Target CID */
			NULL, /* Win */
			NULL, /* Lose */
			NULL, /* PvPMode */
			NULL, /* PvPStatus */
			NULL, /* Win */
			NULL, /* GuildID */
			NULL)) { /* For System Message */
		return FALSE;
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_PVP_CB_RECEIVE, &cb);
}

void ap_pvp_make_info_packet(
	struct ap_pvp_module * mod, 
	struct ap_character * character)
{
	uint8_t type = AP_PVP_PACKET_PVP_INFO;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	struct ap_pvp_character_attachment * attachment = 
		ap_pvp_get_character_attachment(mod, character);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_PVP_PACKET_TYPE, 
		&type, /* Operation */
		&character->id, /* CID */
		NULL, /* Target CID */
		&attachment->win_count, /* Win */
		&attachment->lose_count, /* Lose */
		&attachment->pvp_mode, /* PvPMode */
		&attachment->pvp_status, /* PvPStatus */
		NULL, /* Win */
		NULL, /* GuildID */
		NULL); /* For System Message */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_pvp_make_add_friend_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id,
	uint32_t target_id)
{
	uint8_t type = AP_PVP_PACKET_ADD_FRIEND;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_PVP_PACKET_TYPE, 
		&type, /* Operation */
		&character_id, /* CID */
		&target_id, /* Target CID */
		NULL, /* Win */
		NULL, /* Lose */
		NULL, /* PvPMode */
		NULL, /* PvPStatus */
		NULL, /* Win */
		NULL, /* GuildID */
		NULL); /* For System Message */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_pvp_make_add_enemy_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id,
	uint32_t target_id,
	enum ap_pvp_mode pvp_mode)
{
	uint8_t type = AP_PVP_PACKET_ADD_ENEMY;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_PVP_PACKET_TYPE, 
		&type, /* Operation */
		&character_id, /* CID */
		&target_id, /* Target CID */
		NULL, /* Win */
		NULL, /* Lose */
		&pvp_mode, /* PvPMode */
		NULL, /* PvPStatus */
		NULL, /* Win */
		NULL, /* GuildID */
		NULL); /* For System Message */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_pvp_make_remove_friend_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id,
	uint32_t target_id)
{
	uint8_t type = AP_PVP_PACKET_REMOVE_FRIEND;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_PVP_PACKET_TYPE, 
		&type, /* Operation */
		&character_id, /* CID */
		&target_id, /* Target CID */
		NULL, /* Win */
		NULL, /* Lose */
		NULL, /* PvPMode */
		NULL, /* PvPStatus */
		NULL, /* Win */
		NULL, /* GuildID */
		NULL); /* For System Message */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_pvp_make_remove_enemy_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id,
	uint32_t target_id)
{
	uint8_t type = AP_PVP_PACKET_REMOVE_ENEMY;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_PVP_PACKET_TYPE, 
		&type, /* Operation */
		&character_id, /* CID */
		&target_id, /* Target CID */
		NULL, /* Win */
		NULL, /* Lose */
		NULL, /* PvPMode */
		NULL, /* PvPStatus */
		NULL, /* Win */
		NULL, /* GuildID */
		NULL); /* For System Message */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_pvp_make_init_friend_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id)
{
	uint8_t type = AP_PVP_PACKET_INIT_FRIEND;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_PVP_PACKET_TYPE, 
		&type, /* Operation */
		&character_id, /* CID */
		NULL, /* Target CID */
		NULL, /* Win */
		NULL, /* Lose */
		NULL, /* PvPMode */
		NULL, /* PvPStatus */
		NULL, /* Win */
		NULL, /* GuildID */
		NULL); /* For System Message */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_pvp_make_init_enemy_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id)
{
	uint8_t type = AP_PVP_PACKET_INIT_ENEMY;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_PVP_PACKET_TYPE, 
		&type, /* Operation */
		&character_id, /* CID */
		NULL, /* Target CID */
		NULL, /* Win */
		NULL, /* Lose */
		NULL, /* PvPMode */
		NULL, /* PvPStatus */
		NULL, /* Win */
		NULL, /* GuildID */
		NULL); /* For System Message */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_pvp_make_add_friend_guild_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id,
	const char * guild_id)
{
	uint8_t type = AP_PVP_PACKET_ADD_FRIEND_GUILD;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	char guildid[AP_GUILD_MAX_ID_LENGTH + 1];
	strlcpy(guildid, guild_id, sizeof(guildid));
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_PVP_PACKET_TYPE, 
		&type, /* Operation */
		&character_id, /* CID */
		NULL, /* Target CID */
		NULL, /* Win */
		NULL, /* Lose */
		NULL, /* PvPMode */
		NULL, /* PvPStatus */
		NULL, /* Win */
		guildid, /* GuildID */
		NULL); /* For System Message */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_pvp_make_add_enemy_guild_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id,
	const char * guild_id,
	enum ap_pvp_mode pvp_mode)
{
	uint8_t type = AP_PVP_PACKET_ADD_ENEMY_GUILD;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	char guildid[AP_GUILD_MAX_ID_LENGTH + 1];
	strlcpy(guildid, guild_id, sizeof(guildid));
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_PVP_PACKET_TYPE, 
		&type, /* Operation */
		&character_id, /* CID */
		NULL, /* Target CID */
		NULL, /* Win */
		NULL, /* Lose */
		&pvp_mode, /* PvPMode */
		NULL, /* PvPStatus */
		NULL, /* Win */
		guildid, /* GuildID */
		NULL); /* For System Message */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_pvp_make_remove_friend_guild_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id,
	const char * guild_id)
{
	uint8_t type = AP_PVP_PACKET_REMOVE_FRIEND_GUILD;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	char guildid[AP_GUILD_MAX_ID_LENGTH + 1];
	strlcpy(guildid, guild_id, sizeof(guildid));
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_PVP_PACKET_TYPE, 
		&type, /* Operation */
		&character_id, /* CID */
		NULL, /* Target CID */
		NULL, /* Win */
		NULL, /* Lose */
		NULL, /* PvPMode */
		NULL, /* PvPStatus */
		NULL, /* Win */
		guildid, /* GuildID */
		NULL); /* For System Message */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_pvp_make_remove_enemy_guild_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id,
	const char * guild_id)
{
	uint8_t type = AP_PVP_PACKET_REMOVE_ENEMY_GUILD;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	char guildid[AP_GUILD_MAX_ID_LENGTH + 1];
	strlcpy(guildid, guild_id, sizeof(guildid));
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_PVP_PACKET_TYPE, 
		&type, /* Operation */
		&character_id, /* CID */
		NULL, /* Target CID */
		NULL, /* Win */
		NULL, /* Lose */
		NULL, /* PvPMode */
		NULL, /* PvPStatus */
		NULL, /* Win */
		guildid, /* GuildID */
		NULL); /* For System Message */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_pvp_make_init_friend_guild_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id)
{
	uint8_t type = AP_PVP_PACKET_INIT_FRIEND_GUILD;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_PVP_PACKET_TYPE, 
		&type, /* Operation */
		&character_id, /* CID */
		NULL, /* Target CID */
		NULL, /* Win */
		NULL, /* Lose */
		NULL, /* PvPMode */
		NULL, /* PvPStatus */
		NULL, /* Win */
		NULL, /* GuildID */
		NULL); /* For System Message */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_pvp_make_init_enemy_guild_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id)
{
	uint8_t type = AP_PVP_PACKET_INIT_ENEMY_GUILD;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_PVP_PACKET_TYPE, 
		&type, /* Operation */
		&character_id, /* CID */
		NULL, /* Target CID */
		NULL, /* Win */
		NULL, /* Lose */
		NULL, /* PvPMode */
		NULL, /* PvPStatus */
		NULL, /* Win */
		NULL, /* GuildID */
		NULL); /* For System Message */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_pvp_make_system_message_packet(
	struct ap_pvp_module * mod, 
	enum ap_pvp_system_code code,
	uint32_t param1,
	uint32_t param2,
	const char * string1,
	const char * string2)
{
	uint8_t type = AP_PVP_PACKET_SYSTEM_MESSAGE;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	char str1[AP_GUILD_MAX_ID_LENGTH + 1] = { 0 };
	char str2[AP_CHARACTER_MAX_NAME_LENGTH + 1] = { 0 };
	if (string1)
		strlcpy(str1, string1, sizeof(str1));
	if (string2)
		strlcpy(str2, string2, sizeof(str2));
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_PVP_PACKET_TYPE, 
		&type, /* Operation */
		&code, /* CID */
		NULL, /* Target CID */
		(param1 != UINT32_MAX) ? &param1 : NULL, /* Win */
		(param2 != UINT32_MAX) ? &param2 : NULL, /* Win */
		NULL, /* PvPMode */
		NULL, /* PvPStatus */
		NULL, /* Win */
		str1[0] ? str1 : NULL, /* GuildID */
		str2[0] ? str2 : NULL); /* For System Message */
	ap_packet_set_length(mod->ap_packet, length);
}
