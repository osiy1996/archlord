#ifndef _AP_CHARACTER_H_
#define _AP_CHARACTER_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_base.h"
#include "public/ap_define.h"
#include "public/ap_factors.h"
#include "public/ap_module_instance.h"

#include <assert.h>

#define AP_CHARACTER_MODULE_NAME "AgpmCharacter"

#define AP_CHARACTER_MAX_LEVEL 99

#define AP_CHARACTER_MAX_INVENTORY_GOLD 1000000000
#define AP_CHARACTER_MAX_BANK_GOLD 1000000000

#define AP_CHARACTER_MAX_UNION 20
#define AP_CHARACTER_MAX_ATTACKER_LIST 20

#define AP_CHARACTER_MAX_MURDERER_POINT 99999
#define AP_CHARACTER_MURDERER_LEVEL0_POINT 0
#define AP_CHARACTER_MURDERER_LEVEL1_POINT 40
#define AP_CHARACTER_MURDERER_LEVEL2_POINT 60
#define AP_CHARACTER_MURDERER_LEVEL3_POINT 100
#define AP_CHARACTER_MURDERER_PRESERVE_TIME 7200
#define AP_CHARACTER_CRIMINAL_PRESERVE_TIME 20 * 60

#define AP_CHARACTER_LEVELUP_SKILL_POINT 3
#define AP_CHARACTER_MINIMUM_GET_HEROIC_POINT_LEVEL 90

#define AP_CHARACTER_DEFAULT_MOVE_SPEED 4000
#define AP_CHARACTER_MAX_COMBAT_MODE_TIME 10

#define AP_CHARACTER_MAX_TOWN_NAME 64

#define AP_CHARACTER_MAX_CHARISMA_POINT 1000000
#define AP_CHARACTER_CHARISMA_TYPE_MAX 13

#define AP_CHARACTER_SAVED_SPECIAL_STATUS_VOLUNTEER "V"

#define	AP_CHARACTER_PROCESS_REMOVE_INTERVAL 3000
#define	AP_CHARACTER_PRESERVE_CHARACTER_DATA 5000

#define AP_CHARACTER_MOVE_FLAG_DIRECTION 0x01
#define AP_CHARACTER_MOVE_FLAG_PATHFINDING 0x02
#define AP_CHARACTER_MOVE_FLAG_STOP 0x04
#define AP_CHARACTER_MOVE_FLAG_SYNC 0x08
#define AP_CHARACTER_MOVE_FLAG_FAST 0x10
#define AP_CHARACTER_MOVE_FLAG_HORIZONTAL 0x20
#define	AP_CHARACTER_MOVE_FLAG_FOLLOW 0x40
#define AP_CHARACTER_MOVE_FLAG_CANCEL_ACTION 0x80

#define AP_CHARACTER_FLAG_TRANSFORM 0x01
#define AP_CHARACTER_FLAG_RIDABLE 0x02
#define AP_CHARACTER_FLAG_EVOLUTION 0x04

#define AP_CHARACTER_MAX_CHARACTER_TITLE 32
#define AP_CHARACTER_MAX_CHARACTER_SKILL_INIT 32
#define AP_CHARACTER_MAX_NICKNAME_SIZE 16
#define AP_CHARACTER_INVENTORY_NUM 3
#define AP_CHARACTER_INVENTORY_ROW1 10
#define AP_CHARACTER_INVENTORY_ROW2 7
#define AP_CHARACTER_CHARACTER_DATA_COUNT 1000
#define AP_CHARACTER_BTREE_ORDER 20
#define AP_CHARACTER_MAX_NAME_LENGTH 48
#define AP_CHARACTER_MAX_TEMPLATE_ID 8500
#define AP_CHARACTER_MAX_TEMPLATE_NAME 32
#define AP_CHARACTER_TEMPLATE_DATA_COUNT 100
#define AP_CHARACTER_BTREE_ORDER 20

#define AP_CHARACTER_MAX_EQUIP_ITEMLIST 10
#define AP_CHARACTER_MAX_DROP_ITEMLIST 10

#define AP_CHARACTER_ACTION_DATA_SIZE 128

enum agpm_character_template_race_type {
	AGPMCHARACTER_TYPE_HUMAN	= 1,
	AGPMCHARACTER_TYPE_ORC		= 2,
	AGPMCHARACTER_TYPE_NPC		= 3,
	AGPMCHARACTER_TYPE_MONSTER	= 4
};


static const uint32_t AP_CHARACTER_PC_TID[] = {
	96, 1, 6, 4, 8, 3, 9, 460, 377,
	0, 0, 0, 1722, 1723, 1724, 1732 };
#define AP_CHARACTER_PC_TID_COUNT COUNT_OF(AP_CHARACTER_PC_TID)

#define AP_CHARACTER_SPECIAL_STATUS_NORMAL 0x0000000000000000u
#define AP_CHARACTER_SPECIAL_STATUS_STUN 0x0000000000000001u
#define AP_CHARACTER_SPECIAL_STATUS_FREEZE 0x0000000000000002u
#define AP_CHARACTER_SPECIAL_STATUS_SLOW 0x0000000000000004u
#define AP_CHARACTER_SPECIAL_STATUS_INVINCIBLE 0x0000000000000008u
#define AP_CHARACTER_SPECIAL_STATUS_ATTRIBUTE_INVINCIBLE 0x0000000000000010u
#define AP_CHARACTER_SPECIAL_STATUS_NOT_ADD_AGRO 0x0000000000000020u
#define AP_CHARACTER_SPECIAL_STATUS_HIDE_AGRO 0x0000000000000040u
#define AP_CHARACTER_SPECIAL_STATUS_STUN_PROTECT 0x0000000000000080u
#define AP_CHARACTER_SPECIAL_STATUS_PVP_INVINCIBLE 0x0000000000000100u
#define AP_CHARACTER_SPECIAL_STATUS_TRANSPARENT 0x0000000000000200u
#define AP_CHARACTER_SPECIAL_STATUS_DONT_MOVE 0x0000000000000400u
#define AP_CHARACTER_SPECIAL_STATUS_SUMMONER 0x0000000000000800u
#define AP_CHARACTER_SPECIAL_STATUS_TAME 0x0000000000001000u
#define AP_CHARACTER_SPECIAL_STATUS_HALT 0x0000000000002000u
#define AP_CHARACTER_SPECIAL_STATUS_WAS_TAMED 0x0000000000004000u
#define AP_CHARACTER_SPECIAL_STATUS_COUNSEL 0x0000000000008000u
#define AP_CHARACTER_SPECIAL_STATUS_EVENT_GIFTBOX 0x0000000000010000u
#define AP_CHARACTER_SPECIAL_STATUS_ADMIN_SPAWNED_CHAR 0x0000000000020000u
#define AP_CHARACTER_SPECIAL_STATUS_DISABLE 0x0000000000040000u
#define AP_CHARACTER_SPECIAL_STATUS_USE 0x0000000000080000u
#define AP_CHARACTER_SPECIAL_STATUS_REPAIR 0x0000000000100000u
#define AP_CHARACTER_SPECIAL_STATUS_HALF_TRANSPARENT 0x0000000000200000u
#define AP_CHARACTER_SPECIAL_STATUS_HUMAN_CASTLE_OWNER 0x0000000000400000u
#define AP_CHARACTER_SPECIAL_STATUS_ORC_CASTLE_OWNER 0x0000000001000000u
#define AP_CHARACTER_SPECIAL_STATUS_MOONELF_CASTLE_OWNER 0x0000000002000000u
#define AP_CHARACTER_SPECIAL_STATUS_ARCHLORD_CASTLE_OWNER 0x0000000004000000u
#define AP_CHARACTER_SPECIAL_STATUS_DROP_RATE_100 0x0000000008000000u
#define AP_CHARACTER_SPECIAL_STATUS_RIDE_ARCADIA 0x0000000010000000u
#define AP_CHARACTER_SPECIAL_STATUS_NORMAL_ATK_INVINCIBLE 0x0000000020000000u
#define AP_CHARACTER_SPECIAL_STATUS_SKILL_ATK_INVINCIBLE 0x0000000040000000u
#define AP_CHARACTER_SPECIAL_STATUS_DISABLE_SKILL 0x0000000080000000u
#define AP_CHARACTER_SPECIAL_STATUS_DISABLE_NORMAL_ATK 0x0000000100000000u
#define AP_CHARACTER_SPECIAL_STATUS_SLEEP 0x0000000200000000u
#define AP_CHARACTER_SPECIAL_STATUS_SLOW_INVINCIBLE 0x0000000400000000u
#define AP_CHARACTER_SPECIAL_STATUS_GO 0x0000000800000000u
#define AP_CHARACTER_SPECIAL_STATUS_LEVELLIMIT 0x0000001000000000u
#define AP_CHARACTER_SPECIAL_STATUS_DISARMAMENT 0x0000002000000000u
#define AP_CHARACTER_SPECIAL_STATUS_DRAGONSCION_CASTLE_OWNER 0x0000004000000000u
#define AP_CHARACTER_SPECIAL_STATUS_DISABLE_CHATTING 0x0000008000000000u
#define AP_CHARACTER_SPECIAL_STATUS_HOLD 0x0000010000000000u
#define AP_CHARACTER_SPECIAL_STATUS_CONFUSION 0x0000020000000000u
#define AP_CHARACTER_SPECIAL_STATUS_FEAR 0x0000040000000000u
#define AP_CHARACTER_SPECIAL_STATUS_BERSERK 0x0000080000000000u
#define AP_CHARACTER_SPECIAL_STATUS_SHRINK 0x0000100000000000u
#define AP_CHARACTER_SPECIAL_STATUS_DISEASE 0x0000200000000000u
#define AP_CHARACTER_SPECIAL_STATUS_VOLUNTEER 0x0000400000000000u
#define AP_CHARACTER_SPECIAL_STATUS_FLAG_BLUE 0x0000800000000000u
#define AP_CHARACTER_SPECIAL_STATUS_FLAG_RED 0x0001000000000000u
#define AP_CHARACTER_SPECIAL_STATUS_BEST_OF_CLASS 0x0002000000000000u
#define AP_CHARACTER_SPECIAL_STATUS_NONE 0x8000000000000000u

#define AP_CHARACTER_BIT_INVENTORY_GOLD  (AP_FACTORS_BIT_END << 0)
#define AP_CHARACTER_BIT_BANK_GOLD       (AP_FACTORS_BIT_END << 1)
#define AP_CHARACTER_BIT_ACTION_STATUS   (AP_FACTORS_BIT_END << 2)
#define AP_CHARACTER_BIT_SPECIAL_STATUS  (AP_FACTORS_BIT_END << 3)
#define AP_CHARACTER_BIT_CRIMINAL_STATUS (AP_FACTORS_BIT_END << 4)
#define AP_CHARACTER_BIT_POSITION        (AP_FACTORS_BIT_END << 5)
#define AP_CHARACTER_BIT_CHANTRA_COINS   (AP_FACTORS_BIT_END << 6)

BEGIN_DECLS

struct ap_character_module;

enum ap_character_type {
	AP_CHARACTER_TYPE_NONE = 0x00000000,
	AP_CHARACTER_TYPE_PC = 0x00000001,
	AP_CHARACTER_TYPE_NPC = 0x00000002,
	AP_CHARACTER_TYPE_MONSTER = 0x00000004,
	AP_CHARACTER_TYPE_CREATURE = 0x00000008,
	AP_CHARACTER_TYPE_GUARD = 0x00000010,
	AP_CHARACTER_TYPE_GM = 0x00000020,
	AP_CHARACTER_TYPE_ATTACKABLE = 0x00010000,
	AP_CHARACTER_TYPE_TARGETABLE = 0x00020000,
	AP_CHARACTER_TYPE_MOVABLE_NPC = 0x00040000,
	AP_CHARACTER_TYPE_POLYMORPH = 0x00080000,
	AP_CHARACTER_TYPE_TRAP = 0x00100000,
	AP_CHARACTER_TYPE_SUMMON = 0x00200000,
};

enum ap_character_login_status {
	AP_CHARACTER_STATUS_LOGOUT = 0,
	AP_CHARACTER_STATUS_IN_LOGIN_PROCESS,
	AP_CHARACTER_STATUS_IN_GAME_WORLD,
	AP_CHARACTER_STATUS_RETURN_TO_LOGIN_SERVER,
};

enum ap_character_action_status {
	AP_CHARACTER_ACTION_STATUS_NORMAL = 0,
	AP_CHARACTER_ACTION_STATUS_PREDEAD,
	AP_CHARACTER_ACTION_STATUS_DEAD,
	AP_CHARACTER_ACTION_STATUS_NOT_ACTION,
	AP_CHARACTER_ACTION_STATUS_MOVE,
	AP_CHARACTER_ACTION_STATUS_ATTACK,
	AP_CHARACTER_ACTION_STATUS_READY_SKILL,
	AP_CHARACTER_ACTION_STATUS_TRADE,
	AP_CHARACTER_ACTION_STATUS_PARTY,
};

enum ap_character_criminal_status {
	AP_CHARACTER_CRIMINAL_STATUS_INNOCENT = 0,
	AP_CHARACTER_CRIMINAL_STATUS_CRIMINAL_FLAGGED,
};

enum ap_character_action_type {
	AP_CHARACTER_ACTION_TYPE_NONE = 0,
	AP_CHARACTER_ACTION_TYPE_ATTACK,
	AP_CHARACTER_ACTION_TYPE_ATTACK_MISS,
	AP_CHARACTER_ACTION_TYPE_SKILL,
	AP_CHARACTER_ACTION_TYPE_PICKUP_ITEM,
	AP_CHARACTER_ACTION_TYPE_EVENT_TELEPORT,
	AP_CHARACTER_ACTION_TYPE_EVENT_NPC_TRADE,
	AP_CHARACTER_ACTION_TYPE_EVENT_MASTERY_SPECIALIZE,
	AP_CHARACTER_ACTION_TYPE_EVENT_BANK,
	AP_CHARACTER_ACTION_TYPE_EVENT_ITEMCONVERT,
	AP_CHARACTER_ACTION_TYPE_EVENT_GUILD,
	AP_CHARACTER_ACTION_TYPE_EVENT_PRODUCT,
	AP_CHARACTER_ACTION_TYPE_EVENT_NPC_DIALOG,
	AP_CHARACTER_ACTION_TYPE_EVENT_SKILL_MASTER,
	AP_CHARACTER_ACTION_TYPE_EVENT_REFINERY,
	AP_CHARACTER_ACTION_TYPE_EVENT_QUEST,
	AP_CHARACTER_ACTION_TYPE_MOVE,
	AP_CHARACTER_ACTION_TYPE_PRODUCT_SKILL,
	AP_CHARACTER_ACTION_TYPE_EVENT_CHARCUSTOMIZE,
	AP_CHARACTER_ACTION_TYPE_EVENT_ITEMREPAIR,
	AP_CHARACTER_ACTION_TYPE_EVENT_REMISSION,
	AP_CHARACTER_ACTION_TYPE_EVENT_WANTEDCRIMINAL,
	AP_CHARACTER_ACTION_TYPE_EVENT_SIEGE_WAR,
	AP_CHARACTER_ACTION_TYPE_EVENT_TAX,
	AP_CHARACTER_ACTION_TYPE_USE_SIEGEWAR_ATTACK_OBJECT,
	AP_CHARACTER_ACTION_TYPE_CARVE_A_SEAL,
	AP_CHARACTER_ACTION_TYPE_EVENT_GUILD_WAREHOUSE,
	AP_CHARACTER_ACTION_TYPE_USE_EFFECT,
	AP_CHARACTER_ACTION_TYPE_EVENT_ARCHLORD,
	AP_CHARACTER_ACTION_TYPE_EVENT_GAMBLE,
	AP_CHARACTER_ACTION_TYPE_EVENT_GACHA,
	AP_CHARACTER_ACTION_TYPE_COUNT
};

enum ap_character_action_result_type {
	AP_CHARACTER_ACTION_RESULT_TYPE_NONE = 0,
	AP_CHARACTER_ACTION_RESULT_TYPE_MISS,
	AP_CHARACTER_ACTION_RESULT_TYPE_NOT_ENOUGH_ARROW,
	AP_CHARACTER_ACTION_RESULT_TYPE_NOT_ENOUGH_BOLT,
	AP_CHARACTER_ACTION_RESULT_TYPE_NOT_ENOUGH_MP,
	AP_CHARACTER_ACTION_RESULT_TYPE_SUCCESS,
	AP_CHARACTER_ACTION_RESULT_TYPE_CRITICAL,
	AP_CHARACTER_ACTION_RESULT_TYPE_EVADE,
	AP_CHARACTER_ACTION_RESULT_TYPE_DODGE,
	AP_CHARACTER_ACTION_RESULT_TYPE_BLOCK,
	AP_CHARACTER_ACTION_RESULT_TYPE_REFLECT,
	AP_CHARACTER_ACTION_RESULT_TYPE_ABSORB,
	AP_CHARACTER_ACTION_RESULT_TYPE_COUNTER_ATTACK,
	AP_CHARACTER_ACTION_RESULT_TYPE_DEATH_STRIKE,
	AP_CHARACTER_ACTION_RESULT_TYPE_CAST_SKILL,
	AP_CHARACTER_ACTION_RESULT_TYPE_REFLECT_DAMAGE,
	AP_CHARACTER_ACTION_RESULT_TYPE_REDUCE_DAMAGE,
	AP_CHARACTER_ACTION_RESULT_TYPE_CONVERT_DAMAGE,
	AP_CHARACTER_ACTION_RESULT_TYPE_CHARGE,
};

enum ap_charcter_action_block_type {
	AP_CHARACTER_ACTION_BLOCK_TYPE_ALL = 0x00000001,
	AP_CHARACTER_ACTION_BLOCK_TYPE_ATTACK = 0x00000002,
	AP_CHARACTER_ACTION_BLOCK_TYPE_SKILL = 0x00000004,
};

enum ap_character_agro_type {
	AP_CHARACTER_AGRO_TYPE_NORMAL = 0x00000000,
	AP_CHARACTER_AGRO_TYPE_NOT_ADD = 0x00000001,
	AP_CHARACTER_AGRO_TYPE_HIDE = 0x00000002,
};

enum ap_character_social_action_type {
	AP_CHARACTER_SOCIAL_ACTION_GREETING = 0,
	AP_CHARACTER_SOCIAL_ACTION_CELEBRATION,
	AP_CHARACTER_SOCIAL_ACTION_GRATITUDE,
	AP_CHARACTER_SOCIAL_ACTION_ENCOURAGEMENT,
	AP_CHARACTER_SOCIAL_ACTION_DISREGARD,
	AP_CHARACTER_SOCIAL_ACTION_DANCING,
	AP_CHARACTER_SOCIAL_ACTION_DOZINESS,
	AP_CHARACTER_SOCIAL_ACTION_STRETCH,
	AP_CHARACTER_SOCIAL_ACTION_LAUGH,
	AP_CHARACTER_SOCIAL_ACTION_WEEPING,
	AP_CHARACTER_SOCIAL_ACTION_RAGE,
	AP_CHARACTER_SOCIAL_ACTION_POUT,
	AP_CHARACTER_SOCIAL_ACTION_APOLOGY,
	AP_CHARACTER_SOCIAL_ACTION_TOAST,
	AP_CHARACTER_SOCIAL_ACTION_CHEER,
	AP_CHARACTER_SOCIAL_ACTION_RUSH,
	AP_CHARACTER_SOCIAL_ACTION_SIT,
	AP_CHARACTER_SOCIAL_ACTION_SPECIAL1,
	AP_CHARACTER_SOCIAL_ACTION_SELECT1,
	AP_CHARACTER_SOCIAL_ACTION_SELECT2,
	AP_CHARACTER_SOCIAL_ACTION_SELECT3,
	AP_CHARACTER_SOCIAL_ACTION_SELECT4,
	AP_CHARACTER_SOCIAL_ACTION_SELECT5,
	AP_CHARACTER_SOCIAL_ACTION_SELECT6,
	AP_CHARACTER_SOCIAL_ACTION_SELECT7,
	AP_CHARACTER_SOCIAL_ACTION_SELECT8,
	AP_CHARACTER_SOCIAL_ACTION_SELECT9,
	AP_CHARACTER_SOCIAL_ACTION_SELECT10,
	AP_CHARACTER_SOCIAL_ACTION_SELECT1_BACK,
	AP_CHARACTER_SOCIAL_ACTION_SELECT2_BACK,
	AP_CHARACTER_SOCIAL_ACTION_SELECT3_BACK,
	AP_CHARACTER_SOCIAL_ACTION_SELECT4_BACK,
	AP_CHARACTER_SOCIAL_ACTION_SELECT5_BACK,
	AP_CHARACTER_SOCIAL_ACTION_SELECT6_BACK,
	AP_CHARACTER_SOCIAL_ACTION_SELECT7_BACK,
	AP_CHARACTER_SOCIAL_ACTION_SELECT8_BACK,
	AP_CHARACTER_SOCIAL_ACTION_SELECT9_BACK,
	AP_CHARACTER_SOCIAL_ACTION_SELECT10_BACK,
	AP_CHARACTER_SOCIAL_ACTION_GM_GREETING,
	AP_CHARACTER_SOCIAL_ACTION_GM_CELEBRATION,
	AP_CHARACTER_SOCIAL_ACTION_GM_DANCING,
	AP_CHARACTER_SOCIAL_ACTION_GM_WEEPING,
	AP_CHARACTER_SOCIAL_ACTION_GM_TOAST,
	AP_CHARACTER_SOCIAL_ACTION_GM_CHEER,
	AP_CHARACTER_SOCIAL_ACTION_GM_DEEPBOW,
	AP_CHARACTER_SOCIAL_ACTION_GM_HI,
	AP_CHARACTER_SOCIAL_ACTION_GM_WAIT,
	AP_CHARACTER_SOCIAL_ACTION_GM_HAPPY,
	AP_CHARACTER_SOCIAL_ACTION_COUNT
};

enum ap_character_option_flag_bits {
	AP_CHARACTER_OPTION_REFUSE_TRADE = 0x00000001,
	AP_CHARACTER_OPTION_REFUSE_PARTY_IN = 0x00000002,
	AP_CHARACTER_OPTION_REFUSE_GUILD_BATTLE = 0x00000004,
	AP_CHARACTER_OPTION_REFUSE_BATTLE = 0x00000008,
	AP_CHARACTER_OPTION_REFUSE_GUILD_IN = 0x00000010,
	AP_CHARACTER_OPTION_REFUSE_GUILD_RELATION = 0x00000040,
	AP_CHARACTER_OPTION_REFUSE_BUDDY = 0x00000080,
	AP_CHARACTER_OPTION_AUTO_PICKUP_ONOFF = 0x00000100,
};

enum ap_character_tamable_type {
	AP_CHARACTER_TAMABLE_TYPE_NONE = 0,
	AP_CHARACTER_TAMABLE_TYPE_BY_FORMULA,
};

enum ap_character_range_type {
	AP_CHARACTER_RANGE_TYPE_MELEE = 0,
	AP_CHARACTER_RANGE_TYPE_RANGE,
};

enum ap_character_additional_effect {
	AP_CHARACTER_ADDITIONAL_EFFECT_NONE = 0x00000000,
	AP_CHARACTER_ADDITIONAL_EFFECT_END_BUFF_EXPLOSION = 0x00000001,
	AP_CHARACTER_ADDITIONAL_EFFECT_TELEPORT = 0x00000002,
	AP_CHARACTER_ADDITIONAL_EFFECT_ABSORB_MP = 0x00000004,
	AP_CHARACTER_ADDITIONAL_EFFECT_ABSORB_HP = 0x00000008,
	AP_CHARACTER_ADDITIONAL_EFFECT_CONVERT_MP = 0x00000010,
	AP_CHARACTER_ADDITIONAL_EFFECT_CONVERT_HP = 0x00000020,
	AP_CHARACTER_ADDITIONAL_EFFECT_RELEASE_TARGET = 0x00000040,
	AP_CHARACTER_ADDITIONAL_EFFECT_LENS_STONE = 0x00000080,
	AP_CHARACTER_ADDITIONAL_EFFECT_FIRECRACKER = 0x00000100,
	AP_CHARACTER_ADDITIONAL_EFFECT_CHARISMA_POINT = 0x00000200,
};

enum ap_character_disturb_action {
	AP_CHARACTER_DISTURB_ACTION_NONE = 0x00,
	AP_CHARACTER_DISTURB_ACTION_ATTACK = 0x01,
	AP_CHARACTER_DISTURB_ACTION_MOVE = 0x02,
	AP_CHARACTER_DISTURB_ACTION_USE_ITEM = 0x04,
	AP_CHARACTER_DISTURB_ACTION_SKILL = 0x08
};

enum ap_character_packet_type {
	AP_CHARACTER_PACKET_TYPE_ADD = 0,
	AP_CHARACTER_PACKET_TYPE_UPDATE,
	AP_CHARACTER_PACKET_TYPE_REMOVE,
	AP_CHARACTER_PACKET_TYPE_REMOVE_FOR_VIEW,
	AP_CHARACTER_PACKET_TYPE_SELECT,
	AP_CHARACTER_PACKET_TYPE_LEVEL_UP,
	AP_CHARACTER_PACKET_TYPE_ADD_ATTACKER,
	AP_CHARACTER_PACKET_TYPE_MOVE_BANKMONEY,
	AP_CHARACTER_PACKET_TYPE_DISCONNECT_BY_ANOTHER_USER,
	AP_CHARACTER_PACKET_TYPE_REQUEST_RESURRECTION_TOWN,
	AP_CHARACTER_PACKET_TYPE_REQUEST_RESURRECTION_NOW,
	AP_CHARACTER_PACKET_TYPE_TRANSFORM,
	AP_CHARACTER_PACKET_TYPE_RESTORE_TRANSFORM,
	AP_CHARACTER_PACKET_TYPE_CANCEL_TRANSFORM,
	AP_CHARACTER_PACKET_TYPE_RIDABLE,
	AP_CHARACTER_PACKET_TYPE_RESTORE_RIDABLE,
	AP_CHARACTER_PACKET_TYPE_SOCIAL_ANIMATION,
	AP_CHARACTER_PACKET_TYPE_OPTION_UPDATE,
	AP_CHARACTER_PACKET_TYPE_BLOCK_BY_PENALTY,
	AP_CHARACTER_PACKET_TYPE_REQUEST_RESURRECTION_SIEGE_INNER,
	AP_CHARACTER_PACKET_TYPE_REQUEST_RESURRECTION_SIEGE_OUTER,
	AP_CHARACTER_PACKET_TYPE_ONLINE_TIME,
	AP_CHARACTER_PACKET_TYPE_NPROTECT_AUTH,
	AP_CHARACTER_PACKET_TYPE_UPDATE_SKILLINIT_STRING,
	AP_CHARACTER_PACKET_TYPE_RESURRECTION_BY_OTHER,
	AP_CHARACTER_PACKET_TYPE_EVENT_EFFECT_ID,
	AP_CHARACTER_PACKET_TYPE_EVOLUTION,
	AP_CHARACTER_PACKET_TYPE_RESTORE_EVOLUTION,
	AP_CHARACTER_PACKET_TYPE_REQUEST_PINCHWANTED_CHARACTER,
	AP_CHARACTER_PACKET_OPERATUIN_ANSWER_PINCHWANTED_CHARACTER,
	AP_CHARACTER_PACKET_TYPE_REQUEST_COUPON,
	AP_CHARACTER_PACKET_TYPE_COUPON_INFO,
	AP_CHARACTER_PACKET_TYPE_REQUEST_COUPON_USE,
	AP_CHARACTER_PACKET_TYPE_ANSWER_COUPON_USE,
	AP_CHARACTER_PACKET_TYPE_UPDATE_GAMEPLAY_OPTION = 80,
};

enum ac_character_packet_type {
	AC_CHARACTER_PACKET_REQUEST_ADDCHAR = 0,
	AC_CHARACTER_PACKET_REQUEST_ADDCHAR_RESULT,
	AC_CHARACTER_PACKET_REQUEST_CLIENT_CONNECT,
	AC_CHARACTER_PACKET_REQUEST_CLIENT_CONNECT_RESULT,
	AC_CHARACTER_PACKET_SEND_COMPLETE,
	AC_CHARACTER_PACKET_SEND_INCOMPLETE,
	AC_CHARACTER_PACKET_SEND_CANCEL,
	AC_CHARACTER_PACKET_SETTING_CHARACTER_OK,
	AC_CHARACTER_PACKET_CHARACTER_POSITION,
	AC_CHARACTER_PACKET_LOADING_COMPLETE,
	AC_CHARACTER_PACKET_DELETE_COMPLETE,
	AC_CHARACTER_PACKET_AUTH_KEY,
};

enum ap_character_module_data_index {
	AP_CHARACTER_MDI_CHAR = 0,
	AP_CHARACTER_MDI_TEMPLATE,
	AP_CHARACTER_MDI_STATIC,
	AP_CHARACTER_MDI_CHAR_KIND,
	AP_CHARACTER_MDI_STATIC_ONE,
	AP_CHARACTER_MDI_COUNT
};

enum ap_character_import_data_column_id {
	AP_CHARACTER_IMPORT_DCID_TID,
	AP_CHARACTER_IMPORT_DCID_NAME,
	AP_CHARACTER_IMPORT_DCID_TYPE,
	AP_CHARACTER_IMPORT_DCID_RACE,
	AP_CHARACTER_IMPORT_DCID_GENDER,
	AP_CHARACTER_IMPORT_DCID_CLASS,
	AP_CHARACTER_IMPORT_DCID_LVL,
	AP_CHARACTER_IMPORT_DCID_STR,
	AP_CHARACTER_IMPORT_DCID_DEX,
	AP_CHARACTER_IMPORT_DCID_INT,
	AP_CHARACTER_IMPORT_DCID_CON,
	AP_CHARACTER_IMPORT_DCID_CHA,
	AP_CHARACTER_IMPORT_DCID_WIS,
	AP_CHARACTER_IMPORT_DCID_AC,
	AP_CHARACTER_IMPORT_DCID_MAX_HP,
	AP_CHARACTER_IMPORT_DCID_MAX_SP,
	AP_CHARACTER_IMPORT_DCID_MAX_MP,
	AP_CHARACTER_IMPORT_DCID_CUR_HP,
	AP_CHARACTER_IMPORT_DCID_CUR_SP,
	AP_CHARACTER_IMPORT_DCID_CUR_MP,
	AP_CHARACTER_IMPORT_DCID_MOVEMENT_WALK,
	AP_CHARACTER_IMPORT_DCID_MOVEMENT_RUN,
	AP_CHARACTER_IMPORT_DCID_AR,
	AP_CHARACTER_IMPORT_DCID_DR,
	AP_CHARACTER_IMPORT_DCID_MAR,
	AP_CHARACTER_IMPORT_DCID_MDR,
	AP_CHARACTER_IMPORT_DCID_MIN_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_MAX_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_ATK_SPEED,
	AP_CHARACTER_IMPORT_DCID_RANGE,
	AP_CHARACTER_IMPORT_DCID_FIRE_RES,
	AP_CHARACTER_IMPORT_DCID_WATER_RES,
	AP_CHARACTER_IMPORT_DCID_AIR_RES,
	AP_CHARACTER_IMPORT_DCID_EARTH_RES,
	AP_CHARACTER_IMPORT_DCID_MAGIC_RES,
	AP_CHARACTER_IMPORT_DCID_POISON_RES,
	AP_CHARACTER_IMPORT_DCID_ICE_RES,
	AP_CHARACTER_IMPORT_DCID_THUNDER_RES,
	AP_CHARACTER_IMPORT_DCID_MAGIC_HISTORY,
	AP_CHARACTER_IMPORT_DCID_MURDER_POINT,
	AP_CHARACTER_IMPORT_DCID_EXP,
	AP_CHARACTER_IMPORT_DCID_FIRE_MIN_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_FIRE_MAX_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_WATER_MIN_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_WATER_MAX_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_EARTH_MIN_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_EARTH_MAX_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_AIR_MIN_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_AIR_MAX_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_MAGIC_MIN_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_MAGIC_MAX_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_POISON_MIN_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_POISON_MAX_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_ICE_MIN_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_ICE_MAX_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_THUNDER_MIN_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_THUNDER_MAX_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_PHYSICAL_RESISTANCE,
	AP_CHARACTER_IMPORT_DCID_SKILL_BLOCK,
	AP_CHARACTER_IMPORT_DCID_SIGHT,
	AP_CHARACTER_IMPORT_DCID_BADGE1_RANK_POINT,
	AP_CHARACTER_IMPORT_DCID_BADGE2_RANK_POINT,
	AP_CHARACTER_IMPORT_DCID_BADGE3_RANK_POINT,
	AP_CHARACTER_IMPORT_DCID_RANK,
	AP_CHARACTER_IMPORT_DCID_ITEM_POINT,
	AP_CHARACTER_IMPORT_DCID_GUILD,
	AP_CHARACTER_IMPORT_DCID_GUILD_RANK,
	AP_CHARACTER_IMPORT_DCID_GUILD_SCORE,
	AP_CHARACTER_IMPORT_DCID_LOST_EXP,
	AP_CHARACTER_IMPORT_DCID_WEAPON,
	AP_CHARACTER_IMPORT_DCID_ITEM1,
	AP_CHARACTER_IMPORT_DCID_ITEM2,
	AP_CHARACTER_IMPORT_DCID_ITEM3,
	AP_CHARACTER_IMPORT_DCID_ITEM4,
	AP_CHARACTER_IMPORT_DCID_ARMOR_HEAD,
	AP_CHARACTER_IMPORT_DCID_ARMOR_BODY,
	AP_CHARACTER_IMPORT_DCID_ARMOR_ARM,
	AP_CHARACTER_IMPORT_DCID_ARMOR_HAND,
	AP_CHARACTER_IMPORT_DCID_ARMOR_LEG,
	AP_CHARACTER_IMPORT_DCID_ARMOR_FOOT,
	AP_CHARACTER_IMPORT_DCID_ARMOR_MANTLE,
	AP_CHARACTER_IMPORT_DCID_MONEY,
	AP_CHARACTER_IMPORT_DCID_GELD_MIN,
	AP_CHARACTER_IMPORT_DCID_GELD_MAX,
	AP_CHARACTER_IMPORT_DCID_DROP_TID,
	AP_CHARACTER_IMPORT_DCID_SLOW_PERCENT,
	AP_CHARACTER_IMPORT_DCID_SLOW_TIME,
	AP_CHARACTER_IMPORT_DCID_FAST_PERCENT,
	AP_CHARACTER_IMPORT_DCID_FAST_TIME,
	AP_CHARACTER_IMPORT_DCID_PRODUCT_SKIN_ITEMNAME,
	AP_CHARACTER_IMPORT_DCID_PRODUCT_MEAT,
	AP_CHARACTER_IMPORT_DCID_PRODUCT_MEAT_ITEMNAME,
	AP_CHARACTER_IMPORT_DCID_PRODUCT_GARBAGE,
	AP_CHARACTER_IMPORT_DCID_PRODUCT_GARBAGE_ITMENAME,
	AP_CHARACTER_IMPORT_DCID_ATTRIBUTE_TYPE,
	AP_CHARACTER_IMPORT_DCID_NAME_COLOR,
	AP_CHARACTER_IMPORT_DCID_DROPRANK,
	AP_CHARACTER_IMPORT_DCID_DROPMEDITATION,
	AP_CHARACTER_IMPORT_DCID_SPIRIT_TYPE,
	AP_CHARACTER_IMPORT_DCID_LANDATTACHTYPE,
	AP_CHARACTER_IMPORT_DCID_TAMABLE,
	AP_CHARACTER_IMPORT_DCID_RUN_CORRECT,
	AP_CHARACTER_IMPORT_DCID_ATTACK_CORRECT,
	AP_CHARACTER_IMPORT_DCID_SIEGEWARCHARTYPE,
	AP_CHARACTER_IMPORT_DCID_RANGETYPE,
	AP_CHARACTER_IMPORT_DCID_STAMINAPOINT,
	AP_CHARACTER_IMPORT_DCID_PETTYPE,
	AP_CHARACTER_IMPORT_DCID_STARTSTAMINA,
	AP_CHARACTER_IMPORT_DCID_HEROIC_MIN_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_HEROIC_MAX_DAMAGE,
	AP_CHARACTER_IMPORT_DCID_HEROIC_DEFENSE_POINT,
};

enum ap_character_process_type {
	AP_CHARACTER_PROCESS_ADD_CHAR,
	AP_CHARACTER_PROCESS_REMOVE_CHAR,
	AP_CHARACTER_PROCESS_UPDATE_CHAR
};

enum ap_character_transform_type {
	AP_CHARACTER_TRANSFORM_TYPE_APPEAR_ONLY = 0,
	AP_CHARACTER_TRANSFORM_TYPE_STATUS_ONLY,
	AP_CHARACTER_TRANSFORM_TYPE_APPEAR_STATUS_ALL,
};

enum ap_character_penalty_type {
	AP_CHARACTER_PENALTY_NONE = -1,
	AP_CHARACTER_PENALTY_PRVTRADE = 0,
	AP_CHARACTER_PENALTY_NPCTRADE,
	AP_CHARACTER_PENALTY_ITEMPRICE_DOUBLE,
	AP_CHARACTER_PENALTY_CONVERT,
	AP_CHARACTER_PENALTY_SKILL_LEARN,
	AP_CHARACTER_PENALTY_RIDEABLE,
	AP_CHARACTER_PENALTY_AUCTION,
	AP_CHARACTER_PENALTY_CHARCUST,
	AP_CHARACTER_PENALTY_EXP_LOSE,
	AP_CHARACTER_PENALTY_POTAL,
	AP_CHARACTER_PENALTY_FIRST_ATTACK,
	AP_CHARACTER_PENALTY_COUNT,
};

enum ap_character_resurrect_type {
	AP_CHARACTER_RESURRECT_NOW = 0,
	AP_CHARACTER_RESURRECT_TOWN,
	AP_CHARACTER_RESURRECT_SIEGE_INNER,
	AP_CHARACTER_RESURRECT_SIEGE_OUTER,
	AP_CHARACTER_RESURRECT_MAX,
};

/*
enum eCheckEnableActionState
{
AP_CHARACTER_CHECK_ENABLE_ACTION_OK = 0,
AP_CHARACTER_CHECK_DISABLE_ACTION_SLEEP,
AP_CHARACTER_CHECK_DISABLE_ACTION_STUN,
AP_CHARACTER_CHECK_DISABLE_ACTION_HOLD,
AP_CHARACTER_CHECK_DISABLE_ACTION_BLOCK,
AP_CHARACTER_CHECK_DISABLE_ACTION_CONFUSION,
AP_CHARACTER_CHECK_DISABLE_ACTION_FEAR,
AP_CHARACTER_CHECK_DISABLE_ACTION_BERSERK,
AP_CHARACTER_CHECK_DISABLE_ACTION_SHRINK,
AP_CHARACTER_CHECK_DISABLE_ACTION_ETC,
AP_CHARACTER_CHECK_ENABLE_ACTION_MAx
};
*/

enum ap_character_move_direction {
	AP_CHARACTER_MD_NONE,
	AP_CHARACTER_MD_FORWARD,
	AP_CHARACTER_MD_FORWARD_RIGHT,
	AP_CHARACTER_MD_RIGHT,
	AP_CHARACTER_MD_BACKWARD_RIGHT,
	AP_CHARACTER_MD_BACKWARD,
	AP_CHARACTER_MD_BACKWARD_LEFT,
	AP_CHARACTER_MD_LEFT,
	AP_CHARACTER_MD_FORWARD_LEFT,
	AP_CHARACTER_COUNT
};

enum ap_character_death_cause {
	/** \brief
	 * Death is caused by an attack.
	 * (i.e. normal attack, skill attack).*/
	AP_CHARACTER_DEATH_BY_ATTACK,
	/** \brief 
	 * Death is caused by a Game Master (i.e. chat command). */
	AP_CHARACTER_DEATH_BY_GM,
	/** Died to DoT (Damage Over Time). */
	AP_CHARACTER_DEATH_BY_DOT,
};

enum ap_character_move_next_action_type {
	AP_CHARACTER_MOVE_NEXT_ACTION_TYPE_NONE = 0,
	AP_CHARACTER_MOVE_NEXT_ACTION_TYPE_ATTACK,
	AP_CHARACTER_MOVE_NEXT_ACTION_TYPE_SKILL,
};

enum ap_character_adjust_combat_point_bits {
	AP_CHARACTER_ADJUST_COMBAT_POINT_NONE = 0,
	AP_CHARACTER_ADJUST_COMBAT_POINT_MAX_DAMAGE = 0x01,
	AP_CHARACTER_ADJUST_COMBAT_POINT_MIN_DAMAGE = 0x02,
};

enum ap_character_callback_id {
	AP_CHARACTER_CB_READ_IMPORT,
	AP_CHARACTER_CB_END_READ_IMPORT,
	AP_CHARACTER_CB_RECEIVE,
	AP_CHARACTER_CB_MOVE,
	AP_CHARACTER_CB_SET_MOVEMENT,
	AP_CHARACTER_CB_FOLLOW,
	AP_CHARACTER_CB_INTERRUPT_FOLLOW,
	/** 
	 * \brief Initialize static (NPC) character.
	 *
	 * Triggered after a static character is read from 
	 * file or created at a later time (i.e. with chat cmd). */
	AP_CHARACTER_CB_INIT_STATIC,
	/**
	 * \brief Free character id.
	 * 
	 * Triggered when character no longer requires the id 
	 * that was generated when it was constructed. An example 
	 * is when a static (NPC) character is read from file. */
	AP_CHARACTER_CB_FREE_ID,
	/** \brief Stop character movement. */
	AP_CHARACTER_CB_STOP_MOVEMENT,
	/**
	 * \brief Stop all character actions.
	 *
	 * Triggered when a new action is to be performed.
	 * Callback data type is 'struct ap_character *'.
	 */
	AP_CHARACTER_CB_STOP_ACTION,
	/**
	 * \brief Process character state.
	 *
	 * Triggered per-frame to process character state.
	 */
	AP_CHARACTER_CB_PROCESS,
	/**
	 * \brief Process monster state.
	 *
	 * Triggered per-frame to process monster state.
	 */
	AP_CHARACTER_CB_PROCESS_MONSTER,
	/** \brief Update character factor. */
	AP_CHARACTER_CB_UPDATE_FACTOR,
	/** \brief Update character. */
	AP_CHARACTER_CB_UPDATE,
	/** \brief Set character level. */
	AP_CHARACTER_CB_SET_LEVEL,
	/** \brief Check if character is a valid attack target. */
	AP_CHARACTER_CB_IS_VALID_ATTACK_TARGET,
	/** \brief Check if attack is successful. */
	AP_CHARACTER_CB_ATTEMPT_ATTACK,
	/** \brief Triggered on character death. */
	AP_CHARACTER_CB_DEATH,
	/** \brief Triggered once a special status is enabled. */
	AP_CHARACTER_CB_SPECIAL_STATUS_ON,
	/** \brief Triggered once a special status is disabled. */
	AP_CHARACTER_CB_SPECIAL_STATUS_OFF,
	/** \brief Triggered once character experience is gained. */
	AP_CHARACTER_CB_GAIN_EXPERIENCE,
	/** \brief Triggered once character action status is changed. */
	AP_CHARACTER_CB_SET_ACTION_STATUS,
	AP_CHARACTER_CB_SET_TEMPLATE,
};

struct ap_character_action_info {
	enum ap_character_action_result_type result;
	struct ap_factors_char_point target;
	boolean broadcast_factor;
	uint32_t additional_effect;
	int defense_penalty;
};

struct ap_character_action {
	enum ap_character_action_type action_type;
	boolean force_action;
	void * target_base;
	struct au_pos target_pos;
	int user_data[5];
	uint32_t additional_effect;
};

struct ap_character_template {
	uint32_t tid;
	char name[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	struct ap_factor factor;
	uint32_t char_type;
	uint32_t gold_min;
	uint32_t gold_max;
	enum ap_factors_attribute_type attribute_type;
	uint32_t name_color;
	uint32_t face_index;
	uint32_t hair_index;
	enum ap_character_tamable_type tamable_type;
	uint32_t land_attach_type;
	enum ap_character_range_type range_type;
	uint32_t stamina_point;
	uint32_t pet_type;
	uint32_t start_stamina_point;
	float siege_war_coll_box_width;
	float siege_war_coll_box_height;
	float siege_war_coll_sphere_radius;
	float siege_war_coll_obj_offset_x;
	float siege_war_coll_obj_offset_z;
};

struct ap_character {
	enum ap_base_type base_type;
	uint32_t id;
	uint32_t tid;
	struct ap_character_template * temp;
	char name[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	struct ap_character * summoned_by;
	uint64_t last_process_tick;
	uint64_t action_end_tick;
	uint64_t combat_end_tick;
	boolean is_moving;
	boolean is_following;
	uint32_t follow_id;
	uint16_t follow_distance;
	boolean is_moving_fast;
	enum ap_character_move_direction move_direction;
	boolean is_path_finding;
	boolean is_syncing;
	boolean is_moving_horizontal;
	enum ap_character_move_next_action_type next_action;
	uint32_t char_type;
	char nickname[AP_CHARACTER_MAX_NICKNAME_SIZE];
	struct au_pos pos;
	struct au_pos dst_pos;
	struct au_pos direction;
	float rotation_x;
	float rotation_y;
	uint32_t bound_region_id;
	struct ap_factor factor;
	struct ap_factor factor_point;
	struct ap_factor factor_percent;
	const struct ap_factor * factor_growth;
	int32_t negative_percent_movement;
	int32_t negative_percent_movement_fast;
	boolean is_transformed;
	boolean is_ridable;
	boolean is_evolved;
	enum ap_character_login_status login_status;
	enum ap_character_action_status action_status;
	enum ap_character_criminal_status criminal_status;
	uint32_t remain_criminal_status_ms;
	uint32_t remain_criminal_status_min;
	uint64_t inventory_gold;
	uint64_t bank_gold;
	uint64_t chantra_coins;
	uint8_t extra_bank_slots;
	uint64_t special_status;
	uint64_t special_status_end_tick[64];
	uint8_t face_index;
	uint8_t hair_index;
	uint32_t option_flags;
	uint16_t event_status_flags;
	uint32_t last_bsq_death_time;
	boolean npc_display_for_map;
	boolean npc_display_for_nameboard;
	uint64_t update_flags;
	uint32_t adjust_combat_point;
	boolean stop_experience;
	boolean auto_attack_after_skill_cast;

	struct ap_character * next;
};

struct ap_character_cb_read_import {
	struct ap_character_template * temp;
	enum ap_character_import_data_column_id id;
	const char * value;
};

struct ap_character_cb_end_read_import {
	struct ap_character_template * temp;
};

struct ap_character_cb_receive {
	enum ap_character_packet_type type;
	uint32_t character_id;
	uint32_t character_tid;
	uint8_t character_status;
	uint64_t inventory_gold;
	int64_t bank_gold;
	uint64_t chantra_coins;
	enum ap_character_action_status action_status;
	enum ap_character_criminal_status criminal_status;
	uint32_t attacker_id;
	boolean is_new_character;
	uint8_t region_index;
	enum ap_character_social_action_type social_action;
	uint64_t special_status;
	boolean is_transform_status;
	char skill_init[AP_CHARACTER_MAX_CHARACTER_SKILL_INIT];
	uint8_t face_index;
	uint8_t hair_index;
	uint32_t option_flags;
	uint8_t extra_bank_slots;
	uint16_t event_status_flags;
	uint32_t criminal_duration;
	uint32_t rogue_duration;
	char nickname[AP_CHARACTER_MAX_NICKNAME_SIZE];
	uint32_t last_bsq_death_time;
	void * user_data;
};

struct ap_character_cb_move {
	struct ap_character * character;
	struct au_pos prev_pos;
};

/** \brief AP_CHARACTER_CB_SET_MOVEMENT callback data. */
struct ap_character_cb_set_movement {
	struct ap_character * character;
	struct au_pos dst;
	boolean move_fast;
};

/** \brief AP_CHARACTER_CB_FOLLOW callback data. */
struct ap_character_cb_follow {
	struct ap_character * character;
	struct ap_character * target;
	boolean move_fast;
	uint16_t distance;
};

/** \brief AP_CHARACTER_CB_INTERRUPT_FOLLOW callback data. */
struct ap_character_cb_interrupt_follow {
	struct ap_character * character;
};

struct ap_character_cb_init_static {
	struct ap_character * character;
	boolean acquired_ownership;
};

struct ap_character_cb_free_id {
	struct ap_character * character;
	uint32_t id;
};

/**
 * \brief AP_CHARACTER_CB_STOP_MOVEMENT callback data.
 */
struct ap_character_cb_stop_movement {
	/** \brief Character pointer. */
	struct ap_character * character;
	/** \brief Was character moving? */
	boolean was_moving;
};

/**
 * \brief AP_CHARACTER_CB_STOP_ACTION callback data.
 */
struct ap_character_cb_stop_action {
	/** \brief Character pointer. */
	struct ap_character * character;
};

/**
 * \brief AP_CHARACTER_CB_PROCESS callback data.
 */
struct ap_character_cb_process {
	/** \brief Character pointer. */
	struct ap_character * character;
	/** \brief Current system tick. */
	uint64_t tick;
	/** \brief Delta time (sec). */
	float dt;
};

/**
 * \brief AP_CHARACTER_CB_PROCESS_MONSTER callback data.
 */
struct ap_character_cb_process_monster {
	/** \brief Character pointer. */
	struct ap_character * character;
	/** \brief Current system tick. */
	uint64_t tick;
	/** \brief Delta time (sec). */
	float dt;
};

/**
 * \brief AP_CHARACTER_CB_UPDATE_FACTOR callback data.
 */
struct ap_character_cb_update_factor {
	struct ap_character * character;
	uint64_t update_flags;
};

/**
 * \brief AP_CHARACTER_CB_UPDATE callback data.
 */
struct ap_character_cb_update {
	struct ap_character * character;
	uint64_t update_flags;
	/* \brief 
	 * If TRUE, character updates are broadcast only 
	 * to character itself. */
	boolean is_private;
};

/**
 * \brief AP_CHARACTER_CB_SET_LEVEL callback data.
 */
struct ap_character_cb_set_level {
	struct ap_character * character;
	uint32_t prev_level;
};

/**
 * \brief AP_CHARACTER_CB_IS_VALID_ATTACK_TARGET callback data.
 */
struct ap_character_cb_is_valid_attack_target {
	struct ap_character * character;
	struct ap_character * target;
	boolean force_attack;
};

/**
 * \brief AP_CHARACTER_CB_ATTEMPT_ATTACK callback data.
 */
struct ap_character_cb_attempt_attack {
	struct ap_character * character;
	struct ap_character * target;
	struct ap_character_action_info * action;
};

/** \brief AP_CHARACTER_CB_DEATH callback data. */
struct ap_character_cb_death {
	struct ap_character * character;
	struct ap_character * killer;
	enum ap_character_death_cause cause;
};

/** \brief AP_CHARACTER_CB_SPECIAL_STATUS_ON callback data. */
struct ap_character_cb_special_status_on {
	struct ap_character * character;
	uint64_t special_status;
};

/** \brief AP_CHARACTER_CB_SPECIAL_STATUS_OFF callback data. */
struct ap_character_cb_special_status_off {
	struct ap_character * character;
	uint64_t special_status;
};

/** \brief AP_CHARACTER_CB_GAIN_EXPERIENCE callback data. */
struct ap_character_cb_gain_experience {
	struct ap_character * character;
	uint64_t amount;
};

/** \brief AP_CHARACTER_CB_SET_ACTION_STATUS callback data. */
struct ap_character_cb_set_action_status {
	struct ap_character * character;
	enum ap_character_action_status previous_status;
};

/** \brief AP_CHARACTER_CB_SET_TEMPLATE callback data. */
struct ap_character_cb_set_template {
	struct ap_character * character;
	struct ap_character_template * previous_temp;
};

struct ap_character_module * ap_character_create_module();

boolean ap_character_read_templates(
	struct ap_character_module * mod,
	const char * file_path, 
	boolean decrypt);

/**
 * Read static (NPC) characters from file.
 * \param[in] file_path Path to file.
 * \param[in] decrypt   Whether to decrypt file before reading.
 *
 * \return TRUE if successful. Otherwise FALSE.
 */
boolean ap_character_read_static(
	struct ap_character_module * mod,
	const char * file_path, 
	boolean decrypt);

boolean ap_character_write_static(
	struct ap_character_module * mod,
	const char * file_path, 
	boolean encrypt,
	struct ap_admin * admin);

boolean ap_character_read_import_data(
	struct ap_character_module * mod,
	const char * file_path, 
	boolean decrypt);

boolean ap_character_read_grow_up_table(
	struct ap_character_module * mod,
	const char * file_path,
	boolean decrypt);

boolean ap_character_read_level_up_exp(
	struct ap_character_module * mod,
	const char * file_path,
	boolean decrypt);

struct ap_character * ap_character_new(struct ap_character_module * mod);

void ap_character_free(struct ap_character_module * mod, struct ap_character * character);

/**
 * Free character id (set to 0).
 * \param character Character pointer.
 */
void ap_character_free_id(struct ap_character_module * mod, struct ap_character * character);

void ap_character_add_callback(
	struct ap_character_module * mod,
	enum ap_character_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t ap_character_attach_data(
	struct ap_character_module * mod,
	enum ap_character_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

void ap_character_add_stream_callback(
	struct ap_character_module * mod,
	enum ap_character_module_data_index data_index,
	const char * module_name,
	ap_module_t callback_module,
	ap_module_stream_read_t read_cb,
	ap_module_stream_write_t write_cb);

void ap_character_set_template(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character_template * temp);

struct ap_character_template * ap_character_get_template(
	struct ap_character_module * mod,
	uint32_t tid);

struct ap_character_template * ap_character_iterate_templates(
	struct ap_character_module * mod,
	size_t * index);

void ap_character_move(
	struct ap_character_module * mod,
	struct ap_character * character,
	const struct au_pos * pos);

void ap_character_set_movement(
	struct ap_character_module * mod,
	struct ap_character * character,
	const struct au_pos * dst,
	boolean move_fast);

void ap_character_follow(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character * target,
	boolean move_fast,
	uint16_t follow_distance);

/**
 * Stop character movement.
 * \param[in,out] character Character pointer.
 */
void ap_character_stop_movement(
	struct ap_character_module * mod,
	struct ap_character * character);

/**
 * Stop all character actions.
 * \param[in,out] character Character pointer.
 */
void ap_character_stop_action(struct ap_character_module * mod, struct ap_character * character);

/**
 * Process all character state.
 * \param[in,out] character Character pointer.
 * \param[in]     tick      Current system tick.
 * \param[in]     dt        Delta time (sec).
 */
void ap_character_process(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint64_t tick,
	float dt);

/**
 * Process monster state.
 * \param[in,out] character Character pointer.
 * \param[in]     tick      Current system tick.
 * \param[in]     dt        Delta time (sec).
 */
void ap_character_process_monster(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint64_t tick,
	float dt);

void ap_character_set_grow_up_factor(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint32_t prev_level,
	uint32_t level);

uint64_t ap_character_get_level_up_exp(struct ap_character_module * mod, uint32_t current_level);

int ap_character_calc_defense_penalty(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character * target);

uint32_t ap_character_calc_physical_attack(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character * target,
	struct ap_character_action_info * action);

uint32_t ap_character_calc_spirit_attack(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character * target,
	struct ap_character_action_info * action,
	enum ap_factors_attribute_type type);

uint32_t ap_character_calc_heroic_attack(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character * target,
	struct ap_character_action_info * action);

void ap_character_update_factor(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint64_t update_flags);

void ap_character_update(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint64_t update_flags,
	boolean is_private);

void ap_character_set_level(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint32_t level);

boolean ap_character_is_valid_attack_target(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character * target,
	boolean force_attack);

boolean ap_character_attempt_attack(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character * target,
	struct ap_character_action_info * action);

void ap_character_kill(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character * killer,
	enum ap_character_death_cause cause);

void ap_character_special_status_on(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint64_t special_status,
	uint64_t duration_ms);

void ap_character_special_status_off(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint64_t special_status);

void ap_character_gain_experience(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint64_t amount);

void ap_character_set_action_status(
	struct ap_character_module * mod,
	struct ap_character * character,
	enum ap_character_action_status status);

boolean ap_character_on_receive(
	struct ap_character_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_character_make_add_packet(
	struct ap_character_module * mod,
	struct ap_character * character);

void ap_character_make_event_effect_packet(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint32_t event_effect_id);

void ap_character_make_packet(
	struct ap_character_module * mod,
	enum ap_character_packet_type type,
	uint32_t character_id);

/**
 * Make character packet with buffer.
 * \param[in]  type         Character packet type.
 * \param[in]  character_id Character id;
 * \param[out] buffer       Output buffer.
 * \param[out] length       Output buffer length.
 */
void ap_character_make_packet_buffer(
	struct ap_character_module * mod,
	enum ap_character_packet_type type,
	uint32_t character_id,
	void * buffer,
	uint16_t * length);

void ap_character_make_update_packet(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint64_t flags);

void ap_character_make_update_gameplay_option_packet(
	struct ap_character_module * mod,
	struct ap_character * character);

void ap_character_make_client_packet(
	struct ap_character_module * mod,
	enum ac_character_packet_type type,
	const char * account_id,
	uint32_t * character_tid,
	const char * character_name,
	uint32_t * character_id,
	const struct au_pos * pos,
	uint32_t * auth_key);

boolean ap_character_parse_client_packet(
	struct ap_character_module * mod,
	const void * data, 
	uint16_t length,
	enum ac_character_packet_type * type,
	char * character_name,
	uint32_t * auth_key);

static inline uint32_t ap_character_get_level(const struct ap_character * character)
{
	return character->factor.char_status.level;
}

static inline uint32_t ap_character_get_absolute_level(const struct ap_character * character)
{
	if (CHECK_BIT(character->special_status, AP_CHARACTER_SPECIAL_STATUS_LEVELLIMIT))
		return character->factor.char_status.limited_level;
	else
		return character->factor.char_status.level;
}

static inline boolean ap_character_is_pc(struct ap_character * character)
{
	return ((character->char_type & AP_CHARACTER_TYPE_PC) != 0);
}

static inline boolean ap_character_is_summon(struct ap_character * character)
{
	return ((character->char_type & AP_CHARACTER_TYPE_SUMMON) != 0);
}

static inline void ap_character_fix_attack_speed(struct ap_character * character)
{
	if (ap_character_is_pc(character)) {
		if (character->factor.attack.attack_speed < 15)
			character->factor.attack.attack_speed = 15;
	}
	else if (character->factor.attack.attack_speed < 1) {
		character->factor.attack.attack_speed = 1;
	}
}

static inline void ap_character_fix_hp(struct ap_character * character)
{
	if (character->factor.char_point.hp > character->factor.char_point_max.hp)
		character->factor.char_point.hp = character->factor.char_point_max.hp;
}

static inline void ap_character_fix_mp(struct ap_character * character)
{
	if (character->factor.char_point.mp > character->factor.char_point_max.mp)
		character->factor.char_point.mp = character->factor.char_point_max.mp;
}

static inline void ap_character_adjust_combat_point(struct ap_character * character)
{
	if (character->adjust_combat_point & AP_CHARACTER_ADJUST_COMBAT_POINT_MAX_DAMAGE)
		character->factor.damage.min.physical = character->factor.damage.max.physical;
	else if (character->adjust_combat_point & AP_CHARACTER_ADJUST_COMBAT_POINT_MIN_DAMAGE)
		character->factor.damage.max.physical = character->factor.damage.min.physical;
}

static inline boolean ap_character_has_gold(
	struct ap_character * character, 
	uint64_t amount)
{
	return (character->inventory_gold >= amount);
}

static inline boolean ap_character_exceeds_gold_limit(
	struct ap_character * character, 
	uint64_t amount)
{
	return (character->inventory_gold + amount > AP_CHARACTER_MAX_INVENTORY_GOLD);
}

static inline void ap_character_set_criminal_duration(
	struct ap_character * character, 
	uint32_t ms)
{
	character->remain_criminal_status_ms = ms;
	character->remain_criminal_status_min = ms / 1000;
}

static inline boolean ap_character_is_target_out_of_level_range(
	struct ap_character * character,
	struct ap_character * target)
{
	uint32_t selflevel = ap_character_get_level(character);
	uint32_t targetlevel = ap_character_get_level(target);
	return  (selflevel > targetlevel + 11 || selflevel + 11 < targetlevel);
}

static inline uint8_t ap_character_get_special_status_duration_index(
	uint64_t special_status)
{
	uint8_t i;
	for (i = 0; i < 64; i++) {
		if (special_status & (1ull << i))
			return i;
	}
	assert(0);
	return UINT8_MAX;
}

static inline void ap_character_restore_hpmp(struct ap_character * character)
{
	character->factor.char_point.hp = character->factor.char_point_max.hp;
	character->factor.char_point.mp = character->factor.char_point_max.mp;
}

END_DECLS

#endif /* _AP_CHARACTER_H_ */
