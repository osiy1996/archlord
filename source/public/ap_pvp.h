#ifndef _AP_PVP_H_
#define _AP_PVP_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_character.h"
#include "public/ap_define.h"
#include "public/ap_module_instance.h"

#define AP_PVP_MODULE_NAME "AgpmPvP"

BEGIN_DECLS

enum ap_pvp_mode {
	AP_PVP_MODE_NONE = 0,
	AP_PVP_MODE_SAFE,
	AP_PVP_MODE_FREE,
	AP_PVP_MODE_COMBAT,
	AP_PVP_MODE_BATTLE,
};

enum ap_pvp_status {
	AP_PVP_STATUS_NONE = 0x00,
	AP_PVP_STATUS_NOWPVP = 0x01,
	AP_PVP_STATUS_INVINCIBLE = 0x02,
};

enum ap_pvp_end_result {
	AP_PVP_END_RESULT_TIMEOUT,
	AP_PVP_END_RESULT_WIN,
	AP_PVP_END_RESULT_LOSE,
};

enum ap_pvp_system_code {
	AP_PVP_SYSTEM_CODE_ENTER_SAFE_AREA = 0,
	AP_PVP_SYSTEM_CODE_ENTER_FREE_AREA,
	AP_PVP_SYSTEM_CODE_ENTER_COMBAT_AREA,
	AP_PVP_SYSTEM_CODE_NOW_PVP_STATUS,
	AP_PVP_SYSTEM_CODE_NONE_PVP_STATUS,
	AP_PVP_SYSTEM_CODE_DEAD,
	AP_PVP_SYSTEM_CODE_DEAD_GUILD_MEMBER,
	AP_PVP_SYSTEM_CODE_DEAD_PARTY_MEMBER,
	AP_PVP_SYSTEM_CODE_ITEM_DROP,
	AP_PVP_SYSTEM_CODE_START_INVINCIBLE,
	AP_PVP_SYSTEM_CODE_END_INVINCIBLE,
	AP_PVP_SYSTEM_CODE_CANNOT_USE_TELEPORT,
	AP_PVP_SYSTEM_CODE_KILL_PLAYER,
	AP_PVP_SYSTEM_CODE_MOVE_BATTLE_SQAURE,
	AP_PVP_SYSTEM_CODE_LEAVE_GUILD_OR_PARTY,
	AP_PVP_SYSTEM_CODE_CANNOT_INVITE_GUILD,
	AP_PVP_SYSTEM_CODE_CANNOT_INVITE_PARTY,
	AP_PVP_SYSTEM_CODE_TARGET_INVINCIBLE,
	AP_PVP_SYSTEM_CODE_NOT_ENOUGH_LEVEL,
	AP_PVP_SYSTEM_CODE_CANNOT_INVITE_MEMBER,
	AP_PVP_SYSTEM_CODE_CANNOT_ATTACK_FRIEND,
	AP_PVP_SYSTEM_CODE_CANNOT_ATTACK_OTHER,
	AP_PVP_SYSTEM_CODE_SKILL_CANNOT_APPLY_EFFECT,
	AP_PVP_SYSTEM_CODE_ITEM_ANTI_DROP,
	AP_PVP_SYSTEM_CODE_CHARISMA_UP,
	AP_PVP_SYSTEM_CODE_CANNOT_PVP_BY_LEVEL,
	AP_PVP_SYSTEM_CODE_DISABLE_NORMAL_ATTACK,
	AP_PVP_SYSTEM_CODE_DISABLE_SKILL_CAST,
};

enum ap_pvp_packet_type {
	AP_PVP_PACKET_PVP_INFO = 0,
	AP_PVP_PACKET_ADD_FRIEND,
	AP_PVP_PACKET_ADD_ENEMY,
	AP_PVP_PACKET_REMOVE_FRIEND,
	AP_PVP_PACKET_REMOVE_ENEMY,
	AP_PVP_PACKET_INIT_FRIEND,
	AP_PVP_PACKET_INIT_ENEMY,
	AP_PVP_PACKET_UPDATE_FRIEND,
	AP_PVP_PACKET_UPDATE_ENEMY,
	AP_PVP_PACKET_ADD_FRIEND_GUILD,
	AP_PVP_PACKET_ADD_ENEMY_GUILD,
	AP_PVP_PACKET_REMOVE_FRIEND_GUILD,
	AP_PVP_PACKET_REMOVE_ENEMY_GUILD,
	AP_PVP_PACKET_INIT_FRIEND_GUILD,
	AP_PVP_PACKET_INIT_ENEMY_GUILD,
	AP_PVP_PACKET_UPDATE_FRIEND_GUILD,
	AP_PVP_PACKET_UPDATE_ENEMY_GUILD,
	AP_PVP_PACKET_CANNOT_USE_TELEPORT,
	AP_PVP_PACKET_SYSTEM_MESSAGE,
	AP_PVP_PACKET_REQUEST_DEAD_TYPE,
	AP_PVP_PACKET_RESPONSE_DEAD_TYPE,
	AP_PVP_PACKET_RACE_BATTLE,
};

enum ap_pvp_callback_id {
	AP_PVP_CB_RECEIVE,
	AP_PVP_CB_ADD_ENEMY,
	AP_PVP_CB_REMOVE_ENEMY,
	AP_PVP_CB_IS_TARGET_NATURAL_ENEMY,
};

enum ap_pvp_mode_data_index {
};

struct ap_pvp_char_info {
	uint32_t character_id;
	char character_name[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	enum ap_pvp_mode pvp_mode;
	uint64_t end_pvp_tick;
};

struct ap_pvp_character_attachment {
	boolean init;
	uint32_t win_count;
	uint32_t lose_count;
	enum ap_pvp_mode pvp_mode;
	enum ap_pvp_status pvp_status;
	boolean died_in_combat_area;
	struct ap_pvp_char_info * friends;
	struct ap_pvp_char_info * enemies;
};

struct ap_pvp_cb_receive {
	enum ap_pvp_packet_type type;
	uint32_t character_id;
	uint32_t target_id;
	void * user_data;
};

struct ap_pvp_cb_add_enemy {
	struct ap_character * character;
	struct ap_pvp_character_attachment * character_attachment;
	struct ap_character * target;
	struct ap_pvp_char_info * enemy_info;
};

struct ap_pvp_cb_remove_enemy {
	struct ap_character * character;
	struct ap_pvp_character_attachment * character_attachment;
	uint32_t target_id;
	char target_name[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	boolean is_reactionary;
	enum ap_pvp_end_result end_result;
};

struct ap_pvp_cb_is_target_natural_enemy {
	struct ap_character * character;
	struct ap_character * target;
	boolean is_enemy;
};

struct ap_pvp_module * ap_pvp_create_module();

void ap_pvp_add_callback(
	struct ap_pvp_module * mod,
	enum ap_pvp_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t ap_pvp_attach_data(
	struct ap_pvp_module * mod,
	enum ap_pvp_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

struct ap_pvp_character_attachment * ap_pvp_get_character_attachment(
	struct ap_pvp_module * mod,
	struct ap_character * character);

struct ap_pvp_char_info * ap_pvp_get_enemy(
	struct ap_pvp_module * mod, 
	struct ap_character * character, 
	struct ap_character * target);

struct ap_pvp_char_info * ap_pvp_add_enemy(
	struct ap_pvp_module * mod, 
	struct ap_character * character, 
	struct ap_character * target,
	enum ap_pvp_mode pvp_mode);

void ap_pvp_remove_enemy(
	struct ap_pvp_module * mod, 
	struct ap_character * character, 
	uint32_t target_id,
	boolean is_reactionary,
	enum ap_pvp_end_result end_result);

uint32_t ap_pvp_remove_enemy_by_index(
	struct ap_pvp_module * mod, 
	struct ap_character * character, 
	struct ap_pvp_character_attachment * attachment, 
	uint32_t index,
	boolean is_reactionary,
	enum ap_pvp_end_result end_result);

boolean ap_pvp_is_target_enemy(
	struct ap_pvp_module * mod, 
	struct ap_character * character, 
	struct ap_character * target);

boolean ap_pvp_is_target_natural_enemy(
	struct ap_pvp_module * mod, 
	struct ap_character * character, 
	struct ap_character * target);

void ap_pvp_extend_duration(
	struct ap_pvp_module * mod, 
	struct ap_character * character, 
	struct ap_character * target,
	uint64_t duration_ms);

boolean ap_pvp_on_receive(
	struct ap_pvp_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_pvp_make_info_packet(
	struct ap_pvp_module * mod, 
	struct ap_character * character);

void ap_pvp_make_add_friend_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id,
	uint32_t target_id);

void ap_pvp_make_add_enemy_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id,
	uint32_t target_id,
	enum ap_pvp_mode pvp_mode);

void ap_pvp_make_remove_friend_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id,
	uint32_t target_id);

void ap_pvp_make_remove_enemy_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id,
	uint32_t target_id);

void ap_pvp_make_init_friend_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id);

void ap_pvp_make_init_enemy_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id);

void ap_pvp_make_add_friend_guild_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id,
	const char * guild_id);

void ap_pvp_make_add_enemy_guild_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id,
	const char * guild_id,
	enum ap_pvp_mode pvp_mode);

void ap_pvp_make_remove_friend_guild_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id,
	const char * guild_id);

void ap_pvp_make_remove_enemy_guild_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id,
	const char * guild_id);

void ap_pvp_make_init_friend_guild_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id);

void ap_pvp_make_init_enemy_guild_packet(
	struct ap_pvp_module * mod, 
	uint32_t character_id);

void ap_pvp_make_system_message_packet(
	struct ap_pvp_module * mod, 
	enum ap_pvp_system_code code,
	uint32_t param1,
	uint32_t param2,
	const char * string1,
	const char * string2);

static inline enum ap_pvp_end_result ap_pvp_get_opposing_end_result(
	enum ap_pvp_end_result end_result)
{
	switch (end_result) {
	default:
	case AP_PVP_END_RESULT_TIMEOUT:
		return AP_PVP_END_RESULT_TIMEOUT;
	case AP_PVP_END_RESULT_WIN:
		return AP_PVP_END_RESULT_LOSE;
	case AP_PVP_END_RESULT_LOSE:
		return AP_PVP_END_RESULT_WIN;
	}
}

END_DECLS

#endif /* _AP_PVP_H_ */