#ifndef _AP_ITEM_H_
#define _AP_ITEM_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_factors.h"
#include "public/ap_grid.h"
#include "public/ap_module_instance.h"

#define AP_ITEM_MODULE_NAME "AgpmItem"

#define AP_ITEM_MAX_NAME_LENGTH 60
#define AP_ITEM_MAX_SKILL_PLUS 60
#define AP_ITEM_MAX_SKILL_PLUS_EFFECT 3

#define AP_ITEM_MAX_STACK_COUNT 1000000

#define AP_ITEM_MAX_AVATAR_SET_PART_COUNT 8

#define	AP_ITEM_CASH_ITEM_UNUSE 0
#define	AP_ITEM_CASH_ITEM_INUSE 1
#define AP_ITEM_CASH_ITEM_PAUSE 2

#define AP_ITEM_PRIVATE_TRADE_OPTION_MAX 3

#define	AP_ITEM_CASH_ITEM_MAX_USABLE_COUNT 0x0FFFFFFF
#define AP_ITEM_MAX_ITEM_MAGIC_ATTR 22
#define AP_ITEM_MAX_ITEM_SPIRITED 10
#define AP_ITEM_MAX_ITEM_DATA_COUNT 1000000
#define AP_ITEM_MAX_ITEM_OWN 100

#define AP_ITEM_MAX_PICKUP_ITEM_DISTANCE 2000.0f

#define AP_ITEM_EQUIP_LAYER 3
#define AP_ITEM_EQUIP_ROW 3
#define AP_ITEM_EQUIP_COLUMN 4

#define AP_ITEM_INVENTORY_LAYER 5
#define AP_ITEM_INVENTORY_ROW 5
#define AP_ITEM_INVENTORY_COLUMN 5

#define AGPAITEM_MAX_ITEM_TEMPLATE 256

#define AP_ITEM_BTREE_ORDER 30

#define AP_ITEM_INVALID_IID 0

#define AP_ITEM_SALES_BOX_SIZE 10

#define	AP_ITEM_BANK_MAX_LAYER 4
#define AP_ITEM_BANK_ROW 7
#define AP_ITEM_BANK_COLUMN 4

#define AP_ITEM_TRADEBOX_LAYER 1
#define AP_ITEM_TRADEBOX_ROW 3
#define AP_ITEM_TRADEBOX_COLUMN 4

#define AP_ITEM_SALES_LAYER 1
#define AP_ITEM_SALES_ROW 1
#define AP_ITEM_SALES_COLUMN 10

#define AP_ITEM_NPCTRADEBOX_LAYER 1
#define AP_ITEM_NPCTRADEBOX_ROW 20
#define AP_ITEM_NPCTRADEBOX_COLUMN 4

#define AP_ITEM_QUEST_LAYER 1
#define AP_ITEM_QUEST_ROW 4
#define	AP_ITEM_QUEST_COLUMN 4

#define AP_ITEM_CASH_INVENTORY_LAYER 1
#define AP_ITEM_CASH_INVENTORY_ROW 200
#define	AP_ITEM_CASH_INVENTORY_COLUMN 1

#define AP_ITEM_GUILD_WAREHOUSE_LAYER 2
#define AP_ITEM_GUILD_WAREHOUSE_ROW 7
#define	AP_ITEM_GUILD_WAREHOUSE_COLUMN 4

#define AP_ITEM_SUB_LAYER 4
#define AP_ITEM_SUB_ROW 2
#define AP_ITEM_SUB_COLUMN 4

#define	AP_ITEM_MAX_USE_ITEM 20

#define	AP_ITEM_MAX_CONVERT 10
#define	AP_ITEM_MAX_CONVERT_WEAPON 10
#define	AP_ITEM_MAX_CONVERT_ARMOUR 5

#define AP_ITEM_MAX_DURABILITY 100

#define AP_ITEM_DEFAULT_INVEN_NUM 30

#define	AP_ITEM_REQUIRE_LEVEL_UP_STEP 2

#define AP_ITEM_MAX_DELETE_REASON 32

#define AP_ITEM_TRANSFORM_RECAST_TIME 600000

#define AP_ITEM_REVERSE_ORB_REUSE_INTERVAL 1800000

#define	AP_ITEM_STATUS_FLAG_BIND_ON_ACQUIRE 0x00000001
#define	AP_ITEM_STATUS_FLAG_BIND_ON_EQUIP   0x00000002
#define	AP_ITEM_STATUS_FLAG_BIND_ON_USE     0x00000004
#define	AP_ITEM_STATUS_FLAG_BIND_ON_OWNER   0x00000008
#define AP_ITEM_STATUS_FLAG_QUEST           0x00000010
#define AP_ITEM_STATUS_FLAG_CASH_PPCARD     0x00000100
#define AP_ITEM_STATUS_FLAG_DISARMAMENT     0x00001000
#define AP_ITEM_STATUS_FLAG_CANNOT_BE_SOLD  0x00002000

#define AP_ITEM_MAX_SKULL_LEVEL 10

#define AP_ITEM_GOLD_TEMPLATE_ID 268

BEGIN_DECLS

struct ap_item_module;

enum ap_item_option_part {
	AP_ITEM_OPTION_PART_BODY = 0,
	AP_ITEM_OPTION_PART_LEGS,
	AP_ITEM_OPTION_PART_WEAPON,
	AP_ITEM_OPTION_PART_SHIELD,
	AP_ITEM_OPTION_PART_HEAD,
	AP_ITEM_OPTION_PART_RING,
	AP_ITEM_OPTION_PART_NECKLACE,
	AP_ITEM_OPTION_PART_FOOTS,
	AP_ITEM_OPTION_PART_HANDS,
	AP_ITEM_OPTION_PART_REFINERY,
	AP_ITEM_OPTION_PART_GACHA,
	AP_ITEM_OPTION_PART_COUNT
};

enum ap_item_option_set_type {
	AP_ITEM_OPTION_SET_DROP = 0,
	AP_ITEM_OPTION_SET_REFINERY,
	AP_ITEM_OPTION_SET_ALL,
};

#define	AP_ITEM_OPTION_SET_TYPE_BODY		(1 << AP_ITEM_OPTION_PART_BODY)
#define	AP_ITEM_OPTION_SET_TYPE_LEGS		(1 << AP_ITEM_OPTION_PART_LEGS)
#define	AP_ITEM_OPTION_SET_TYPE_WEAPON		(1 << AP_ITEM_OPTION_PART_WEAPON)
#define	AP_ITEM_OPTION_SET_TYPE_SHIELD		(1 << AP_ITEM_OPTION_PART_SHIELD)
#define	AP_ITEM_OPTION_SET_TYPE_HEAD		(1 << AP_ITEM_OPTION_PART_HEAD)
#define	AP_ITEM_OPTION_SET_TYPE_RING		(1 << AP_ITEM_OPTION_PART_RING)
#define	AP_ITEM_OPTION_SET_TYPE_NECKLACE	(1 << AP_ITEM_OPTION_PART_NECKLACE)
#define	AP_ITEM_OPTION_SET_TYPE_FOOTS		(1 << AP_ITEM_OPTION_PART_FOOTS)
#define	AP_ITEM_OPTION_SET_TYPE_HANDS		(1 << AP_ITEM_OPTION_PART_HANDS)
#define	AP_ITEM_OPTION_SET_TYPE_REFINERY	(1 << AP_ITEM_OPTION_PART_REFINERY)
#define	AP_ITEM_OPTION_SET_TYPE_GACHA		(1 << AP_ITEM_OPTION_PART_GACHA)

#define	AP_ITEM_OPTION_MAX_COUNT 5
#define AP_ITEM_OPTION_MAX_LINK_COUNT 10
#define AP_ITEM_OPTION_MAX_RUNE_COUNT \
	AP_ITEM_OPTION_MAX_COUNT * AP_ITEM_OPTION_MAX_LINK_COUNT
#define AP_ITEM_OPTION_LINK_POOL_SIZE 32

#define AP_ITEM_OPTION_DESCRIPTION_SIZE 75
#define AP_ITEM_OPTION_MAX_TYPE 256
#define AP_ITEM_OPTION_MAX_RANK 4

enum ap_item_option_skill_type {
	AP_ITEM_OPTION_SKILL_TYPE_CRITICAL = 1,
	AP_ITEM_OPTION_SKILL_TYPE_STUN,
	AP_ITEM_OPTION_SKILL_TYPE_DMG_CONVERT_HP,
	AP_ITEM_OPTION_SKILL_TYPE_REGEN_HP,
	AP_ITEM_OPTION_SKILL_TYPE_DOT,
	AP_ITEM_OPTION_SKILL_TYPE_EVADE,
	AP_ITEM_OPTION_SKILL_TYPE_MAX,
};

struct ap_item_option_skill_data {
	int32_t critical_rate;
	int32_t critical_damage;
	int32_t stun_rate;
	uint32_t stun_time;
	int32_t damage_convert_hp_rate;
	int32_t damage_convert_hp;
	int32_t regen_hp;
	int32_t regen_mp;
	int32_t dot_rate;
	uint32_t dot_time;
	int32_t dot_damage;
	int32_t skill_level_up;
	int32_t bonus_exp;
	int32_t bonus_gold;
	int32_t bonus_drop_rate;
	int32_t bonus_rare_drop_rate;
	int32_t bonus_charisma_rate;
	int32_t ignore_defense;
	int32_t ignore_elemental_resistance;
	int32_t critical_resistance;
	int32_t reinforcement_rate;
};

struct ap_item_option_template {
	uint32_t id;
	struct ap_factor factor;
	struct ap_factor factor_percent;
	struct ap_factor skill_factor;
	struct ap_factor skill_factor_percent;
	uint32_t skill_tid;
	uint32_t skill_level;
	uint32_t set_part;
	uint32_t type;
	uint32_t point_type;
	uint32_t level_limit;
	uint32_t rank_min;
	uint32_t rank_max;
	uint32_t probability;
	boolean is_visible;
	enum ap_item_option_set_type set_type;
	char description[AP_ITEM_OPTION_DESCRIPTION_SIZE];
	enum ap_item_option_skill_type skill_type;
	struct ap_item_option_skill_data skill_data;
	uint32_t level_min;
	uint32_t level_max;
	uint32_t link_id;
};

struct ap_item_option_link_pool {
	struct ap_item_option_template * options[AP_ITEM_OPTION_LINK_POOL_SIZE];
	uint32_t count;
};

enum ap_item_type {
	AP_ITEM_TYPE_EQUIP = 0,
	AP_ITEM_TYPE_USABLE,
	AP_ITEM_TYPE_OTHER,
	AP_ITEM_TYPE_COUNT
};

enum ap_item_part {
	AP_ITEM_PART_NONE = 1,
	AP_ITEM_PART_BODY,
	AP_ITEM_PART_HEAD,
	AP_ITEM_PART_ARMS,
	AP_ITEM_PART_HANDS,
	AP_ITEM_PART_LEGS,
	AP_ITEM_PART_FOOT,
	AP_ITEM_PART_ARMS2,
	AP_ITEM_PART_HAND_LEFT,
	AP_ITEM_PART_HAND_RIGHT,
	AP_ITEM_PART_ACCESSORY_RING1,
	AP_ITEM_PART_ACCESSORY_RING2,
	AP_ITEM_PART_ACCESSORY_NECKLACE,
	AP_ITEM_PART_RIDE,
	AP_ITEM_PART_LANCER,
	AP_ITEM_PART_V_BODY,
	AP_ITEM_PART_V_HEAD,
	AP_ITEM_PART_V_ARMS,
	AP_ITEM_PART_V_HANDS,
	AP_ITEM_PART_V_LEGS,
	AP_ITEM_PART_V_FOOT,
	AP_ITEM_PART_V_ARMS2,
	AP_ITEM_PART_V_HAND_LEFT,
	AP_ITEM_PART_V_HAND_RIGHT,
	AP_ITEM_PART_V_ACCESSORY_RING1,
	AP_ITEM_PART_V_ACCESSORY_RING2,
	AP_ITEM_PART_V_ACCESSORY_NECKLACE,
	AP_ITEM_PART_V_RIDE,
	AP_ITEM_PART_V_LANCER,
	AP_ITEM_PART_V_ALL,
	AP_ITEM_PART_COUNT
};

enum ap_item_equip_kind {
	AP_ITEM_EQUIP_KIND_ARMOR    = 0x01,
	AP_ITEM_EQUIP_KIND_WEAPON   = 0x02,
	AP_ITEM_EQUIP_KIND_SHIELD   = 0x04,
	AP_ITEM_EQUIP_KIND_RING     = 0x08,
	AP_ITEM_EQUIP_KIND_NECKLACE = 0x10,
	AP_ITEM_EQUIP_KIND_RIDE     = 0x20,
	AP_ITEM_EQUIP_KIND_COUNT    = 5
};

#define AP_ITEM_EQUIP_KIND_ARMOR_INDEX    0
#define AP_ITEM_EQUIP_KIND_WEAPON_INDEX   1
#define AP_ITEM_EQUIP_KIND_SHIELD_INDEX   2
#define AP_ITEM_EQUIP_KIND_RING_INDEX     3
#define AP_ITEM_EQUIP_KIND_NECKLACE_INDEX 4

enum ap_item_equip_weapon_type {
	AP_ITEM_EQUIP_WEAPON_TYPE_NONE = -1,
	AP_ITEM_EQUIP_WEAPON_TYPE_ONE_HAND_SWORD = 0,
	AP_ITEM_EQUIP_WEAPON_TYPE_ONE_HAND_AXE,
	AP_ITEM_EQUIP_WEAPON_TYPE_ONE_HAND_MACE,
	AP_ITEM_EQUIP_WEAPON_TYPE_ONE_HAND_HAMMER,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_SWORD,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_AXE,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_MACE,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_HAMMER,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_POLEARM,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_SCYTHE,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_BOW,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_CROSSBOW,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_CLAW,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_WING,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_STAFF,
	AP_ITEM_EQUIP_WEAPON_TYPE_ONE_HAND_TROPHY,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_WAND,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_LANCE,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_KATARIA,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_CHAKRAM,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_STANDARD,
	AP_ITEM_EQUIP_WEAPON_TYPE_ONE_HAND_RAPIER,
	AP_ITEM_EQUIP_WEAPON_TYPE_ONE_HAND_DAGGER,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_RAPIERDAGGER,
	AP_ITEM_EQUIP_WEAPON_TYPE_ONE_HAND_KARON,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_KARON,
	AP_ITEM_EQUIP_WEAPON_TYPE_ONE_HAND_XENON,
	AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_XENON,
	AP_ITEM_EQUIP_WEAPON_TYPE_COUNT
};

enum ap_item_usable_type {
	AP_ITEM_USABLE_TYPE_POTION = 0,
	AP_ITEM_USABLE_TYPE_BRANK = 1,
	AP_ITEM_USABLE_TYPE_SPIRIT_STONE = 2,
	AP_ITEM_USABLE_TYPE_SOUL_CUBE,
	AP_ITEM_USABLE_TYPE_TELEPORT_SCROLL,
	AP_ITEM_USABLE_TYPE_ARROW,
	AP_ITEM_USABLE_TYPE_BOLT,
	AP_ITEM_USABLE_TYPE_TRANSFORM,
	AP_ITEM_USABLE_TYPE_CONVERT_CATALYST,
	AP_ITEM_USABLE_TYPE_RUNE,
	AP_ITEM_USABLE_TYPE_RECEIPE,
	AP_ITEM_USABLE_TYPE_SKILL_BOOK,
	AP_ITEM_USABLE_TYPE_SKILL_SCROLL,
	AP_ITEM_USABLE_TYPE_SKILLROLLBACK_SCROLL,
	AP_ITEM_USABLE_TYPE_REVERSE_ORB,
	AP_ITEM_USABLE_TYPE_RECALL_PARTY,
	AP_ITEM_USABLE_TYPE_MAP,
	AP_ITEM_USABLE_TYPE_LOTTERY_BOX,
	AP_ITEM_USABLE_TYPE_LOTTERY_KEY,
	AP_ITEM_USABLE_TYPE_ANTI_DROP,
	AP_ITEM_USABLE_TYPE_AREA_CHATTING,
	AP_ITEM_USABLE_TYPE_ADD_BANK_SLOT,
	AP_ITEM_USABLE_TYPE_USE_EFFECT,
	AP_ITEM_USABLE_TYPE_CHATTING,
	AP_ITEM_USABLE_TYPE_JUMP,
	AP_ITEM_USABLE_TYPE_LUCKY_SCROLL,
	AP_ITEM_USABLE_TYPE_NICK,
	AP_ITEM_USABLE_TYPE_SKILL_INIT,
	AP_ITEM_USABLE_TYPE_SOCKET_INIT,
	AP_ITEM_USABLE_TYPE_ANY_SUNDRY,
	AP_ITEM_USABLE_TYPE_ANY_BANK,
	AP_ITEM_USABLE_TYPE_AVATAR,
	AP_ITEM_USABLE_TYPE_CUSTOMIZE,
	AP_ITEM_USABLE_TYPE_QUEST,
	AP_ITEM_USABLE_TYPE_PET_FOOD,
	AP_ITEM_USABLE_TYPE_PRIVATE_TRADE_OPTION,
	AP_ITEM_USABLE_TYPE_CERARIUMORB = 36,
	AP_ITEM_USABLE_TYPE_COUNT
};

enum ap_item_usable_scroll_subtype {
	AP_ITEM_USABLE_SCROLL_SUBTYPE_NONE = 0,
	AP_ITEM_USABLE_SCROLL_SUBTYPE_MOVE_SPEED,
	AP_ITEM_USABLE_SCROLL_SUBTYPE_ATTACK_SPEED,
	AP_ITEM_USABLE_SCROLL_SUBTYPE_DEFENSE,
};

enum ap_item_usable_potion_type {
	AP_ITEM_USABLE_POTION_TYPE_NONE = 0,
	AP_ITEM_USABLE_POTION_TYPE_HP,
	AP_ITEM_USABLE_POTION_TYPE_MP,
	AP_ITEM_USABLE_POTION_TYPE_SP,
	AP_ITEM_USABLE_POTION_TYPE_BOTH,
};

enum ap_item_usable_potion_type2 {
	AP_ITEM_USABLE_POTION_TYPE_NORMAL = 0,
	AP_ITEM_USABLE_POTION_TYPE_HOT = 1,
	AP_ITEM_USABLE_POTION_TYPE_GUILD = 2,
	AP_ITEM_USABLE_POTION_TYPE_VITAL = 3,
	AP_ITEM_USABLE_POTION_TYPE_RECOVERY = 4,
	AP_ITEM_USABLE_POTION_TYPE_CURE = 5,
};

enum ap_item_usable_teleport_scroll_type {
	AP_ITEM_USABLE_TELEPORT_SCROLL_GO_TOWN = 0,
	AP_ITEM_USABLE_TELEPORT_SCROLL_RETURN_TOWN,
	AP_ITEM_USABLE_TELEPORT_SCROLL_DESTINATION,
	AP_ITEM_USABLE_TELEPORT_SCROLL_GO_TOWN_NOW,
};

enum ap_item_usable_spirit_stone_type {
	AP_ITEM_USABLE_SS_TYPE_NONE = 0,
	AP_ITEM_USABLE_SS_TYPE_MAGIC,
	AP_ITEM_USABLE_SS_TYPE_WATER,
	AP_ITEM_USABLE_SS_TYPE_FIRE,
	AP_ITEM_USABLE_SS_TYPE_EARTH,
	AP_ITEM_USABLE_SS_TYPE_AIR,
	AP_ITEM_USABLE_SS_TYPE_ICE,
	AP_ITEM_USABLE_SS_TYPE_LIGHTENING,
	AP_ITEM_USABLE_SS_TYPE_POISON,
	AP_ITEM_USABLE_SS_TYPE_CON = 100,
	AP_ITEM_USABLE_SS_TYPE_STR,
	AP_ITEM_USABLE_SS_TYPE_INT,
	AP_ITEM_USABLE_SS_TYPE_DEX,
	AP_ITEM_USABLE_SS_TYPE_WIS = 106,
	AP_ITEM_USABLE_SS_TYPE_COUNT
};

enum ap_item_usable_lottery_type {
	AP_ITEM_USABLE_LOTTERY_TYPE_NONE = 0,
	AP_ITEM_USABLE_LOTTERY_TYPE_GOLD,
	AP_ITEM_USABLE_LOTTERY_TYPE_SILVER,
	AP_ITEM_USABLE_LOTTERY_TYPE_BRONZE,
	AP_ITEM_USABLE_LOTTERY_TYPE_PLATINUM,
	AP_ITEM_USABLE_LOTTERY_TYPE_COUNT
};

enum ap_item_usable_effect_type {
	AP_ITEM_USABLE_EFFECT_TYPE_FIREWORK = 0,
	AP_ITEM_USABLE_EFFECT_TYPE_COUNT
};

enum ap_item_usable_chatting_type {
	AP_ITEM_USABLE_CHATTING_TYPE_EMPHASIS = 0,
	AP_ITEM_USABLE_CHATTING_TYPE_COUNT
};

enum ap_item_other_type {
	AP_ITEM_OTHER_TYPE_SKULL = 0,
	AP_ITEM_OTHER_TYPE_MONEY,
	AP_ITEM_OTHER_TYPE_ETC_ITEM,
	AP_ITEM_OTHER_TYPE_FIRST_LOOTER_ONLY,
	AP_ITEM_OTHER_TYPE_REMISSION,
	AP_ITEM_OTHER_TYPE_ARCHON_SCROLL,
	AP_ITEM_OTHER_TYPE_COUNT
};

enum ap_item_other_skull_type {
	AP_ITEM_OTHER_SKULL_TYPE_HUMAN = 0,
	AP_ITEM_OTHER_SKULL_TYPE_ORC,
	AP_ITEM_OTHER_SKULL_TYPE_1,
	AP_ITEM_OTHER_SKULL_TYPE_2,
	AP_ITEM_OTHER_SKULL_TYPE_3,
	AP_ITEM_OTHER_SKULL_TYPE_4,
	AP_ITEM_OTHER_SKULL_TYPE_5,
	AP_ITEM_OTHER_SKULL_TYPE_COUNT
};

enum ap_item_usable_area_chatting_type {
	AP_ITEM_USABLE_AREA_CHATTING_TYPE_NONE = 0,
	AP_ITEM_USABLE_AREA_CHATTING_TYPE_RACE,
	AP_ITEM_USABLE_AREA_CHATTING_TYPE_ALL,
	AP_ITEM_USABLE_AREA_CHATTING_TYPE_GLOBAL,
	AP_ITEM_USABLE_AREA_CHATTING_TYPE_COUNT
};

enum ap_item_status {
	AP_ITEM_STATUS_NONE = -1,
	AP_ITEM_STATUS_NOTSETTING,
	AP_ITEM_STATUS_FIELD,
	AP_ITEM_STATUS_INVENTORY,
	AP_ITEM_STATUS_EQUIP,
	AP_ITEM_STATUS_TEMP,
	AP_ITEM_STATUS_NUM,
	AP_ITEM_STATUS_TRADE_GRID,
	AP_ITEM_STATUS_CLIENT_TRADE_GRID,
	AP_ITEM_STATUS_SALESBOX_GRID,
	AP_ITEM_STATUS_SALESBOX_BACKOUT,
	AP_ITEM_STATUS_NPC_TRADE,
	AP_ITEM_STATUS_RECEIPE,
	AP_ITEM_STATUS_QUEST,
	AP_ITEM_STATUS_SUB_INVENTORY,
	AP_ITEM_STATUS_TRADE_OPTION_GRID,
	AP_ITEM_STATUS_CASH_MALL = 50,
	AP_ITEM_STATUS_CASH_INVENTORY,
	AP_ITEM_STATUS_MAILBOX,
	AP_ITEM_STATUS_GUILD_WAREHOUSE,
	AP_ITEM_STATUS_SIEGEWAR_OBJECT = 90,
	AP_ITEM_STATUS_BANK = 100,
};

enum ap_item_rune_attribute {
	AP_ITEM_RUNE_ATTR_NONE = (-1),
	AP_ITEM_RUNE_ATTR_ADD_MOVE_SPEED_WITH_WING = 22,
	AP_ITEM_RUNE_ATTR_ADD_MOVE_SPEED_WITH_WING2 = 23,
	AP_ITEM_RUNE_ATTR_CONVERT = 24,
	AP_ITEM_RUNE_ATTR_ADD_ALL_REGISTANCE_WITH_CLOAK = 33,
};

enum ap_item_other_etc_type {
	AP_ITEM_OTHER_ETC_TYPE_NONE = (-1),
};

enum ap_item_buyer_type {
	AP_ITEM_BUYER_TYPE_ALL = 0,
	AP_ITEM_BUYER_TYPE_GUILD_MASTER_ONLY = 1,
};

enum ap_item_using_type {
	AP_ITEM_USING_TYPE_ALL = 0,
	AP_ITEM_USING_TYPE_SIEGE_WAR_ONLY = 1,
	AP_ITEM_USING_TYPE_SIEGE_WAR_ATTACK_ONLY = 2,
	AP_ITEM_USING_TYPE_SIEGE_WAR_DEFENSE_ONLY = 4,
};

enum ap_item_cash_item_type {
	AP_ITEM_CASH_ITEM_TYPE_NONE         = 0,
	AP_ITEM_CASH_ITEM_TYPE_ONE_USE      = 1,
	AP_ITEM_CASH_ITEM_TYPE_PLAY_TIME	= 2,
	AP_ITEM_CASH_ITEM_TYPE_REAL_TIME	= 3,
	AP_ITEM_CASH_ITEM_TYPE_ONE_ATTACK	= 4,
	AP_ITEM_CASH_ITEM_TYPE_EQUIP		= 5,
	AP_ITEM_CASH_ITEM_TYPE_STAMINA		= 6,
};

#define AP_ITEM_IS_CASH_ITEM(ITEM) \
	((ITEM)->temp->cash_item_type != AP_ITEM_CASH_ITEM_TYPE_NONE)

enum ap_item_cash_item_use_type {
	AP_ITEM_CASH_ITEM_USE_TYPE_UNUSABLE = 0,
	AP_ITEM_CASH_ITEM_USE_TYPE_ONCE = 1,
	AP_ITEM_CASH_ITEM_USE_TYPE_CONTINUOUS = 2,
};

enum ap_item_pc_bang_type {
	AP_ITEM_PCBANG_TYPE_NONE              = 0x00,
	AP_ITEM_PCBANG_TYPE_BUY_ONLY          = 0x01,
	AP_ITEM_PCBANG_TYPE_USE_ONLY          = 0x02,
	AP_ITEM_PCBANG_TYPE_DOUBLE_EFFECT     = 0x04,
	AP_ITEM_PCBANG_TYPE_LIMITED_COUNT     = 0x08,
	AP_ITEM_PCBANG_TYPE_USE_ONLY_TPACK    = 0x10,
	AP_ITEM_PCBANG_TYPE_USE_ONLY_GPREMIUM = 0x20,
};

enum ap_item_section_type {
	AP_ITEM_SECTION_TYPE_NONE = 0x00,
	AP_ITEM_SECTION_TYPE_UNABLE_USE_EPIC_ZONE = 0x01,
	AP_ITEM_SECTION_TYPE_COUNT
};

enum ap_item_grid_pos {
	AP_ITEM_GRID_POS_TAB = 0,
	AP_ITEM_GRID_POS_ROW,
	AP_ITEM_GRID_POS_COLUMN,
	AP_ITEM_GRID_POS_COUNT
};

enum ap_item_trade_status {
	AP_ITEM_TRADE_STATUS_NONE = 0,
	AP_ITEM_TRADE_STATUS_WAIT_CONFIRM,
	AP_ITEM_TRADE_STATUS_TRADING,
	AP_ITEM_TRADE_STATUS_LOCK,
	AP_ITEM_TRADE_STATUS_READY_TO_EXCHANGE
};

enum ap_item_bound_type {
	AP_ITEM_NOT_BOUND = 0,
	AP_ITEM_BIND_ON_ACQUIRE,
	AP_ITEM_BIND_ON_EQUIP,
	AP_ITEM_BIND_ON_USE,
	AP_ITEM_BIND_ON_GUILDMASTER,
};

enum ap_item_equip_flag_bits {
	AP_ITEM_EQUIP_BY_ITEM_IN_USE = 0x01,
	AP_ITEM_EQUIP_WITH_NO_STATS = 0x02,
	AP_ITEM_EQUIP_SILENT = 0x04,
};

/** \brief Character factor adjustment setting. */
enum ap_item_character_factor_adjust {
	/** \brief Do not adjust. */
	AP_ITEM_CHARACTER_FACTOR_ADJUST_NONE,
	/** \brief Adjust based on combat equipment. */
	AP_ITEM_CHARACTER_FACTOR_ADJUST_COMBAT,
	/** \brief Adjust based on mount equipment. */
	AP_ITEM_CHARACTER_FACTOR_ADJUST_MOUNT,
};

enum ap_item_data_column_id {
	AP_ITEM_DCID_TID,
	AP_ITEM_DCID_ITEMNAME,
	AP_ITEM_DCID_LEVEL,
	AP_ITEM_DCID_SUITABLELEVELMIN,
	AP_ITEM_DCID_SUITABLELEVELMAX,
	AP_ITEM_DCID_CLASS,
	AP_ITEM_DCID_RACE,
	AP_ITEM_DCID_GENDER,
	AP_ITEM_DCID_PART,
	AP_ITEM_DCID_KIND,
	AP_ITEM_DCID_ITEMTYPE,
	AP_ITEM_DCID_TYPE,
	AP_ITEM_DCID_SUBTYPE,
	AP_ITEM_DCID_EXTRATYPE,
	AP_ITEM_DCID_BOUNDTYPE,
	AP_ITEM_DCID_EVENTITEM,
	AP_ITEM_DCID_PCBANG_ONLY,
	AP_ITEM_DCID_VILLAIN_ONLY,
	AP_ITEM_DCID_GROUPID,
	AP_ITEM_DCID_DROPRATE,
	AP_ITEM_DCID_DROPRANK,
	AP_ITEM_DCID_FONT_COLOR,
	AP_ITEM_DCID_SPIRIT_TYPE,
	AP_ITEM_DCID_NPC_PRICE,
	AP_ITEM_DCID_PC_PRICE,
	AP_ITEM_DCID_NPC_CHARISMA_PRICE,
	AP_ITEM_DCID_NPC_SHRINE_COIN,
	AP_ITEM_DCID_PC_CHARISMA_PRICE,
	AP_ITEM_DCID_PC_SHRINE_COIN,
	AP_ITEM_DCID_NPC_ARENA_COIN,
	AP_ITEM_DCID_STACK,
	AP_ITEM_DCID_HAND,
	AP_ITEM_DCID_RANK,
	AP_ITEM_DCID_DUR,
	AP_ITEM_DCID_MINSOCKET,
	AP_ITEM_DCID_MAXSOCKET,
	AP_ITEM_DCID_MINOPTION,
	AP_ITEM_DCID_MAXOPTION,
	AP_ITEM_DCID_ATK_RANGE,
	AP_ITEM_DCID_ATK_SPEED,
	AP_ITEM_DCID_PHYSICAL_MIN_DAMAGE,
	AP_ITEM_DCID_PHYSICAL_MAX_DAMAGE,
	AP_ITEM_DCID_MAGIC_MIN_DAMAGE,
	AP_ITEM_DCID_WATER_MIN_DAMAGE,
	AP_ITEM_DCID_FIRE_MIN_DAMAGE,
	AP_ITEM_DCID_EARTH_MIN_DAMAGE,
	AP_ITEM_DCID_AIR_MIN_DAMAGE,
	AP_ITEM_DCID_POISON_MIN_DAMAGE,
	AP_ITEM_DCID_ICE_MIN_DAMAGE,
	AP_ITEM_DCID_LINGHTNING_MIN_DAMAGE,
	AP_ITEM_DCID_MAGIC_MAX_DAMAGE,
	AP_ITEM_DCID_WATER_MAX_DAMAGE,
	AP_ITEM_DCID_FIRE_MAX_DAMAGE,
	AP_ITEM_DCID_EARTH_MAX_DAMAGE,
	AP_ITEM_DCID_AIR_MAX_DAMAGE,
	AP_ITEM_DCID_POISON_MAX_DAMAGE,
	AP_ITEM_DCID_ICE_MAX_DAMAGE,
	AP_ITEM_DCID_LINGHTNING_MAX_DAMAGE,
	AP_ITEM_DCID_PHYSICAL_DEFENSE_POINT,
	AP_ITEM_DCID_PHYSICAL_ATKRATE,
	AP_ITEM_DCID_MAGIC_ATKRATE_,
	AP_ITEM_DCID_PDR,
	AP_ITEM_DCID_BLOCKRATE,
	AP_ITEM_DCID_MAGIC_DEFENSE_POINT,
	AP_ITEM_DCID_WATER_DEFENSE_POINT,
	AP_ITEM_DCID_FIRE_DEFENSE_POINT,
	AP_ITEM_DCID_EARTH_DEFENSE_POINT,
	AP_ITEM_DCID_AIR_DEFENSE_POINT,
	AP_ITEM_DCID_POISON_DEFENSE_POINT,
	AP_ITEM_DCID_ICE_DEFENSE_POINT,
	AP_ITEM_DCID_LINGHTNING_DEFENSE_POINT,
	AP_ITEM_DCID_MAGIC_DEFENSE_RATE,
	AP_ITEM_DCID_WATER_DEFENSE_RATE,
	AP_ITEM_DCID_FIRE_DEFENSE_RATE,
	AP_ITEM_DCID_EARTH_DEFENSE_RATE,
	AP_ITEM_DCID_AIR_DEFENSE_RATE,
	AP_ITEM_DCID_POISON_DEFENSE_RATE,
	AP_ITEM_DCID_ICE_DEFENSE_RATE,
	AP_ITEM_DCID_LINGHTNING_DEFENSE_RATE,
	AP_ITEM_DCID_CON_BONUS_POINT,
	AP_ITEM_DCID_STR_BONUS_POINT,
	AP_ITEM_DCID_DEX_BONUS_POINT,
	AP_ITEM_DCID_INT_BONUS_POINT,
	AP_ITEM_DCID_WIS_BONUS_POINT,
	AP_ITEM_DCID_APPLY_EFFECT_COUNT,
	AP_ITEM_DCID_APPLY_EFFECT_TIME,
	AP_ITEM_DCID_USE_INTERVAL,
	AP_ITEM_DCID_HP,
	AP_ITEM_DCID_SP,
	AP_ITEM_DCID_MP,
	AP_ITEM_DCID_RUNE_ATTRIBUTE,
	AP_ITEM_DCID_USE_SKILL_ID,
	AP_ITEM_DCID_USE_SKILL_LEVEL,
	AP_ITEM_DCID_POLYMORPH_ID,
	AP_ITEM_DCID_POLYMORPH_DUR,
	AP_ITEM_DCID_FIRSTCATEGORY,
	AP_ITEM_DCID_FIRSTCATEGORYNAME,
	AP_ITEM_DCID_SECONDCATEGORY,
	AP_ITEM_DCID_SECONDCATEGORYNAME,
	AP_ITEM_DCID_PHYSICALRANK,
	AP_ITEM_DCID_HP_BUFF,
	AP_ITEM_DCID_MP_BUFF,
	AP_ITEM_DCID_ATTACK_BUFF,
	AP_ITEM_DCID_DEFENSE_BUFF,
	AP_ITEM_DCID_RUN_BUFF,
	AP_ITEM_DCID_CASH,
	AP_ITEM_DCID_DESTINATION,
	AP_ITEM_DCID_REMAINTIME,
	AP_ITEM_DCID_EXPIRETIME,
	AP_ITEM_DCID_CLASSIFY_ID,
	AP_ITEM_DCID_CANSTOPUSINGITEM,
	AP_ITEM_DCID_CASHITEMUSETYPE,
	AP_ITEM_DCID_ENABLEONRIDE,
	AP_ITEM_DCID_BUYER_TYPE,
	AP_ITEM_DCID_USING_TYPE,
	AP_ITEM_DCID_OPTIONTID,
	AP_ITEM_DCID_POTIONTYPE2,
	AP_ITEM_DCID_LINK_ID,
	AP_ITEM_DCID_SKILL_PLUS,
	AP_ITEM_DCID_GAMBLING,
	AP_ITEM_DCID_QUESTITEM,
	AP_ITEM_DCID_GACHA_TYPE_NUMER,
	AP_ITEM_DCID_GACHARANK,
	AP_ITEM_DCID_GACHA_POINT_TYPE,
	AP_ITEM_DCID_STAMINACURE,
	AP_ITEM_DCID_REMAINPETSTAMINA,
	AP_ITEM_DCID_GACHAMINLV,
	AP_ITEM_DCID_GACHAMAXLV,
	AP_ITEM_DCID_ITEM_SECTION_NUM,
	AP_ITEM_DCID_HEROIC_MIN_DAMAGE,
	AP_ITEM_DCID_HEROIC_MAX_DAMAGE,
	AP_ITEM_DCID_HEROIC_DEFENSE_POINT,
};

enum ap_item_option_data_column_id {
	AP_ITEM_OPTION_DCID_TID,
	AP_ITEM_OPTION_DCID_DESCRIPTION,
	AP_ITEM_OPTION_DCID_UPPER_BODY,
	AP_ITEM_OPTION_DCID_LOWER_BODY,
	AP_ITEM_OPTION_DCID_WEAPON,
	AP_ITEM_OPTION_DCID_SHIELD,
	AP_ITEM_OPTION_DCID_HEAD,
	AP_ITEM_OPTION_DCID_RING,
	AP_ITEM_OPTION_DCID_NECKLACE,
	AP_ITEM_OPTION_DCID_SHOES,
	AP_ITEM_OPTION_DCID_GLOVES,
	AP_ITEM_OPTION_DCID_REFINED,
	AP_ITEM_OPTION_DCID_GACHA,
	AP_ITEM_OPTION_DCID_TYPE,
	AP_ITEM_OPTION_DCID_POINTTYPE,
	AP_ITEM_OPTION_DCID_ITEM_LEVEL_MIN,
	AP_ITEM_OPTION_DCID_ITEM_LEVEL_MAX,
	AP_ITEM_OPTION_DCID_LEVEL_LIMIT,
	AP_ITEM_OPTION_DCID_RANK_MIN,
	AP_ITEM_OPTION_DCID_RANK_MAX,
	AP_ITEM_OPTION_DCID_PROBABILITY,
	AP_ITEM_OPTION_DCID_SKILL_LEVEL,
	AP_ITEM_OPTION_DCID_SKILL_ID,
	AP_ITEM_OPTION_DCID_ATK_RANGE_BONUS,
	AP_ITEM_OPTION_DCID_ATK_SPEED_BONUS,
	AP_ITEM_OPTION_DCID_MOVEMENT_RUN_BONUS,
	AP_ITEM_OPTION_DCID_PHYSICALDAMAGE,
	AP_ITEM_OPTION_DCID_PHYSICALATTACKRESIST,
	AP_ITEM_OPTION_DCID_SKILLBLOCKRATE,
	AP_ITEM_OPTION_DCID_MAGICDAMAGE,
	AP_ITEM_OPTION_DCID_WATERDAMAGE,
	AP_ITEM_OPTION_DCID_FIREDAMAGE,
	AP_ITEM_OPTION_DCID_EARTHDAMAGE,
	AP_ITEM_OPTION_DCID_AIRDAMAGE,
	AP_ITEM_OPTION_DCID_POISONDAMAGE,
	AP_ITEM_OPTION_DCID_ICEDAMAGE,
	AP_ITEM_OPTION_DCID_LINGHTNINGDAMAGE,
	AP_ITEM_OPTION_DCID_FIRE_RES,
	AP_ITEM_OPTION_DCID_WATER_RES,
	AP_ITEM_OPTION_DCID_AIR_RES,
	AP_ITEM_OPTION_DCID_EARTH_RES,
	AP_ITEM_OPTION_DCID_MAGIC_RES,
	AP_ITEM_OPTION_DCID_POISON_RES,
	AP_ITEM_OPTION_DCID_ICE_RES,
	AP_ITEM_OPTION_DCID_THUNDER_RES,
	AP_ITEM_OPTION_DCID_PHYSICAL_DEFENSE_POINT,
	AP_ITEM_OPTION_DCID_PHYSICAL_ATKRATE,
	AP_ITEM_OPTION_DCID_MAGIC_ATKRATE_,
	AP_ITEM_OPTION_DCID_BLOCKRATE,
	AP_ITEM_OPTION_DCID_MAGIC_DEFENSE_POINT,
	AP_ITEM_OPTION_DCID_WATER_DEFENSE_POINT,
	AP_ITEM_OPTION_DCID_FIRE_DEFENSE_POINT,
	AP_ITEM_OPTION_DCID_EARTH_DEFENSE_POINT,
	AP_ITEM_OPTION_DCID_AIR_DEFENSE_POINT,
	AP_ITEM_OPTION_DCID_POISON_DEFENSE_POINT,
	AP_ITEM_OPTION_DCID_ICE_DEFENSE_POINT,
	AP_ITEM_OPTION_DCID_LINGHTNING_DEFENSE_POINT,
	AP_ITEM_OPTION_DCID_HP_BONUS_POINT,
	AP_ITEM_OPTION_DCID_MP_BONUS_POINT,
	AP_ITEM_OPTION_DCID_HP_BONUS_PERCENT,
	AP_ITEM_OPTION_DCID_MP_BONUS_PERCENT,
	AP_ITEM_OPTION_DCID_STR_BONUS_POINT,
	AP_ITEM_OPTION_DCID_DEX_BONUS_POINT,
	AP_ITEM_OPTION_DCID_INT_BONUS_POINT,
	AP_ITEM_OPTION_DCID_CON_BONUS_POINT,
	AP_ITEM_OPTION_DCID_WIS_BONUS_POINT,
	AP_ITEM_OPTION_DCID_PHYSICALRANK,
	AP_ITEM_OPTION_DCID_SKILL_TYPE,
	AP_ITEM_OPTION_DCID_DURATION,
	AP_ITEM_OPTION_DCID_SKILL_RATE,
	AP_ITEM_OPTION_DCID_CRITICAL,
	AP_ITEM_OPTION_DCID_STUN_TIME,
	AP_ITEM_OPTION_DCID_DAMAGE_CONVERT_HP,
	AP_ITEM_OPTION_DCID_REGEN_HP,
	AP_ITEM_OPTION_DCID_REGEN_MP,
	AP_ITEM_OPTION_DCID_DOT_DAMAGE_TIME,
	AP_ITEM_OPTION_DCID_SKILL_CAST,
	AP_ITEM_OPTION_DCID_SKILL_DELAY,
	AP_ITEM_OPTION_DCID_SKILL_LEVELUP,
	AP_ITEM_OPTION_DCID_BONUS_EXP,
	AP_ITEM_OPTION_DCID_BONUS_MONEY,
	AP_ITEM_OPTION_DCID_BONUS_DROP_RATE,
	AP_ITEM_OPTION_DCID_EVADE_RATE,
	AP_ITEM_OPTION_DCID_DODGE_RATE,
	AP_ITEM_OPTION_DCID_VISIBLE,
	AP_ITEM_OPTION_DCID_LEVEL_MIN,
	AP_ITEM_OPTION_DCID_LEVEL_MAX,
	AP_ITEM_OPTION_DCID_LINK_ID,
	AP_ITEM_OPTION_DCID_HEROIC_MIN_DAMAGE,
	AP_ITEM_OPTION_DCID_HEROIC_MAX_DAMAGE,
	AP_ITEM_OPTION_DCID_HEROIC_DEFENSE_POINT,
	AP_ITEM_OPTION_DCID_IGNORE_PHYSICAL_DEFENSE_RATE,
	AP_ITEM_OPTION_DCID_IGNORE_ATTRIBUTE_DEFENSE_RATE,
	AP_ITEM_OPTION_DCID_CRITICAL_DEFENSE_RATE,
	AP_ITEM_OPTION_DCID_REINFORCEMENTRATE,
	AP_ITEM_OPTION_DCID_IGNORE_PHYSICALATTACKRESIST,
	AP_ITEM_OPTION_DCID_IGNORE_BLOCKRATE,
	AP_ITEM_OPTION_DCID_IGNORE_SKILLBLOCKRATE,
	AP_ITEM_OPTION_DCID_IGNORE_EVADERATE,
	AP_ITEM_OPTION_DCID_IGNORE_DODGERATE,
	AP_ITEM_OPTION_DCID_IGNORE_CRITICALDEFENSERATE,
	AP_ITEM_OPTION_DCID_IGNORE_STUNDEFENSERATE,
	AP_ITEM_OPTION_DCID_ADD_DEBUFF_DURATION,
};

enum ap_item_update_flag_bits {
	AP_ITEM_UPDATE_NONE           = 0,
	AP_ITEM_UPDATE_TID            = 0x0001,
	AP_ITEM_UPDATE_STACK_COUNT    = 0x0002,
	AP_ITEM_UPDATE_STATUS         = 0x0004,
	AP_ITEM_UPDATE_STATUS_FLAGS   = 0x0008,
	AP_ITEM_UPDATE_GRID_POS       = 0x0010,
	AP_ITEM_UPDATE_DURABILITY     = 0x0020,
	AP_ITEM_UPDATE_MAX_DURABILITY = 0x0040,
	AP_ITEM_UPDATE_OPTION_TID     = 0x0080,
	AP_ITEM_UPDATE_IN_USE         = 0x0100,
	AP_ITEM_UPDATE_REMAIN_TIME    = 0x0200,
	AP_ITEM_UPDATE_EXPIRE_TIME    = 0x0400,
	AP_ITEM_UPDATE_FACTORS        = 0x0800,
	AP_ITEM_UPDATE_ALL            = 0x0FFF,
};

enum ap_item_module_data_index {
	AP_ITEM_MDI_ITEM,
	AP_ITEM_MDI_TEMPLATE,
	AP_ITEM_MDI_RUNE_TEMPLATE,
	AP_ITEM_MDI_OPTION_TEMPLATE,
	AP_ITEM_MDI_AVATAR_SET,
	AP_ITEM_MDI_COUNT
};

enum ap_item_packet_type {
	AP_ITEM_PACKET_TYPE_ADD = 0,
	AP_ITEM_PACKET_TYPE_REMOVE,
	AP_ITEM_PACKET_TYPE_UPDATE,
	AP_ITEM_PACKET_TYPE_USE_ITEM,
	AP_ITEM_PACKET_TYPE_USE_ITEM_BY_TID,
	AP_ITEM_PACKET_TYPE_USE_ITEM_FAILED_BY_TID,
	AP_ITEM_PACKET_TYPE_USE_RETURN_SCROLL,
	AP_ITEM_PACKET_TYPE_USE_RETURN_SCROLL_FAILED,
	AP_ITEM_PACKET_TYPE_CANCEL_RETURN_SCROLL,
	AP_ITEM_PACKET_TYPE_ENABLE_RETURN_SCROLL,
	AP_ITEM_PACKET_TYPE_DISABLE_RETURN_SCROLL,
	AP_ITEM_PACKET_TYPE_PICKUP_ITEM,
	AP_ITEM_PACKET_TYPE_PICKUP_ITEM_RESULT,
	AP_ITEM_PACKET_TYPE_EGO_ITEM,
	AP_ITEM_PACKET_TYPE_STACK_ITEM,
	AP_ITEM_PACKET_TYPE_DROP_MONEY,
	AP_ITEM_PACKET_TYPE_USE_ITEM_SUCCESS,
	AP_ITEM_PACKET_TYPE_SPLIT_ITEM,
	AP_ITEM_PACKET_TYPE_UPDATE_REUSE_TIME_FOR_REVERSE_ORB,
	AP_ITEM_PACKET_TYPE_UPDATE_REUSE_TIME_FOR_TRANSFORM,
	AP_ITEM_PACKET_TYPE_INIT_TIME_FOR_TRANSFORM,
	AP_ITEM_PACKET_TYPE_REQUEST_DESTROY_ITEM,
	AP_ITEM_PACKET_TYPE_REQUEST_BUY_BANK_SLOT,
	AP_ITEM_PACKET_TYPE_UNUSE_ITEM,
	AP_ITEM_PACKET_TYPE_UNUSE_ITEM_FAILED,
	AP_ITEM_PACKET_TYPE_PAUSE_ITEM,
	AP_ITEM_PACKET_TYPE_UPDATE_ITEM_USE_TIME,
	AP_ITEM_PACKET_TYPE_USE_ITEM_RESULT,
	AP_ITEM_PACKET_TYPE_UPDATE_COOLDOWN,
	AP_ITEM_PACKET_TYPE_UPDATE_STAMINA_REMAIN_TIME,
	AP_ITEM_PACKET_TYPE_CHANGE_AUTOPICK_ITEM,
	AP_ITEM_PACKET_TYPE_UPDATE_STATUS_FLAG = 0x21,
};

enum ap_item_pick_up_result {
	AP_ITEM_PICK_UP_RESULT_SUCCESS = 0,
	AP_ITEM_PICK_UP_RESULT_FAIL
};

enum ap_item_callback_id {
	AP_ITEM_CB_RECEIVE,
	AP_ITEM_CB_END_READ_OPTION,
	AP_ITEM_CB_READ_IMPORT,
	AP_ITEM_CB_END_READ_IMPORT,
	AP_ITEM_CB_EQUIP,
	AP_ITEM_CB_UNEQUIP,
};

struct ap_item_lottery_item {
	uint32_t item_tid;
	char name[AP_ITEM_MAX_NAME_LENGTH + 1];
	uint32_t min_stack_count;
	uint32_t max_stack_count;
	uint32_t rate;
};

struct ap_item_lottery_box {
	char name[AP_ITEM_MAX_NAME_LENGTH + 1];
	struct ap_item_lottery_item * items;
	uint32_t total_rate;
};

struct ap_item_avatar_set {
	struct ap_item_template * temp[AP_ITEM_MAX_AVATAR_SET_PART_COUNT];
	uint32_t count;
};

struct ap_item_template_equip {
	enum ap_item_part part;
	enum ap_item_equip_kind kind;
	enum ap_item_equip_weapon_type weapon_type;
};

struct ap_item_template_usable {
	uint16_t part;
	enum ap_item_usable_type usable_type;
	uint16_t usable_part;
	uint32_t use_interval;
	struct ap_factor effect_factor;
	uint32_t effect_apply_count;
	uint32_t effect_apply_interval;
	enum ap_item_usable_potion_type potion_type;
	enum ap_item_usable_potion_type2 potion_type2;
	boolean is_percent_potion;
	enum ap_item_usable_spirit_stone_type spirit_stone_type;
	enum ap_item_usable_teleport_scroll_type teleport_scroll_type;
	enum ap_item_rune_attribute rune_attribute_type;
	struct au_pos destination;
	uint32_t transform_duration;
	uint32_t transform_tid;
	uint32_t transform_add_max_hp;
	uint32_t transform_add_max_mp;
	uint32_t use_skill_tid;
	uint32_t use_skill_level;
	enum ap_item_usable_scroll_subtype scroll_subtype;
	enum ap_item_usable_area_chatting_type area_chatting_type;
	struct ap_item_lottery_box lottery_box;
	struct ap_item_avatar_set avatar_set;
};

struct ap_item_template_other {
	enum ap_item_other_type other_type;
	enum ap_item_other_etc_type etc_type;
	enum ap_item_other_skull_type skull_type;
};

struct ap_item_template {
	uint32_t tid;
	char name[AP_ITEM_MAX_NAME_LENGTH + 1];
	boolean is_stackable;
	uint32_t max_stackable_count;
	enum ap_item_type type;
	int32_t subtype;
	int32_t extra_type;
	int16_t price;
	struct ap_factor factor;
	struct ap_factor factor_restrict;
	int32_t first_category;
	char first_category_name[48];
	int32_t second_category;
	char second_category_name[48];
	uint32_t title_font_color;
	uint32_t status_flags;
	boolean free_duration;
	uint32_t min_socket_count;
	uint32_t max_socket_count;
	uint32_t min_option_count;
	uint32_t max_option_count;
	boolean is_event_item;
	boolean is_use_only_pc_bang;
	boolean is_villain_only;
	int32_t hp_buff;
	int32_t mp_buff;
	int32_t attack_buff;
	int32_t defense_buff;
	int32_t run_buff;
	enum ap_item_buyer_type buyer_type;
	enum ap_item_using_type using_type;
	enum ap_item_cash_item_type cash_item_type;
	boolean not_continuous;
	uint64_t remain_time;
	uint32_t expire_time;
	uint32_t classify_id;
	boolean can_stop_using;
	enum ap_item_cash_item_use_type cash_item_use_type;
	boolean enable_on_ride;
	uint8_t option_count;
	uint16_t option_tid[AP_ITEM_OPTION_MAX_COUNT];
	struct ap_item_option_template * options[AP_ITEM_OPTION_MAX_COUNT];
	uint16_t link_id[AP_ITEM_OPTION_MAX_LINK_COUNT];
	struct ap_item_option_link_pool * link_pools[AP_ITEM_OPTION_MAX_LINK_COUNT];
	uint8_t link_count;
	uint16_t skill_plus_tid[AP_ITEM_MAX_SKILL_PLUS];
	uint8_t skill_plus_count;
	boolean enable_gamble;
	uint32_t quest_group;
	uint32_t gacha_type;
	uint64_t stamina_cure;
	uint64_t stamina_remain_time;
	enum ap_item_section_type section_type;
	uint32_t npc_arena_coin;
	struct ap_item_template_equip equip;
	struct ap_item_template_usable usable;
	struct ap_item_template_other other;
};

struct ap_item {
	uint32_t id;
	uint32_t tid;
	struct ap_item_template * temp;
	uint32_t update_flags;
	uint32_t dimension;
	uint32_t character_id;
	uint32_t prev_owner;
	uint32_t stack_count;
	struct au_pos position;
	struct ap_factor factor;
	struct ap_factor factor_percent;
	enum ap_item_status new_status;
	enum ap_item_status status;
	uint32_t status_flags;
	uint16_t grid_pos[AP_ITEM_GRID_POS_COUNT];
	uint16_t prev_grid_pos[AP_ITEM_GRID_POS_COUNT];
	//AgpdGridItem* 	m_pcsGridItem;
	uint32_t equip_flags;
	uint64_t equip_tick;
	uint64_t remove_tick;
	uint32_t option_count;
	uint16_t option_tid[AP_ITEM_OPTION_MAX_COUNT];
	struct ap_item_option_template * options[AP_ITEM_OPTION_MAX_COUNT];
	uint16_t rune_option_tid[AP_ITEM_OPTION_MAX_RUNE_COUNT];
	struct ap_item_option_template * rune_options[AP_ITEM_OPTION_MAX_RUNE_COUNT];
	uint32_t current_link_level;
	uint16_t skill_plus_tid[AP_ITEM_MAX_SKILL_PLUS_EFFECT];
	boolean in_use;
	uint64_t remain_time;
	time_t expire_time;
	uint32_t cash_item_use_count;
	uint64_t stamina_remain_time;
	uint64_t recent_turn_off_tick;
};

struct ap_item_cooldown {
	uint32_t item_tid;
	uint64_t end_tick;
};

/** \brief struct ap_character attachment. */
struct ap_item_character {
	struct ap_grid * inventory;
	struct ap_grid * equipment;
	struct ap_grid * cash_inventory;
	/** \brief Pet inventory. */
	struct ap_grid * sub_inventory;
	struct ap_grid * bank;
	uint64_t recent_inventory_process_tick;
	enum ap_item_character_factor_adjust factor_adjust_attack;
	enum ap_item_character_factor_adjust factor_adjust_movement;
	float combat_weapon_min_damage;
	float combat_weapon_max_damage;
	int32_t combat_weapon_attack_speed;
	int32_t combat_weapon_attack_range;
	float mount_weapon_min_damage;
	float mount_weapon_max_damage;
	int32_t mount_weapon_attack_speed;
	int32_t mount_weapon_attack_range;
	int32_t mount_movement;
	int32_t mount_movement_fast;
	boolean is_return_scroll_enabled;
	struct au_pos return_scroll_position;
};

struct ap_item_skill_buff_attachment {
	uint32_t bound_item_id;
};

struct ap_item_grid_pos_in_packet {
	uint16_t layer;
	uint16_t row;
	uint16_t column;
};

struct ap_item_cb_receive {
	enum ap_item_packet_type type;
	enum ap_item_status status;
	uint32_t item_id;
	uint32_t stack_count;
	struct ap_item_grid_pos_in_packet * inventory;
	struct ap_item_grid_pos_in_packet * bank;
	void * user_data;
};

struct ap_item_cb_end_read_option {
	struct ap_item_option_template * temp;
};

/** \brief AP_ITEM_CB_READ_IMPORT callback data. */
struct ap_item_cb_read_import {
	struct ap_item_template * temp;
	enum ap_item_data_column_id column_id;
	const char * value;
};

struct ap_item_cb_end_read_import {
	struct ap_item_template * temp;
};

/** \brief AP_ITEM_CB_EQUIP callback data. */
struct ap_item_cb_equip {
	struct ap_character * character;
	struct ap_item * item;
};

/** \brief AP_ITEM_CB_UNEQUIP callback data. */
struct ap_item_cb_unequip {
	struct ap_character * character;
	struct ap_item * item;
};

struct ap_item_module * ap_item_create_module();

void ap_item_close_module(struct ap_item_module * mod);

void ap_item_shutdown(struct ap_item_module * mod);

boolean ap_item_read_import_data(
	struct ap_item_module * mod,
	const char * file_path, 
	boolean decrypt);

boolean ap_item_read_option_data(
	struct ap_item_module * mod,
	const char * file_path, 
	boolean decrypt);

boolean ap_item_read_lottery_box(
	struct ap_item_module * mod,
	const char * file_path, 
	boolean decrypt);

boolean ap_item_read_avatar_set(
	struct ap_item_module * mod,
	const char * file_path, 
	boolean decrypt);

void ap_item_add_callback(
	struct ap_item_module * mod,
	enum ap_item_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t ap_item_attach_data(
	struct ap_item_module * mod,
	enum ap_item_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

/**
 * Retrieve character item attachment.
 * \param[in] character Character pointer.
 * 
 * \return Character item attachment pointer.
 */
struct ap_item_character * ap_item_get_character(
	struct ap_item_module * mod,
	struct ap_character * character);

struct ap_item_skill_buff_attachment * ap_item_get_skill_buff_attachment(
	struct ap_item_module * mod,
	struct ap_skill_buff_list * buff);

void ap_item_add_stream_callback(
	struct ap_item_module * mod,
	enum ap_item_module_data_index data_index,
	const char * module_name,
	ap_module_t callback_module,
	ap_module_stream_read_t read_cb,
	ap_module_stream_write_t write_cb);

/**
 * Get equipment grid index.
 * \param[in] part Target equipment part.
 *
 * \return A valid grid index if successful.
 *         Otherwise UINT8_MAX.
 */
uint8_t ap_item_get_equip_index(enum ap_item_part part);

/**
 * Retrieve character equipment.
 * \param[in]  character    Character pointer.
 * \param[in]  part         Equipment part.
 * 
 * \return If part is valid and target equipment slot is occupied,
 *         return value will be an item pointer.
 *         Otherwise NULL.
 */
struct ap_item * ap_item_get_equipment(
	struct ap_item_module * mod,
	struct ap_character * character,
	enum ap_item_part part);

struct ap_item_template * ap_item_get_template(
	struct ap_item_module * mod,
	uint32_t tid);

struct ap_item_option_template * ap_item_get_option_template(
	struct ap_item_module * mod,
	uint16_t tid);

struct ap_item * ap_item_create(struct ap_item_module * mod, uint32_t tid);

void ap_item_free(struct ap_item_module * mod, struct ap_item * item);

/**
 * Finds item in character inventory.
 * \param[in] character  Character pointer.
 * \param[in] item_id    Item id.
 * \param[in] count      Number of valid item status.
 * \param[in] ...        Enumeration of \ref enum ap_item_status.
 *
 * \return Returns item pointer if an item with a matching 
 *         item id is found. Otherwise, returns NULL.
 */
struct ap_item * ap_item_find(
	struct ap_item_module * mod,
	struct ap_character * character,
	uint32_t item_id,
	uint32_t count, ...);

/**
 * Filters items in character inventory according to cash item type and 
 * returns first match.
 * \param[in] character  Character pointer.
 * \param[in] type       Cash item type.
 * \param[in] count      Number of valid item status.
 * \param[in] ...        Enumeration of \ref enum ap_item_status.
 *
 * \return Returns item pointer if an item with a matching 
 *         item id is found. Otherwise, returns NULL.
 */
struct ap_item * ap_item_find_by_cash_item_type(
	struct ap_item_module * mod,
	struct ap_character * character,
	enum ap_item_cash_item_type type,
	uint32_t count, ...);

/**
 * Filters items in character inventory that are in use according to 
 * cash item type and returns first match.
 * \param[in] character  Character pointer.
 * \param[in] type       Cash item type.
 * \param[in] count      Number of valid item status.
 * \param[in] ...        Enumeration of \ref enum ap_item_status.
 *
 * \return Returns item pointer if an item with a matching 
 *         item id is found. Otherwise, returns NULL.
 */
struct ap_item * ap_item_find_item_in_use_by_cash_item_type(
	struct ap_item_module * mod,
	struct ap_character * character,
	enum ap_item_cash_item_type type,
	uint32_t count, ...);

/**
 * Filters items in character inventory that are in use according to 
 * usable item type and returns first match.
 * \param[in] character  Character pointer.
 * \param[in] type       Usable item type.
 * \param[in] count      Number of valid item status.
 * \param[in] ...        Enumeration of \ref enum ap_item_status.
 *
 * \return Returns item pointer if an item with a matching 
 *         item id is found. Otherwise, returns NULL.
 */
struct ap_item * ap_item_find_item_in_use_by_usable_type(
	struct ap_item_module * mod,
	struct ap_character * character,
	enum ap_item_usable_type type,
	uint32_t count, ...);

/**
 * Filters usable items in character inventory without 
 * status flag and returns first match.
 * \param[in] character   Character pointer.
 * \param[in] item_tid    Item template id.
 * \param[in] status_flag Status flag bit.
 * \param[in] count       Number of valid item status.
 * \param[in] ...         Enumeration of \ref enum ap_item_status.
 *
 * \return Returns item pointer if an item with a matching 
 *         item id is found. Otherwise, returns NULL.
 */
struct ap_item * ap_item_find_usable_item_without_status_flag(
	struct ap_item_module * mod,
	struct ap_character * character,
	uint32_t item_tid,
	uint64_t status_flag,
	uint32_t count, ...);

/**
 * Find grid matching with item status.
 * \param[in] character   Character pointer.
 * \param[in] item_status Item status.
 *
 * \return If a matching grid is found, returns grid pointer.
 *         Otherwise, returns NULL.
 */
struct ap_grid * ap_item_get_character_grid(
	struct ap_item_module * mod,
	struct ap_character * character,
	enum ap_item_status item_status);

/**
 * Remove item from character item grid.
 * \param[in] character Character pointer.
 * \param[in] item      Item pointer.
 */
void ap_item_remove_from_grid(
	struct ap_item_module * mod,
	struct ap_character * character,
	struct ap_item * item);

uint32_t ap_item_get_free_slot_count(
	struct ap_item_module * mod,
	struct ap_character * character,
	enum ap_item_status item_status,
	uint32_t item_tid);

boolean ap_item_check_use_restriction(
	struct ap_item_module * mod,
	struct ap_item * item,
	struct ap_character * character);

/**
 * Check if character meets item equip restrictions.
 * \param[in] item      Item pointer.
 * \param[in] character Character pointer.
 *
 * \return TRUE if character meets item equip restrictions.
 *         Otherwise FALSE.
 */
boolean ap_item_check_equip_restriction(
	struct ap_item_module * mod,
	struct ap_item * item,
	struct ap_character * character);

/**
 * Retrieve other equipment to be unequiped 
 * together with target equipment.
 * \param[in]  character   Character pointer.
 * \param[in]  item        Item pointer.
 *
 * \return If there is any equipment that needs to be 
 *         unequiped together, return value will be 
 *         an item pointer. Otherwise NULL.
 */
struct ap_item * ap_item_to_be_unequiped_together(
	struct ap_item_module * mod,
	struct ap_character * character,
	struct ap_item * item);

/**
 * Retrieve item(s) to be unequiped for target item to be equiped.
 * \param[in]  character   Character pointer.
 * \param[in]  item        Item pointer.
 * \param[out] list First item to unequip.
 *
 * \return Number of items to unequip.
 */
uint32_t ap_item_to_be_unequiped(
	struct ap_item_module * mod,
	struct ap_character * character,
	struct ap_item * item,
	struct ap_item ** list);

/**
 * Equip item.
 * This function does not check if character meets item 
 * restrictions.
 * \param[in,out] character   Character pointer.
 * \param[in,out] item        Item pointer.
 * \param[in]     equip_flags Equipment bit flags.
 *
 * \return If target equipment slot is occupied, this function 
 *         will return FALSE.
 */
boolean ap_item_equip(
	struct ap_item_module * mod,
	struct ap_character * character,
	struct ap_item * item,
	uint32_t equip_flags);

boolean ap_item_unequip(
	struct ap_item_module * mod,
	struct ap_character * character, 
	struct ap_item * item);

void ap_item_apply_option(
	struct ap_item_module * mod,
	struct ap_character * c, 
	const struct ap_item_option_template * option,
	int modifier);

boolean ap_item_on_receive(
	struct ap_item_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_item_make_add_packet(
	struct ap_item_module * mod,
	const struct ap_item * item);

void ap_item_make_add_packet_buffer(
	struct ap_item_module * mod,
	const struct ap_item * item,
	void * buffer,
	uint16_t * length);

void ap_item_make_remove_packet(
	struct ap_item_module * mod,
	const struct ap_item * item);

void ap_item_make_update_packet(
	struct ap_item_module * mod,
	const struct ap_item * item,
	uint32_t update_flags);

void ap_item_make_use_item_by_tid_packet(
	struct ap_item_module * mod,
	uint32_t item_tid,
	uint32_t character_id,
	uint32_t target_id,
	uint32_t reuse_ms);

void ap_item_make_enable_return_scroll_packet(
	struct ap_item_module * mod,
	uint32_t character_id);

void ap_item_make_disable_return_scroll_packet(
	struct ap_item_module * mod,
	uint32_t character_id);

void ap_item_make_pick_up_item_result_packet(
	struct ap_item_module * mod,
	const struct ap_item * item,
	enum ap_item_pick_up_result result);

void ap_item_make_custom_pick_up_item_result_packet(
	struct ap_item_module * mod,
	uint32_t item_tid,
	uint32_t stack_count,
	enum ap_item_pick_up_result result);

void ap_item_make_update_reuse_time_for_reverse_orb_packet(
	struct ap_item_module * mod,
	uint32_t character_id,
	uint32_t reuse_time);

void ap_item_make_update_use_time_packet(
	struct ap_item_module * mod,
	const struct ap_item * item);

static inline void ap_item_set_stack_count(
	struct ap_item * item,
	uint32_t stack_count)
{
	assert(stack_count != 0);
	assert(stack_count <= AP_ITEM_MAX_STACK_COUNT);
	item->stack_count = stack_count;
	item->update_flags |= AP_ITEM_UPDATE_STACK_COUNT;
}

static inline void ap_item_increase_stack_count(
	struct ap_item * item,
	uint32_t amount)
{
	assert(item->stack_count + amount < AP_ITEM_MAX_STACK_COUNT);
	item->stack_count += amount;
	item->update_flags |= AP_ITEM_UPDATE_STACK_COUNT;
}

static inline void ap_item_decrease_stack_count(
	struct ap_item * item,
	uint32_t amount)
{
	assert(item->stack_count > amount);
	item->stack_count -= amount;
	item->update_flags |= AP_ITEM_UPDATE_STACK_COUNT;
}

static inline uint32_t ap_item_get_required_slot_count(
	const struct ap_item_template * temp,
	uint32_t stack_count)
{
	if (!temp->max_stackable_count)
		return stack_count;
	if (stack_count % temp->max_stackable_count)
		return ((stack_count / temp->max_stackable_count) + 1);
	else
		return (stack_count / temp->max_stackable_count);
}

static inline void ap_item_set_status(
	struct ap_item * item,
	enum ap_item_status status)
{
	item->status = status;
	item->update_flags |= AP_ITEM_UPDATE_STATUS;
}

static inline void ap_item_set_grid_pos(
	struct ap_item * item,
	uint16_t layer,
	uint16_t row,
	uint16_t col)
{
	item->grid_pos[AP_ITEM_GRID_POS_TAB] = layer;
	item->grid_pos[AP_ITEM_GRID_POS_ROW] = row;
	item->grid_pos[AP_ITEM_GRID_POS_COLUMN] = col;
	item->update_flags |= AP_ITEM_UPDATE_GRID_POS;
}

static inline uint32_t ap_item_get_empty_slot_count(
	struct ap_item_module * mod,
	struct ap_character * character,
	enum ap_item_status item_status)
{
	struct ap_grid * grid = ap_item_get_character_grid(mod, character, item_status);
	assert(grid != NULL);
	return ap_grid_get_empty_count(grid);
}

END_DECLS

#endif /* _AP_ITEM_H_ */
