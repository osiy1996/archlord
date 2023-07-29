#ifndef _AP_FACTORS_H_
#define _AP_FACTORS_H_

#include "public/ap_module_instance.h"

#define AP_FACTORS_MODULE_NAME "AgpmFactors"

#define AP_FACTORS_INI_START "FactorStart"
#define AP_FACTORS_INI_END "FactorEnd"

#define AP_FACTORS_TYPE_CHAR_STATUS_NAME_LENGTH 64
#define AP_FACTORS_TYPE_CHAR_STATUS_NAME_SET_MAX 20
#define AP_FACTORS_TYPE_CHAR_STATUS_NAME_SET_RACE "Race"
#define AP_FACTORS_TYPE_CHAR_STATUS_NAME_SET_CLASS "Class"
#define AP_FACTORS_TYPE_CHAR_STATUS_NAME_SET_GENDER "Gender"

#define AP_FACTORS_BIT_FACTOR_INIT_VALUE 0x80000000

#define AP_FACTORS_MAX_NUM_DATA_TYPE_NAME 20
#define AP_FACTORS_MAX_LENGTH_DATA_TYPE_NAME 32

#define AP_FACTORS_BIT_CHAR_STATUS    0x0000000000000001u
#define AP_FACTORS_BIT_MOVEMENT_SPEED 0x0000000100000001u
#define AP_FACTORS_BIT_CHAR_TYPE      0x0000000000000002u
#define AP_FACTORS_BIT_CHAR_POINT     0x0000000000000004u
#define AP_FACTORS_BIT_HP             0x0000200000000004u
#define AP_FACTORS_BIT_MP             0x0000400000000004u
#define AP_FACTORS_BIT_EXP            0x0000800000000004u
#define AP_FACTORS_BIT_BONUS_EXP      0x0001000000000004u
#define AP_FACTORS_BIT_CHAR_POINT_MAX 0x0000000000000008u
#define AP_FACTORS_BIT_MAX_HP         0x0000000200000008u
#define AP_FACTORS_BIT_MAX_MP         0x0000000400000008u
#define AP_FACTORS_BIT_ATTACK_RATING  0x0000000800000008u
#define AP_FACTORS_BIT_DEFENSE_RATING 0x0000001000000008u
#define AP_FACTORS_BIT_CHAR_POINT_RR  0x0000000000000010u
#define AP_FACTORS_BIT_DAMAGE         0x0000000000000020u
#define AP_FACTORS_BIT_PHYSICAL_DMG   0x0000002000000020u
#define AP_FACTORS_BIT_HEROIC_DMG     0x0000004000000020u
#define AP_FACTORS_BIT_DEFENSE        0x0000000000000040u
#define AP_FACTORS_BIT_ATTACK         0x0000000000000080u
#define AP_FACTORS_BIT_ATTACK_RANGE   0x0000008000000080u
#define AP_FACTORS_BIT_ATTACK_SPEED   0x0000010000000080u
#define AP_FACTORS_BIT_ITEM           0x0000000000000100u
#define AP_FACTORS_BIT_PRICE          0x0000000000000200u
#define AP_FACTORS_BIT_DIRT           0x0000000000000400u
#define AP_FACTORS_BIT_SKILL_LEVEL    0x0000020000000400u
#define AP_FACTORS_BIT_SKILL_POINT    0x0000040000000400u
#define AP_FACTORS_BIT_SKILL_EXP      0x0000080000000400u
#define AP_FACTORS_BIT_HEROIC_POINT   0x0000100000000400u
#define AP_FACTORS_BIT_MASK           0x00000000000007FFu
#define AP_FACTORS_BIT_ALL            0xFFFFFFFF000007FFu
#define AP_FACTORS_BIT_END            0x0000000000000800u

BEGIN_DECLS

struct ap_factors_module;

enum ap_factors_type {
	AP_FACTORS_TYPE_NONE = -1,
	AP_FACTORS_TYPE_RESULT,
	AP_FACTORS_TYPE_CHAR_STATUS,
	AP_FACTORS_TYPE_CHAR_TYPE,
	AP_FACTORS_TYPE_CHAR_POINT,
	AP_FACTORS_TYPE_CHAR_POINT_MAX,
	AP_FACTORS_TYPE_CHAR_POINT_RECOVERY_RATE,
	AP_FACTORS_TYPE_ATTRIBUTE,
	AP_FACTORS_TYPE_DAMAGE,
	AP_FACTORS_TYPE_DEFENSE,
	AP_FACTORS_TYPE_ATTACK,
	AP_FACTORS_TYPE_ITEM,
	AP_FACTORS_TYPE_DIRT,
	AP_FACTORS_TYPE_PRICE,
	AP_FACTORS_TYPE_OWNER,
	AP_FACTORS_TYPE_AGRO,
	AP_FACTORS_TYPE_COUNT
};

enum ap_factors_char_status_type {
	AP_FACTORS_CHAR_STATUS_TYPE_CON = 0,
	AP_FACTORS_CHAR_STATUS_TYPE_STR,
	AP_FACTORS_CHAR_STATUS_TYPE_INT,
	AP_FACTORS_CHAR_STATUS_TYPE_DEX,
	AP_FACTORS_CHAR_STATUS_TYPE_CHA,
	AP_FACTORS_CHAR_STATUS_TYPE_LUK,
	AP_FACTORS_CHAR_STATUS_TYPE_WIS,
	AP_FACTORS_CHAR_STATUS_TYPE_LEVEL,
	AP_FACTORS_CHAR_STATUS_TYPE_MOVEMENT,
	AP_FACTORS_CHAR_STATUS_TYPE_MOVEMENT_FAST,
	AP_FACTORS_CHAR_STATUS_TYPE_UNION_RANK,
	AP_FACTORS_CHAR_STATUS_TYPE_MURDERER,
	AP_FACTORS_CHAR_STATUS_TYPE_MUKZA,
	AP_FACTORS_CHAR_STATUS_TYPE_BEFORELEVEL,
	AP_FACTORS_CHAR_STATUS_TYPE_LIMITEDLEVEL,
	AP_FACTORS_CHAR_STATUS_TYPE_COUNT
};

enum ap_factors_char_type_type {
	AP_FACTORS_CHAR_TYPE_TYPE_RACE = 0,
	AP_FACTORS_CHAR_TYPE_TYPE_GENDER,
	AP_FACTORS_CHAR_TYPE_TYPE_CLASS,
	AP_FACTORS_CHAR_TYPE_TYPE_COUNT
};

enum ap_factors_char_point_type {
	AP_FACTORS_CHAR_POINT_TYPE_HP = 0,
	AP_FACTORS_CHAR_POINT_TYPE_MP,
	AP_FACTORS_CHAR_POINT_TYPE_SP,
	AP_FACTORS_CHAR_POINT_TYPE_EXP_HIGH,
	AP_FACTORS_CHAR_POINT_TYPE_EXP_LOW,
	AP_FACTORS_CHAR_POINT_TYPE_AP,
	AP_FACTORS_CHAR_POINT_TYPE_MAP,
	AP_FACTORS_CHAR_POINT_TYPE_MI,
	AP_FACTORS_CHAR_POINT_TYPE_AGRO,
	AP_FACTORS_CHAR_POINT_TYPE_DMG_NORMAL,
	AP_FACTORS_CHAR_POINT_TYPE_DMG_ATTR_MAGIC,
	AP_FACTORS_CHAR_POINT_TYPE_DMG_ATTR_WATER,
	AP_FACTORS_CHAR_POINT_TYPE_DMG_ATTR_FIRE,
	AP_FACTORS_CHAR_POINT_TYPE_DMG_ATTR_EARTH,
	AP_FACTORS_CHAR_POINT_TYPE_DMG_ATTR_AIR,
	AP_FACTORS_CHAR_POINT_TYPE_DMG_ATTR_POISON,
	AP_FACTORS_CHAR_POINT_TYPE_DMG_ATTR_LIGHTENING,
	AP_FACTORS_CHAR_POINT_TYPE_DMG_ATTR_ICE,
	AP_FACTORS_CHAR_POINT_TYPE_BONUS_EXP,
	AP_FACTORS_CHAR_POINT_TYPE_DMG_HEROIC,
	AP_FACTORS_CHAR_POINT_TYPE_COUNT
};

enum ap_factors_char_point_max_type {
	AP_FACTORS_CHAR_POINT_MAX_TYPE_HP = 0,
	AP_FACTORS_CHAR_POINT_MAX_TYPE_MP,
	AP_FACTORS_CHAR_POINT_MAX_TYPE_SP,
	AP_FACTORS_CHAR_POINT_MAX_TYPE_EXP_HIGH,
	AP_FACTORS_CHAR_POINT_MAX_TYPE_EXP_LOW,
	AP_FACTORS_CHAR_POINT_MAX_TYPE_AP,
	AP_FACTORS_CHAR_POINT_MAX_TYPE_MAP,
	AP_FACTORS_CHAR_POINT_MAX_TYPE_MI,
	AP_FACTORS_CHAR_POINT_MAX_TYPE_AR,
	AP_FACTORS_CHAR_POINT_MAX_TYPE_DR,
	AP_FACTORS_CHAR_POINT_MAX_TYPE_MAR,
	AP_FACTORS_CHAR_POINT_MAX_TYPE_MDR,
	AP_FACTORS_CHAR_POINT_MAX_TYPE_BASE_EXP,
	AP_FACTORS_CHAR_POINT_MAX_TYPE_ADD_MAX_HP,
	AP_FACTORS_CHAR_POINT_MAX_TYPE_ADD_MAX_MP,
	AP_FACTORS_CHAR_POINT_MAX_TYPE_ADD_MAX_SP,
	AP_FACTORS_CHAR_POINT_MAX_TYPE_COUNT
};

enum ap_factors_char_point_recovery_rate_type {
	AP_FACTORS_CHAR_POINT_RECOVERY_RATE_TYPE_HP = 0,
	AP_FACTORS_CHAR_POINT_RECOVERY_RATE_TYPE_MP,
	AP_FACTORS_CHAR_POINT_RECOVERY_RATE_TYPE_SP,
	AP_FACTORS_CHAR_POINT_RECOVERY_RATE_TYPE_COUNT
};

enum ap_factors_attribute_type {
	AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL = 0,
	AP_FACTORS_ATTRIBUTE_TYPE_MAGIC,
	AP_FACTORS_ATTRIBUTE_TYPE_WATER,
	AP_FACTORS_ATTRIBUTE_TYPE_FIRE,
	AP_FACTORS_ATTRIBUTE_TYPE_EARTH,
	AP_FACTORS_ATTRIBUTE_TYPE_AIR,
	AP_FACTORS_ATTRIBUTE_TYPE_POISON,
	AP_FACTORS_ATTRIBUTE_TYPE_LIGHTENING,
	AP_FACTORS_ATTRIBUTE_TYPE_ICE,
	AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL_BLOCK,
	AP_FACTORS_ATTRIBUTE_TYPE_SKILL_BLOCK,
	AP_FACTORS_ATTRIBUTE_TYPE_HEROIC,
	AP_FACTORS_ATTRIBUTE_TYPE_HEROIC_MELEE,
	AP_FACTORS_ATTRIBUTE_TYPE_HEROIC_RANGE,
	AP_FACTORS_ATTRIBUTE_TYPE_HEROIC_MAGIC,
	AP_FACTORS_ATTRIBUTE_TYPE_COUNT
};

enum ap_factors_damage_type {
	AP_FACTORS_DAMAGE_TYPE_MIN = 0,
	AP_FACTORS_DAMAGE_TYPE_MAX,
	AP_FACTORS_DAMAGE_TYPE_COUNT
};

enum ap_factors_defense_type {
	AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT = 0,
	AP_FACTORS_DEFENSE_TYPE_DEFENSE_RATE,
	AP_FACTORS_DEFENSE_TYPE_COUNT
};

enum ap_factors_attack_type {
	AP_FACTORS_ATTACK_TYPE_ATTACKRANGE = 0,
	AP_FACTORS_ATTACK_TYPE_HITRANGE,
	AP_FACTORS_ATTACK_TYPE_SPEED,
	AP_FACTORS_ATTACK_TYPE_SKILL_CAST,
	AP_FACTORS_ATTACK_TYPE_SKILL_DELAY,
	AP_FACTORS_ATTACK_TYPE_HIT_RATE,
	AP_FACTORS_ATTACK_TYPE_EVADE_RATE,
	AP_FACTORS_ATTACK_TYPE_DODGE_RATE,
	AP_FACTORS_ATTACK_TYPE_COUNT
};

enum ap_factors_item_type {
	AP_FACTORS_ITEM_TYPE_DURABILITY = 0,
	AP_FACTORS_ITEM_TYPE_HAND,
	AP_FACTORS_ITEM_TYPE_RANK,
	AP_FACTORS_ITEM_TYPE_PHYSICAL_RANK,
	AP_FACTORS_ITEM_TYPE_MAX_DURABILITY,
	AP_FACTORS_ITEM_TYPE_GACHA,
	AP_FACTORS_ITEM_TYPE_COUNT
};

enum ap_factors_price_type {
	AP_FACTORS_PRICE_TYPE_NPC_PRICE = 0,
	AP_FACTORS_PRICE_TYPE_PC_PRICE,
	AP_FACTORS_PRICE_TYPE_MONEY_HIGH,
	AP_FACTORS_PRICE_TYPE_MONEY_LOW,
	AP_FACTORS_PRICE_TYPE_COUNT
};

enum ap_factors_dirt_type {
	AP_FACTORS_DIRT_TYPE_DURATION = 0,
	AP_FACTORS_DIRT_TYPE_INTENSITY,
	AP_FACTORS_DIRT_TYPE_RANGE,
	AP_FACTORS_DIRT_TYPE_TARGET,
	AP_FACTORS_DIRT_TYPE_SKILL_LEVEL,
	AP_FACTORS_DIRT_TYPE_SKILL_POINT,
	AP_FACTORS_DIRT_TYPE_SKILL_EXP,
	AP_FACTORS_DIRT_TYPE_HEROIC_POINT,
	AP_FACTORS_DIRT_TYPE_COUNT
};

enum ap_factors_owner_type {
	AP_FACTORS_OWNER_TYPE_ID = 0,
	AP_FACTORS_OWNER_TYPE_OWNER,
	AP_FACTORS_OWNER_TYPE_COUNT
};

enum ap_factors_agro_type {
	AP_FACTORS_AGRO_TYPE_STATIC = 0,
	AP_FACTORS_AGRO_TYPE_XP_STATIC,
	AP_FACTORS_AGRO_TYPE_SLOW,
	AP_FACTORS_AGRO_TYPE_FAST,
	AP_FACTORS_AGRO_TYPE_AGRO,
	AP_FACTORS_AGRO_TYPE_COUNT
};

struct ap_factors_char_status {
	float con;
	float str;
	float intel;
	float agi;
	float cha;
	float luk;
	float wis;
	uint32_t level;
	int32_t movement;
	int32_t movement_fast;
	uint32_t union_rank;
	uint32_t murderer;
	uint32_t mukza;
	uint32_t before_level;
	uint32_t limited_level;
};

struct ap_factors_char_type {
	uint32_t race;
	uint32_t gender;
	uint32_t cs;
};

struct ap_factors_char_point {
	uint32_t hp;
	uint32_t mp;
	uint32_t sp;
	uint32_t exp_high;
	uint32_t exp_low;
	uint32_t ap;
	uint32_t map;
	uint32_t mi;
	uint32_t agro;
	int32_t dmg_normal;
	int32_t dmg_attr_magic;
	int32_t dmg_attr_water;
	int32_t dmg_attr_fire;
	int32_t dmg_attr_earth;
	int32_t dmg_attr_air;
	int32_t dmg_attr_poison;
	int32_t dmg_attr_lightning;
	int32_t dmg_attr_ice;
	uint32_t bonus_exp;
	int32_t dmg_heroic;
};

struct ap_factors_char_point_max {
	uint32_t hp;
	uint32_t mp;
	uint32_t sp;
	uint32_t exp_high;
	uint32_t exp_low;
	uint32_t ap;
	uint32_t map;
	uint32_t mi;
	int32_t attack_rating;
	int32_t defense_rating;
	int32_t mar;
	int32_t mdr;
	uint32_t base_exp;
	uint32_t add_max_hp;
	uint32_t add_max_mp;
	uint32_t add_max_sp;
};

struct ap_factors_char_point_recovery_rate {
	uint32_t hp;
	uint32_t mp;
	uint32_t sp;
};

struct ap_factors_attribute {
	float physical;
	float magic;
	float water;
	float fire;
	float earth;
	float air;
	float poison;
	float lightning;
	float ice;
	float physical_block;
	float skill_block;
	float heroic;
	float heroic_melee;
	float heroic_ranged;
	float heroic_magic;
};

struct ap_factors_damage {
	struct ap_factors_attribute min;
	struct ap_factors_attribute max;
};

struct ap_factors_defense {
	struct ap_factors_attribute point;
	struct ap_factors_attribute rate;
};

struct ap_factors_attack {
	int32_t attack_range;
	int32_t hit_range;
	int32_t attack_speed;
	int32_t skill_cast;
	int32_t skill_cooldown;
	int32_t accuracy;
	int32_t evade_rate;
	int32_t dodge_rate;
};

struct ap_factors_item {
	uint32_t durability;
	uint32_t hand;
	uint32_t rank;
	uint32_t physical_rank;
	uint32_t max_durability;
	uint32_t gacha;
};

struct ap_factors_price {
	uint32_t npc_price;
	uint32_t pc_price;
	uint32_t money_high;
	uint32_t money_low;
};

struct ap_factors_dirt {
	uint32_t duration;
	uint32_t intensity;
	uint32_t range;
	uint32_t target;
	uint32_t skill_level;
	int32_t skill_point;
	uint32_t skill_exp;
	int32_t heroic_point;
};

struct ap_factors_owner {
	uint32_t id;
	uint32_t owner;
};

struct ap_factors_agro {
	uint32_t statc;
	uint32_t xp_static;
	uint32_t slow;
	uint32_t fast;
	uint32_t agro;
};

struct ap_factors_detail {
	enum ap_factors_type factor_type;
	uint32_t count;
	size_t offset[20];
};

/* 
 * In the original implementation, factors are 
 * implemented in an unnecessarily complicated and 
 * inefficient way. Due to separate allocations and 
 * various indirections, multiple cache misses 
 * will occur in order to access a single value.
 * 
 * During each frame, we will read/write thousands of 
 * factor values (movement, combat, skill casts, etc.).
 * As such, using if statements and indirections will 
 * cause a considerable performance loss as well as high 
 * latency between frames (i.e. lag).
 *
 * That being the case, we will instead store all factor 
 * data as part of the structure, and access these values 
 * directly, without get/set functions.
 */
struct ap_factor {
	boolean is_point;
	void * factors[AP_FACTORS_TYPE_COUNT];
	struct ap_factors_char_status char_status;
	struct ap_factors_char_type char_type;
	struct ap_factors_char_point char_point;
	struct ap_factors_char_point_max char_point_max;
	struct ap_factors_char_point_recovery_rate char_point_recovery_rate;
	struct ap_factors_attribute attribute;
	struct ap_factors_damage damage;
	struct ap_factors_defense defense;
	struct ap_factors_attack attack;
	struct ap_factors_item item;
	struct ap_factors_price price;
	struct ap_factors_dirt dirt;
	struct ap_factors_owner owner;
	struct ap_factors_agro agro;
};

struct ap_factors_module * ap_factors_create_module();

boolean ap_factors_read_char_type(
	struct ap_factors_module * mod,
	const char * file_path,
	boolean decrypt);

boolean ap_factors_get_character_race(
	struct ap_factors_module * mod,
	const char * name,
	enum au_race_type * race);

boolean ap_factors_get_character_class(
	struct ap_factors_module * mod,
	const char * name,
	enum au_char_class_type * class_);

void ap_factors_enable(
	struct ap_factor * factor,
	enum ap_factors_type type);

void ap_factors_disable(
	struct ap_factor * factor,
	enum ap_factors_type type);

void ap_factors_set_value(
	struct ap_factor * factor, 
	enum ap_factors_type type, 
	uint32_t subtype, 
	const char * value);

void ap_factors_set_attribute(
	struct ap_factor * factor,
	enum ap_factors_type type,
	uint32_t subtype,
	enum ap_factors_attribute_type attr_type,
	const char * value);

boolean ap_factors_stream_read(
	struct ap_factors_module * mod,
	struct ap_factor * factor,
	struct ap_module_stream * stream);

void ap_factors_copy(
	struct ap_factor * dst, 
	const struct ap_factor * src);

void ap_factors_make_packet(
	struct ap_factors_module * mod,
	void * buffer,
	const struct ap_factor * factor,
	uint64_t bits);

void ap_factors_make_result_packet(
	struct ap_factors_module * mod,
	void * buffer,
	const struct ap_factor * factor,
	uint64_t bits);

void ap_factors_make_char_view_packet(
	struct ap_factors_module * mod,
	void * buffer,
	const struct ap_factor * factor);

void ap_factors_make_damage_packet(
	struct ap_factors_module * mod,
	void * buffer, 
	const struct ap_factors_char_point * factor);

void ap_factors_make_result_damage_packet(
	struct ap_factors_module * mod,
	void * buffer, 
	const struct ap_factors_char_point * factor);

void ap_factors_make_skill_result_packet(
	struct ap_factors_module * mod,
	void * buffer, 
	const struct ap_factor * factor);

inline void ap_factors_set_current_exp(
	struct ap_factor * factor,
	uint64_t exp)
{
	factor->char_point.exp_low = (uint32_t)exp;
	factor->char_point.exp_high = (uint32_t)(exp >> 32);
}

inline uint64_t ap_factors_get_current_exp(
	const struct ap_factor * factor)
{
	return factor->char_point.exp_low | 
		((uint64_t)factor->char_point.exp_high << 32);
}

inline void ap_factors_set_max_exp(
	struct ap_factor * factor,
	uint64_t exp)
{
	factor->char_point_max.exp_low = (uint32_t)exp;
	factor->char_point_max.exp_high = (uint32_t)(exp >> 32);
}

inline uint64_t ap_factors_get_max_exp(
	const struct ap_factor * factor)
{
	return factor->char_point_max.exp_low | 
		((uint64_t)factor->char_point_max.exp_high << 32);
}

END_DECLS

#endif /* _AP_FACTORS_H_ */
