#ifndef _AP_GUILD_H_
#define _AP_GUILD_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_admin.h"
#include "public/ap_character.h"
#include "public/ap_define.h"
#include "public/ap_item.h"
#include "public/ap_module_instance.h"

#define AP_GUILD_MODULE_NAME "AgpmGuild"

#define AP_GUILD_MAX_MAKE_GUILD_ID_LENGTH 16
#define AP_GUILD_MAX_ID_LENGTH 32
#define AP_GUILD_MAX_DATE_LENGTH 32
#define AP_GUILD_MAX_MEMBER_COUNT 500
#define AP_GUILD_MAX_PASSWORD_LENGTH 12
#define AP_GUILD_MAX_NOTICE_LENGTH 89
#define AP_GUILD_MAX_GUILDMARK_TEMPLATE_COUNT 200
#define AP_GUILD_BATTLE_READY_TIME 30
#define AP_GUILD_BATTLE_DURATION_MIN 300
#define AP_GUILD_BATTLE_DURATION_MAX 10800
#define AP_GUILD_BATTLE_CANCEL_ENABLE_TIME 60
#define AP_GUILD_BATTLE_NEED_MEMBER_COUNT 10
#define AP_GUILD_BATTLE_NEED_LEVEL_SUM 100
#define AP_GUILD_MAX_VISIBLE_GUILD_LIST 19
#define AP_GUILD_LEAVE_NEED_TIME 60 * 60 * 24
#define AP_GUILD_MAX_JOINT_GUILD 3
#define AP_GUILD_MAX_HOSTILE_GUILD 5
#define AP_GUILD_MIN_JOINT_MEMBER_COUNT 20
#define AP_GUILD_MIN_HOSTILE_MEMBER_COUNT 10

#define AP_GUILD_LOG_FILE_NAME "AgsmGuild_Log.log"

#define	AP_GUILD_MARK_BOTTOM "Bottom"
#define	AP_GUILD_MARK_PATTERN "Pattern"
#define	AP_GUILD_MARK_SYMBOL "Symbol"
#define	AP_GUILD_MARK_COLOR "Color"

#define	AP_GUILD_ID_MARK_BOTTOM 1
#define	AP_GUILD_ID_MARK_PATTERN 2
#define	AP_GUILD_ID_MARK_SYMBOL 3
#define	AP_GUILD_ID_MARK_COLOR 4

#define AP_GUILD_ID_MARK_LIMIT 3

#define	AP_GUILD_GUILDMARK_MAX_COLOR 24

#define AP_GUILD_GUILDMARK_ENABLE_MEMBER_COUNT 10
#define AP_GUILD_GUILDMEMBER_INCREASE_ENABLE_MEMBER_COUNT 20

#define AP_GUILD_CREATE_REQUIRE_LEVEL 20
#define AP_GUILD_BATTLE_POINT_LEVEL_GAP_ZERO 123456

#define	AP_GUILD_MAX_AGPD_REQUIRE_MAX_MEMBER_INCREASE 6
#define	AP_GUILD_MAX_AGPD_GUILDMARK_TEMPLATE 5
#define AP_GUILD_GUILD_DESTROY_TIME 60 * 60 * 24

#define AP_GUILD_NEXT_GUILDMAX_MEMBER(GUILD) \
	((GUILD)->member_count > 0 ) ? (GUILD)->max_member_count + \
		(10 - ((GUILD)->max_member_count % 10) ) : 10;

BEGIN_DECLS

enum ap_guild_callback_id {
	AP_GUILD_CB_RECEIVE,
};

enum ap_guild_member_rank {
	AP_GUILD_MEMBER_RANK_NORMAL = 1,
	AP_GUILD_MEMBER_RANK_JOIN_REQUEST = 2,
	AP_GUILD_MEMBER_RANK_LEAVE_REQUEST = 3,
	AP_GUILD_MEMBER_RANK_MASTER = 10,
	AP_GUILD_MEMBER_RANK_SUBMASTER = 11,
};

enum ap_guild_member_status {
	AP_GUILD_MEMBER_STATUS_OFFLINE = 0,
	AP_GUILD_MEMBER_STATUS_ONLINE,
};

enum ap_guild_status {
	AP_GUILD_STATUS_NONE = 0,
	AP_GUILD_STATUS_BATTLE_DECLARE,
	AP_GUILD_STATUS_BATTLE_READY,
	AP_GUILD_STATUS_BATTLE,
};

enum ap_guild_battle_result {
	AP_GUILD_BATTLE_RESULT_NONE = 0,
	AP_GUILD_BATTLE_RESULT_WIN,
	AP_GUILD_BATTLE_RESULT_DRAW,
	AP_GUILD_BATTLE_RESULT_LOSE,
	AP_GUILD_BATTLE_RESULT_WIN_BY_WITHDRAW,
	AP_GUILD_BATTLE_RESULT_LOSE_BY_WITHDRAW,
};

enum ap_guild_battle_type {
	AP_GUILD_BATTLE_TYPE_POINT_MATCH,
	AP_GUILD_BATTLE_TYPE_PK_MATCH,
	AP_GUILD_BATTLE_TYPE_DEATH_MATCH,
	AP_GUILD_BATTLE_TYPE_TOTAL_SURVIVE,
	AP_GUILD_BATTLE_TYPE_PRIVATE_SURVIVE,
	AP_GUILD_BATTLE_TYPE_COUNT,
};

enum ap_guild_battle_mode {
	AP_GUILD_BATTLE_MODE_TIME = 6,
	AP_GUILD_BATTLE_MODE_TOTAL = 6,
	AP_GUILD_BATTLE_MODE_PRIVATE = 4,
};

extern uint32_t g_GuildBattleTime[AP_GUILD_BATTLE_MODE_TIME];
extern uint32_t g_GuildBattleTotalMan[AP_GUILD_BATTLE_MODE_TOTAL];
extern uint32_t g_GuildBattlePrivateMan[AP_GUILD_BATTLE_MODE_PRIVATE];

enum ap_guild_rank {
	AP_GUILD_RANK_BEGINNER = 1,
};

enum ap_guild_relation_type {
	AP_GUILD_RELATION_NONE = 0,
	AP_GUILD_RELATION_JOINT,
	AP_GUILD_RELATION_JOINT_LEADER,
	AP_GUILD_RELATION_HOSTILE = 10,
};

enum ap_guild_battle_rank {
	AP_GUILD_BATTLE_RANK_NOTRANKED = 0,
	AP_GUILD_BATTLE_RANK_1STPLACE = 1,
	AP_GUILD_BATTLE_RANK_2NDPLACE,
	AP_GUILD_BATTLE_RANK_COUNT
};

enum ap_guild_packet_type {
	AP_GUILD_PACKET_TYPE_CREATE = 0,
	AP_GUILD_PACKET_TYPE_JOIN_REQUEST,
	AP_GUILD_PACKET_TYPE_JOIN_REJECT,
	AP_GUILD_PACKET_TYPE_JOIN,
	AP_GUILD_PACKET_TYPE_LEAVE,
	AP_GUILD_PACKET_TYPE_FORCED_LEAVE,
	AP_GUILD_PACKET_TYPE_DESTROY,
	AP_GUILD_PACKET_TYPE_UPDATE_MAX_MEMBER_COUNT,
	AP_GUILD_PACKET_TYPE_CHAR_DATA,
	AP_GUILD_PACKET_TYPE_UPDATE_NOTICE,
	AP_GUILD_PACKET_TYPE_SYSTEM_MESSAGE,
	AP_GUILD_PACKET_TYPE_UPDATE_STATUS,
	AP_GUILD_PACKET_TYPE_UPDATE_RECORD,
	AP_GUILD_PACKET_TYPE_BATTLE_INFO,
	AP_GUILD_PACKET_TYPE_BATTLE_PERSON,
	AP_GUILD_PACKET_TYPE_BATTLE_REQUEST,
	AP_GUILD_PACKET_TYPE_BATTLE_ACCEPT,
	AP_GUILD_PACKET_TYPE_BATTLE_REJECT,
	AP_GUILD_PACKET_TYPE_BATTLE_CANCEL_REQUEST,
	AP_GUILD_PACKET_TYPE_BATTLE_CANCEL_ACCEPT,
	AP_GUILD_PACKET_TYPE_BATTLE_CANCEL_REJECT,
	AP_GUILD_PACKET_TYPE_BATTLE_START,
	AP_GUILD_PACKET_TYPE_BATTLE_UPDATE_TIME,
	AP_GUILD_PACKET_TYPE_BATTLE_UPDATE_SCORE,
	AP_GUILD_PACKET_TYPE_BATTLE_WITHDRAW,
	AP_GUILD_PACKET_TYPE_BATTLE_RESULT,
	AP_GUILD_PACKET_TYPE_BATTLE_MEMBER,
	AP_GUILD_PACKET_TYPE_BATTLE_MEMBER_LIST,
	AP_GUILD_PACKET_TYPE_GUILDLIST,
	AP_GUILD_PACKET_TYPE_LEAVE_ALLOW,
	AP_GUILD_PACKET_TYPE_RENAME_GUILDID,
	AP_GUILD_PACKET_TYPE_RENAME_CHARACTERID,
	AP_GUILD_PACKET_TYPE_BUY_GUILDMARK,
	AP_GUILD_PACKET_TYPE_BUY_GUILDMARK_FORCE,
	AP_GUILD_PACKET_TYPE_JOINT_REQUEST,
	AP_GUILD_PACKET_TYPE_JOINT_REJECT,
	AP_GUILD_PACKET_TYPE_JOINT,
	AP_GUILD_PACKET_TYPE_JOINT_LEAVE,
	AP_GUILD_PACKET_TYPE_HOSTILE_REQUEST,
	AP_GUILD_PACKET_TYPE_HOSTILE_REJECT,
	AP_GUILD_PACKET_TYPE_HOSTILE,
	AP_GUILD_PACKET_TYPE_HOSTILE_LEAVE_REQUEST,
	AP_GUILD_PACKET_TYPE_HOSTILE_LEAVE_REJECT,
	AP_GUILD_PACKET_TYPE_HOSTILE_LEAVE,
	AP_GUILD_PACKET_TYPE_JOINT_DETAIL,
	AP_GUILD_PACKET_TYPE_HOSTILE_DETAIL,
	AP_GUILD_PACKET_TYPE_BATTLE_ROUND,
	AP_GUILD_PACKET_TYPE_WORLD_CHAMPIONSHIP,
	AP_GUILD_PACKET_TYPE_CLASS_SOCIETY_SYSTEM,
	AP_GUILD_PACKET_TYPE_COUNT
};

enum ap_guild_system_code {
	AP_GUILD_SYSTEM_CODE_EXIST_GUILD_ID = 0,
	AP_GUILD_SYSTEM_CODE_NEED_MORE_LEVEL,
	/** \brief Not enough gold.
	 * Requires num1 field to be set to the 
	 * amount of required gold. */
	AP_GUILD_SYSTEM_CODE_NEED_MORE_MONEY,
	/** \brief Do not have required item.
	 * Requires num1 field to be set to item tid. */
	AP_GUILD_SYSTEM_CODE_NEED_ITEM,
	AP_GUILD_SYSTEM_CODE_USE_SPECIAL_CHAR,
	AP_GUILD_SYSTEM_CODE_ALREADY_MEMBER,
	AP_GUILD_SYSTEM_CODE_MAX_MEMBER,
	AP_GUILD_SYSTEM_CODE_JOIN_FAIL,
	AP_GUILD_SYSTEM_CODE_LEAVE_FAIL,
	AP_GUILD_SYSTEM_CODE_NOT_MASTER,
	AP_GUILD_SYSTEM_CODE_INVALID_PASSWORD,
	AP_GUILD_SYSTEM_CODE_GUILD_CREATE_COMPLETE,
	AP_GUILD_SYSTEM_CODE_GUILD_DESTROY,
	/** \brief A new member has joined the guild.
	 * Requires string1 field to be set to member name. */
	AP_GUILD_SYSTEM_CODE_GUILD_JOIN,
	AP_GUILD_SYSTEM_CODE_GUILD_JOIN_REJECT,
	/** \brief A member has left the guild.
	 * Requires string1 field to be set to member name. */
	AP_GUILD_SYSTEM_CODE_GUILD_LEAVE,
	/** \brief A member has been kicked from guild.
	 * Requires string1 field to be set to member name. */
	AP_GUILD_SYSTEM_CODE_GUILD_FORCED_LEAVE,
	AP_GUILD_SYSTEM_CODE_ALREADY_MEMBER2,
	AP_GUILD_SYSTEM_CODE_GUILD_FORCED_LEAVE2,
	AP_GUILD_SYSTEM_CODE_BATTLE_BOTH_MEMBER_COUNT,
	/** \brief Not enough number of members.
	 * Requires num1 field to be set to the number of 
	 * required members. */
	AP_GUILD_SYSTEM_CODE_BATTLE_NOT_ENOUGH_MEMBER_COUNT,
	AP_GUILD_SYSTEM_CODE_BATTLE_NOT_ENOUGH_MEMBER_COUNT2,
	/** \brief Combined member level is too low.
	 * Requires num1 field to be set to required 
	 * combined level. */
	AP_GUILD_SYSTEM_CODE_BATTLE_NOT_ENOUGH_MEMBER_LEVEL,
	AP_GUILD_SYSTEM_CODE_BATTLE_NOT_ENOUGH_MEMBER_LEVEL2,
	AP_GUILD_SYSTEM_CODE_BATTLE_NOT_REQUEST_STATUS,
	AP_GUILD_SYSTEM_CODE_BATTLE_NOT_REQUEST_STATUS2,
	AP_GUILD_SYSTEM_CODE_BATTLE_ENEMY_MASTER_OFFLINE,
	AP_GUILD_SYSTEM_CODE_BATTLE_NOT_EXIST_GUILD,
	AP_GUILD_SYSTEM_CODE_BATTLE_REQUEST,
	AP_GUILD_SYSTEM_CODE_BATTLE_REJECT,
	AP_GUILD_SYSTEM_CODE_BATTLE_REJECT_BY_OTHER,
	/** \brief Cannot cancel so soon.
	 * Requires num1 field to be set to required buffer 
	 * time in seconds. */
	AP_GUILD_SYSTEM_CODE_BATTLE_CANCEL_PASSED_TIME,
	AP_GUILD_SYSTEM_CODE_BATTLE_CANCEL_REJECT,
	AP_GUILD_SYSTEM_CODE_BATTLE_CANCEL_REJECT_BY_OTHER,
	AP_GUILD_SYSTEM_CODE_BATTLE_CANCEL_ACCEPT,
	AP_GUILD_SYSTEM_CODE_BATTLE_NOT_WITHDRAW_UNTIL_START,
	AP_GUILD_SYSTEM_CODE_BATTLE_WITHDRAW,
	AP_GUILD_SYSTEM_CODE_BATTLE_WITHDRAW_BY_WIN,
	AP_GUILD_SYSTEM_CODE_BATTLE_WIN,
	AP_GUILD_SYSTEM_CODE_BATTLE_DRAW,
	AP_GUILD_SYSTEM_CODE_BATTLE_LOSE,
	AP_GUILD_SYSTEM_CODE_BATTLE_NOT_USABLE,
	AP_GUILD_SYSTEM_CODE_GUILD_JOIN_REFUSE,
	AP_GUILD_SYSTEM_CODE_BATTLE_REFUSE,
	AP_GUILD_SYSTEM_CODE_NOT_ENOUGHT_GHELD_FOR_INCREASE_MAX,
	AP_GUILD_SYSTEM_CODE_NOT_ENOUGHT_SKULL_FOR_INCREASE_MAX,
	AP_GUILD_SYSTEM_CODE_DESTROY_FAIL_TOO_EARLY,
	AP_GUILD_SYSTEM_CODE_NO_EXIST_SEARCH_GUILD,
	AP_GUILD_SYSTEM_CODE_JOIN_REQUEST_SELF,
	AP_GUILD_SYSTEM_CODE_JOIN_REQUEST_REJECT,
	AP_GUILD_SYSTEM_CODE_LEAVE_REQUEST,
	AP_GUILD_SYSTEM_CODE_RENAME_USE_SPECIAL_CHAR,
	AP_GUILD_SYSTEM_CODE_RENAME_EXIST_GUILD_ID,
	AP_GUILD_SYSTEM_CODE_RENAME_NOT_MASTER,
	AP_GUILD_SYSTEM_CODE_RENAME_IMPOSIBLE_GUILD_ID,
	AP_GUILD_SYSTEM_CODE_BUY_GUILDMARK_NO_EXIST_SKULL,
	AP_GUILD_SYSTEM_CODE_BUY_GUILDMARK_NOT_ENOUGHT_SKULL,
	AP_GUILD_SYSTEM_CODE_BUY_GUILDMARK_NOT_ENOUGHT_GHELD,
	AP_GUILD_SYSTEM_CODE_BUY_GUILDMARK_NO_EXIST_BOTTOM,
	AP_GUILD_SYSTEM_CODE_BUY_GUILDMARK_DUPLICATE,
	AP_GUILD_SYSTEM_CODE_JOINT_DISABLE,
	AP_GUILD_SYSTEM_CODE_JOINT_NOT_LEADER,
	AP_GUILD_SYSTEM_CODE_JOINT_ALREADY_JOINT,
	AP_GUILD_SYSTEM_CODE_JOINT_MAX,
	AP_GUILD_SYSTEM_CODE_JOINT_MASTER_OFFLINE,
	AP_GUILD_SYSTEM_CODE_JOINT_NOT_ENOUGH_MEMBER,
	AP_GUILD_SYSTEM_CODE_JOINT_OHTER_NOT_ENOUGH_MEMBER,
	AP_GUILD_SYSTEM_CODE_JOINT_WAIT,
	AP_GUILD_SYSTEM_CODE_JOINT_REJECT,
	AP_GUILD_SYSTEM_CODE_JOINT_FAILURE,
	AP_GUILD_SYSTEM_CODE_JOINT_SUCCESS,
	AP_GUILD_SYSTEM_CODE_JOINT_LEAVE_SUCCESS,
	AP_GUILD_SYSTEM_CODE_JOINT_LEAVE_OTHER_GUILD,
	AP_GUILD_SYSTEM_CODE_JOINT_DESTROY,
	AP_GUILD_SYSTEM_CODE_JOINT_SUCCESS2,
	AP_GUILD_SYSTEM_CODE_HOSTILE_DISABLE,
	AP_GUILD_SYSTEM_CODE_HOSTILE_ALREADY_HOSTILE,
	AP_GUILD_SYSTEM_CODE_HOSTILE_MAX,
	AP_GUILD_SYSTEM_CODE_HOSTILE_NOT_ENOUGH_MEMBER,
	AP_GUILD_SYSTEM_CODE_HOSTILE_MASTER_OFFLINE,
	AP_GUILD_SYSTEM_CODE_HOSTILE_OTHER_NOT_ENOUGH_MEMBER,
	AP_GUILD_SYSTEM_CODE_HOSTILE_WAIT,
	AP_GUILD_SYSTEM_CODE_HOSTILE_REJECT,
	AP_GUILD_SYSTEM_CODE_HOSTILE_FAILURE,
	AP_GUILD_SYSTEM_CODE_HOSTILE_SUCCESS,
	AP_GUILD_SYSTEM_CODE_HOSTILE_LEAVE_DISABLE,
	AP_GUILD_SYSTEM_CODE_HOSTILE_LEAVE_WAIT,
	AP_GUILD_SYSTEM_CODE_HOSTILE_LEAVE_REJECT,
	AP_GUILD_SYSTEM_CODE_HOSTILE_LEAVE_FAILURE,
	AP_GUILD_SYSTEM_CODE_HOSTILE_LEAVE_SUCCESS,
	AP_GUILD_SYSTEM_CODE_ARCHLORD_DEFENSE_SUCCESS,
	AP_GUILD_SYSTEM_CODE_ARCHLORD_DEFENSE_FAILURE,
	AP_GUILD_SYSTEM_CODE_ARCHLORD_ATTACK_SUCCESS,
	AP_GUILD_SYSTEM_CODE_ARCHLORD_ATTACK_FAILURE,
	AP_GUILD_SYSTEM_CODE_REFUSE_RELATION,
	AP_GUILD_SYSTEM_CODE_WAREHOUSE_NOT_ENOUGH_MEMBER,
	AP_GUILD_SYSTEM_CODE_GUILD_DESTROY_NOT_EMPTY_WAREHOUSE,
};

enum ap_guild_mode_data_index {
	AP_GUILD_MDI_GUILD = 0,
	AP_GUILD_MDI_MEMBER,
	AP_GUILD_MDI_BATTLE_POINT,
	AP_GUILD_MDI_GUILD_MARK_TEMPLATE,
};

struct ap_guild_member_battle_info {
	boolean in_battle;
	uint32_t score;
	uint32_t kill;
	uint32_t death;
	uint32_t sequence;
};

struct ap_guild_member {
	char name[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	enum ap_guild_member_rank rank;
	time_t join_date;
	uint32_t level;
	uint32_t tid;
	enum ap_guild_member_status status;
	struct ap_guild_member_battle_info battle_info;
};

struct ap_guild_battle {
	enum ap_guild_battle_type type;
	uint32_t count;
	char enemy_guild_id[AP_GUILD_MAX_ID_LENGTH + 1];
	char enemy_guild_master_name[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	uint64_t accept_tick;
	uint64_t ready_tick;
	uint64_t start_tick;
	uint64_t duration;
	uint64_t current_tick;
	uint32_t score;
	uint32_t enemy_score;
	uint32_t up_score;
	uint32_t enemy_up_score;
	uint32_t up_point;
	uint32_t enemy_up_point;
	enum ap_guild_battle_result result;
	uint32_t round;
};

struct ap_guild_relation_unit {
	char guild_id[AP_GUILD_MAX_ID_LENGTH + 1];
	time_t date;
	enum ap_guild_relation_type relation;
};

struct ap_guild_relation {
	struct ap_guild_relation_unit * joint;
	struct ap_guild_relation_unit * hostile;
	time_t last_joint_leave_date;
	char last_wait_guild_id[AP_GUILD_MAX_ID_LENGTH + 1];
};

struct ap_guild {
	char id[AP_GUILD_MAX_ID_LENGTH + 1];
	char master_name[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	char sub_master_name[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	char password[AP_GUILD_MAX_PASSWORD_LENGTH + 1];
	char notice[AP_GUILD_MAX_NOTICE_LENGTH + 1];
	enum ap_guild_rank rank;
	time_t creation_date;
	uint32_t max_member_count;
	uint32_t union_id;
	uint32_t page;
	uint32_t guild_mark_tid;
	uint32_t guild_mark_color;
	uint64_t last_refresh_tick;
	uint32_t total_battle_point;
	uint32_t archon_scroll_count;
	boolean remove;
	enum ap_guild_battle_rank battle_rank;
	struct ap_admin member_admin;
	struct ap_admin join_admin;
	struct ap_guild_relation relation;
	enum ap_guild_status status;
	uint32_t win;
	uint32_t draw;
	uint32_t lose;
	uint32_t point;
	struct ap_guild_battle battle_info;
	uint32_t invited_character_count;
	uint32_t invited_character_id[8];
};

struct ap_guild_character {
	struct ap_guild * guild;
	struct ap_guild_member * member;
	char guild_id[AP_GUILD_MAX_ID_LENGTH + 1];
	char join_guild_id[AP_GUILD_MAX_ID_LENGTH + 1];
	uint32_t guild_mark_tid;
	uint32_t guild_mark_color;
	enum ap_guild_battle_rank battle_rank;
};

struct ap_guild_system_message {
	uint32_t code;
	char * data_str[2];
	uint32_t data_u32[2];
} ;

struct ap_guild_battle_point {
	int32_t level_gap;
	uint32_t point;
};

struct ap_guild_require_item_increase_max_member {
	uint32_t max_member;
	uint32_t gold;
	char skull_name[AP_ITEM_MAX_NAME_LENGTH + 1];
	uint32_t skull_tid;
	uint32_t skull_count;
};

struct ap_guild_request_member {
	time_t leave_request_time;
	uint32_t level;
	uint32_t tid;
	char guild_id[AP_GUILD_MAX_ID_LENGTH + 1];
	char member_name[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	enum ap_guild_member_status status;
};

struct ap_guild_mark_template {
	char * name;
	uint32_t type_code;
	uint32_t tid;
	uint32_t index;
	uint32_t gold;
	uint32_t skull_tid;
	uint32_t skull_count;
	struct ap_grid_item * grid_item;
};

struct ap_guild_cb_receive {
	enum ap_guild_packet_type type;
	uint32_t character_id;
	char guild_id[AP_GUILD_MAX_ID_LENGTH + 1];
	char guild_password[AP_GUILD_MAX_PASSWORD_LENGTH + 1];
	char notice[AP_GUILD_MAX_NOTICE_LENGTH + 1];
	char member_name[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	uint32_t guild_mark_tid;
	uint32_t guild_mark_color;
	void * user_data;
};

struct ap_guild_module * ap_guild_create_module();

void ap_guild_add_callback(
	struct ap_guild_module * mod,
	enum ap_guild_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t ap_guild_attach_data(
	struct ap_guild_module * mod,
	enum ap_guild_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

struct ap_guild * ap_guild_add(struct ap_guild_module * mod, const char * guild_id);

void ap_guild_remove(struct ap_guild_module * mod, struct ap_guild * guild);

struct ap_guild_member * ap_guild_add_member(
	struct ap_guild_module * mod,
	struct ap_guild * guild,
	const char * member_name);

void ap_guild_remove_member(
	struct ap_guild_module * mod,
	struct ap_guild * guild,
	struct ap_guild_member * member);

struct ap_guild_character * ap_guild_get_character(
	struct ap_guild_module * mod,
	struct ap_character * character);

struct ap_guild * ap_guild_get(struct ap_guild_module * mod, const char * guild_id);

struct ap_guild * ap_guild_iterate(struct ap_guild_module * mod, size_t * index);

struct ap_guild * ap_guild_get_character_guild(
	struct ap_guild_module * mod,
	const char * character_name);

struct ap_guild_member * ap_guild_get_member(
	struct ap_guild_module * mod,
	struct ap_guild * guild,
	const char * character_name);

struct ap_guild_member * ap_guild_iterate_member(
	struct ap_guild_module * mod,
	struct ap_guild * guild,
	size_t * index);

boolean ap_guild_on_receive(
	struct ap_guild_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_guild_make_add_packet(struct ap_guild_module * mod, struct ap_guild * guild);

void ap_guild_make_join_request_packet(
	struct ap_guild_module * mod,
	const struct ap_guild * guild,
	const char * from,
	const char * to);

void ap_guild_make_join_packet(
	struct ap_guild_module * mod,
	const struct ap_guild * guild,
	const struct ap_guild_member * member);

void ap_guild_make_char_data_packet(
	struct ap_guild_module * mod,
	const struct ap_guild * guild,
	const struct ap_guild_member * member,
	uint32_t character_id);

void ap_guild_make_system_message_packet(
	struct ap_guild_module * mod,
	enum ap_guild_system_code code,
	const char * string1,
	const char * string2,
	uint32_t * num1,
	uint32_t * num2);

inline uint32_t ap_guild_get_member_count(struct ap_guild * guild)
{
	return ap_admin_get_object_count(&guild->member_admin);;
}

inline boolean ap_guild_is_full(struct ap_guild * guild)
{
	return (ap_guild_get_member_count(guild) >= 
		guild->max_member_count);
}

END_DECLS

#endif /* _AP_GUILD_H_ */