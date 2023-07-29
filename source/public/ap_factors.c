#include "public/ap_factors.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_define.h"
#include "public/ap_module.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"

#include <assert.h>
#include <stdlib.h>

#define CONVERTFLOAT(F) (int32_t)((F) * 100.0f)

#define CHAR_TYPE_NAME_SIZE 64
#define CHAR_TYPE_MAX 20
#define CHAR_TYPE_RACE "Race"
#define CHAR_TYPE_CLASS "Class"
#define CHAR_TYPE_GENDER "Gender"

struct ap_factors_module {
	struct ap_module_instance instance;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
	struct au_packet factor_packets[AP_FACTORS_TYPE_COUNT];
	char gender_type_name[AU_CHARGENDER_TYPE_COUNT][CHAR_TYPE_NAME_SIZE];
	char race_type_name[CHAR_TYPE_MAX][CHAR_TYPE_NAME_SIZE];
	char class_type_name[CHAR_TYPE_MAX][AU_CHARCLASS_TYPE_COUNT][CHAR_TYPE_NAME_SIZE];
};

static const char * g_IniName[AP_FACTORS_TYPE_COUNT][2] = {
	{ NULL, NULL },
	{ "CharStatusStart", "CharStatusEnd" },
	{ "CharTypeStart", "CharTypeEnd" },
	{ "CharPointStart", "CharPointEnd" },
	{ "CharPointMaxStart", "CharPointMaxEnd" },
	{ "CharPointRecoveryRateStart", "CharPointRecoveryRateEnd" },
	{ NULL, NULL }, /* Attribute */
	{ "DamageStart", "DamageEnd" },
	{ "DefenseStart", "DefenseEnd" },
	{ "AttackStart", "AttackEnd" },
	{ "ItemStart", "ItemEnd" },
	{ "DIRTStart", "DIRTEnd" },
	{ "PriceStart", "PriceEnd" },
	{ NULL, NULL }, /* Owner */
	{ NULL, NULL } }; /* Agro */

static const char * g_IniFactorName[AP_FACTORS_TYPE_COUNT][20] = {
	{ NULL},
	{ "Con", "Str", "Int", "Dex", "Cha", "Luk", "Wis", "Level", "Movement", "MovementFast"},
	{ "Race", "Gender", "Class"},
	{ "Hp", "Mp", "Sp", "Exp", "ExpHigh", "AttackPoint", "MagicAttackPoint", "MagicIntensity", "Agro"},
	{ "Hp", "Mp", "Sp", "Exp", "ExpHigh", "AttackPoint", "MagicAttackPoint", "MagicIntensity", "AR", "DR", "BaseEXP"},
	{ "Hp", "Mp", "Sp"},
	{ "Physical", "Magic", "Water", "Fire", "Earth", "AIR", "Poison", "Lightening", "Ice", "PhysicalBlock", "Skill", "Heroic", "Heroic_Melee", "Heroic_Ranged", "Heroic_Magic"},
	{ "MinDamage", "MaxDamage"},
	{ "DefensePoint", "DefenseRate"},
	{ "AttackRange", "HitRange", "Speed", "SkillCast", "SkillDelay", "HitRate", "EvadeRate", "DodgeRate"},
	{ "Durability", "Hand", "Rank", "PhysicalRank", "MaxDurability"},
	{ "Duration", "Intensity", "Range", "Target"},
	{ "Price", "PCPrice"} };

static struct ap_factors_detail g_Detail[AP_FACTORS_TYPE_COUNT] = {
	{ AP_FACTORS_TYPE_RESULT, 1 },
	{ AP_FACTORS_TYPE_NONE, AP_FACTORS_CHAR_STATUS_TYPE_COUNT },
	{ AP_FACTORS_TYPE_NONE, AP_FACTORS_CHAR_TYPE_TYPE_COUNT },
	{ AP_FACTORS_TYPE_NONE, AP_FACTORS_CHAR_POINT_TYPE_COUNT },
	{ AP_FACTORS_TYPE_NONE, AP_FACTORS_CHAR_POINT_MAX_TYPE_COUNT },
	{ AP_FACTORS_TYPE_NONE, AP_FACTORS_CHAR_POINT_RECOVERY_RATE_TYPE_COUNT },
	{ AP_FACTORS_TYPE_NONE, AP_FACTORS_ATTRIBUTE_TYPE_COUNT },
	{ AP_FACTORS_TYPE_ATTRIBUTE, AP_FACTORS_DAMAGE_TYPE_COUNT, 
		{ offsetof(struct ap_factor, damage.min),
		  offsetof(struct ap_factor, damage.max) }, },
	{ AP_FACTORS_TYPE_ATTRIBUTE, AP_FACTORS_DEFENSE_TYPE_COUNT,
		{ offsetof(struct ap_factor, defense.point),
		  offsetof(struct ap_factor, defense.rate) }, },
	{ AP_FACTORS_TYPE_NONE, AP_FACTORS_ATTACK_TYPE_COUNT },
	{ AP_FACTORS_TYPE_NONE, AP_FACTORS_ITEM_TYPE_COUNT },
	{ AP_FACTORS_TYPE_NONE, AP_FACTORS_DIRT_TYPE_COUNT },
	{ AP_FACTORS_TYPE_NONE, AP_FACTORS_PRICE_TYPE_COUNT },
	{ AP_FACTORS_TYPE_NONE, AP_FACTORS_OWNER_TYPE_COUNT },
	{ AP_FACTORS_TYPE_NONE, AP_FACTORS_AGRO_TYPE_COUNT } };

static boolean invalid_type(enum ap_factors_type type)
{
	assert(!(type < 0 || type >= AP_FACTORS_TYPE_COUNT));
	return (type < 0 || type >= AP_FACTORS_TYPE_COUNT);
}

static void make_attr_packet(
	struct ap_factors_module * mod,
	void * buffer, 
	const struct ap_factors_attribute * attr)
{
	int32_t converted[] = {
		CONVERTFLOAT(attr->physical),
		CONVERTFLOAT(attr->magic),
		CONVERTFLOAT(attr->water),
		CONVERTFLOAT(attr->fire),
		CONVERTFLOAT(attr->earth),
		CONVERTFLOAT(attr->air),
		CONVERTFLOAT(attr->poison),
		CONVERTFLOAT(attr->lightning),
		CONVERTFLOAT(attr->ice),
		CONVERTFLOAT(attr->physical_block),
		CONVERTFLOAT(attr->skill_block),
		CONVERTFLOAT(attr->heroic),
		CONVERTFLOAT(attr->heroic_melee),
		CONVERTFLOAT(attr->heroic_ranged),
		CONVERTFLOAT(attr->heroic_magic) };
	au_packet_make_packet(
		&mod->factor_packets[AP_FACTORS_TYPE_ATTRIBUTE], 
		buffer, FALSE, NULL, 0,
		&converted[0], /* Physical */
		&converted[1], /* Magic */
		&converted[2], /* Water */
		&converted[3], /* Fire */
		&converted[4], /* Earth */
		&converted[5], /* Air */
		&converted[6], /* Poison */
		&converted[7], /* Lightening */
		&converted[8], /* Ice */
		&converted[9], /* Physical Block */
		&converted[10], /* Skill Block */
		&converted[11], /* Heroic */
		&converted[12], /* Heroic_Melee */
		&converted[13], /* Heroic_Ranged */
		&converted[14]); /* Heroic_Magic */
}

static void make_packet(
	struct ap_factors_module * mod,
	void * buffer, 
	const struct ap_factor * f,
	enum ap_factors_type type,
	uint64_t bits)
{
	switch (type) {
	case AP_FACTORS_TYPE_CHAR_STATUS: {
		int32_t converted[] = {
			CONVERTFLOAT(f->char_status.con),
			CONVERTFLOAT(f->char_status.str),
			CONVERTFLOAT(f->char_status.intel),
			CONVERTFLOAT(f->char_status.agi),
			CONVERTFLOAT(f->char_status.cha),
			CONVERTFLOAT(f->char_status.wis) };
		au_packet_make_packet(
			&mod->factor_packets[AP_FACTORS_TYPE_CHAR_STATUS], 
			buffer, FALSE, NULL, 0,
			&converted[0], /* con */
			&converted[1], /* str */
			&converted[2], /* int */
			&converted[3], /* dex */
			&converted[4], /* cha */
			NULL, /* luk */
			&converted[5], /* wis */
			&f->char_status.level, /* level */
			&f->char_status.movement, /* movement */
			&f->char_status.movement_fast, /* movement fast */
			NULL, /* union rank */
			&f->char_status.murderer, /* murderer point */
			NULL); /* mukza point */
		break;
	}
	case AP_FACTORS_TYPE_CHAR_TYPE:
		au_packet_make_packet(
			&mod->factor_packets[AP_FACTORS_TYPE_CHAR_TYPE], 
			buffer, FALSE, NULL, 0,
			&f->char_type.race, /* Race */ 
			&f->char_type.gender, /* Gender */ 
			&f->char_type.cs); /* Class */ 
		break;
	case AP_FACTORS_TYPE_CHAR_POINT: {
		uint64_t specificbits = (AP_FACTORS_BIT_HP | 
			AP_FACTORS_BIT_MP | 
			AP_FACTORS_BIT_EXP | 
			AP_FACTORS_BIT_BONUS_EXP) & 0xFFFFFFFF00000000;
		if (bits & specificbits) {
			au_packet_make_packet(
				&mod->factor_packets[AP_FACTORS_TYPE_CHAR_POINT], 
				buffer, FALSE, NULL, 0,
				CHECK_BIT(bits, AP_FACTORS_BIT_HP) ?
					&f->char_point.hp : NULL, /* HP */
				CHECK_BIT(bits, AP_FACTORS_BIT_MP) ?
					&f->char_point.mp : NULL, /* MP */
				NULL, /* SP */
				CHECK_BIT(bits, AP_FACTORS_BIT_EXP) ?
					&f->char_point.exp_high : NULL, /* EXPHIGH */
				CHECK_BIT(bits, AP_FACTORS_BIT_EXP) ?
					&f->char_point.exp_low : NULL, /* EXPLOW */
				NULL, /* Attack Point */
				NULL, /* Magic Attack Point */
				NULL, /* Magic Intensity */
				NULL, /* Agro Point */
				NULL, /* Dmg Normal */
				NULL, /* Dmg Attr Magic */
				NULL, /* Dmg Attr Water */
				NULL, /* Dmg Attr Fire */
				NULL, /* Dmg Attr Earth */
				NULL, /* Dmg Attr Air */
				NULL, /* Dmg Attr Poison */
				NULL, /* Dmg Attr Lightening */
				NULL, /* Dmg Attr Ice */
				CHECK_BIT(bits, AP_FACTORS_BIT_BONUS_EXP) ?
					&f->char_point.bonus_exp : NULL, /* Bonus Exp */
				NULL); /* Dmg Attr Heroic  */
		}
		else {
			au_packet_make_packet(
				&mod->factor_packets[AP_FACTORS_TYPE_CHAR_POINT], 
				buffer, FALSE, NULL, 0,
				&f->char_point.hp, /* HP */
				&f->char_point.mp, /* MP */
				NULL, /* SP */
				&f->char_point.exp_high, /* EXPHIGH */
				&f->char_point.exp_low, /* EXPLOW */
				NULL, /* Attack Point */
				NULL, /* Magic Attack Point */
				NULL, /* Magic Intensity */
				NULL, /* Agro Point */
				NULL, /* Dmg Normal */
				NULL, /* Dmg Attr Magic */
				NULL, /* Dmg Attr Water */
				NULL, /* Dmg Attr Fire */
				NULL, /* Dmg Attr Earth */
				NULL, /* Dmg Attr Air */
				NULL, /* Dmg Attr Poison */
				NULL, /* Dmg Attr Lightening */
				NULL, /* Dmg Attr Ice */
				NULL, /* Bonus Exp */
				NULL); /* Dmg Attr Heroic  */
		}
		break;
	}
	case AP_FACTORS_TYPE_CHAR_POINT_MAX:
		au_packet_make_packet(
			&mod->factor_packets[AP_FACTORS_TYPE_CHAR_POINT_MAX], 
			buffer, FALSE, NULL, 0,
			&f->char_point_max.hp, /* HP */
			&f->char_point_max.mp, /* MP */
			NULL, /* SP */
			&f->char_point_max.exp_high, /* EXPHIGH */
			&f->char_point_max.exp_low, /* EXPLOW */
			NULL, /* Attack Point */
			NULL, /* Magic Attack Point */
			NULL, /* Magic Intensity */
			&f->char_point_max.attack_rating, /* AR */
			&f->char_point_max.defense_rating, /* DR */
			NULL); /* Base EXP */
		break;
	case AP_FACTORS_TYPE_CHAR_POINT_RECOVERY_RATE:
		au_packet_make_packet(
			&mod->factor_packets[AP_FACTORS_TYPE_CHAR_POINT_RECOVERY_RATE], 
			buffer, FALSE, NULL, 0,
			&f->char_point_recovery_rate.hp, /* HP */
			&f->char_point_recovery_rate.mp, /* MP */
			NULL); /* SP */
		break;
	case AP_FACTORS_TYPE_DAMAGE: {
		void * dmin = ap_packet_get_temp_buffer(mod->ap_packet);
		void * dmax = ap_packet_get_temp_buffer(mod->ap_packet);
		make_attr_packet(mod, dmin, &f->damage.min);
		make_attr_packet(mod, dmax, &f->damage.max);
		au_packet_make_packet(
			&mod->factor_packets[AP_FACTORS_TYPE_DAMAGE], 
			buffer, FALSE, NULL, 0,
			dmin,
			dmax);
		break;
	}
	case AP_FACTORS_TYPE_DEFENSE: {
		void * point = ap_packet_get_temp_buffer(mod->ap_packet);
		void * rate = ap_packet_get_temp_buffer(mod->ap_packet);
		make_attr_packet(mod, point, &f->defense.point);
		make_attr_packet(mod, rate, &f->defense.rate);
		au_packet_make_packet(
			&mod->factor_packets[AP_FACTORS_TYPE_DEFENSE], 
			buffer, FALSE, NULL, 0,
			point,
			rate);
		break;
	}
	case AP_FACTORS_TYPE_ATTACK:
		au_packet_make_packet(
			&mod->factor_packets[AP_FACTORS_TYPE_ATTACK], 
			buffer, FALSE, NULL, 0,
			&f->attack.attack_range, /* attack range */
			&f->attack.hit_range, /* hit range */
			&f->attack.attack_speed, /* speed */
			&f->attack.skill_cast, /* skill cast */
			&f->attack.skill_cooldown, /* skill delay */
			&f->attack.accuracy, /* hit rate */
			&f->attack.evade_rate, /* evade rate */
			&f->attack.dodge_rate); /* dodge rate */
		break;
	case AP_FACTORS_TYPE_ITEM:
		au_packet_make_packet(
			&mod->factor_packets[AP_FACTORS_TYPE_ITEM], 
			buffer, FALSE, NULL, 0,
			&f->item.durability, /* durability */
			&f->item.hand, /* hand */
			&f->item.rank, /* rank */
			&f->item.physical_rank, /* physical rank */
			&f->item.max_durability); /* max durability */
		break;
	case AP_FACTORS_TYPE_DIRT: {
		uint64_t specificbits = (AP_FACTORS_BIT_SKILL_LEVEL |
			AP_FACTORS_BIT_SKILL_POINT |
			AP_FACTORS_BIT_SKILL_EXP |
			AP_FACTORS_BIT_HEROIC_POINT) & 0xFFFFFFFF00000000;
		if (bits & specificbits) {
			au_packet_make_packet(
				&mod->factor_packets[AP_FACTORS_TYPE_DIRT], 
				buffer, FALSE, NULL, 0,
				NULL, /* duration */
				NULL, /* intensity */
				NULL, /* range */
				NULL, /* target */
				CHECK_BIT(bits, AP_FACTORS_BIT_SKILL_LEVEL) ? 
					&f->dirt.skill_level : NULL,
				CHECK_BIT(bits, AP_FACTORS_BIT_SKILL_POINT) ? 
					&f->dirt.skill_point : NULL,
				CHECK_BIT(bits, AP_FACTORS_BIT_SKILL_EXP) ? 
					&f->dirt.skill_exp : NULL,
				CHECK_BIT(bits, AP_FACTORS_BIT_HEROIC_POINT) ? 
					&f->dirt.heroic_point : NULL);
		}
		else {
			au_packet_make_packet(
				&mod->factor_packets[AP_FACTORS_TYPE_DIRT], 
				buffer, FALSE, NULL, 0,
				NULL, /* duration */
				NULL, /* intensity */
				NULL, /* range */
				NULL, /* target */
				NULL, /* skill level */
				&f->dirt.skill_point,
				NULL, /* skill exp */
				&f->dirt.heroic_point);
		}
		break;
	}
	case AP_FACTORS_TYPE_PRICE:
		au_packet_make_packet(
			&mod->factor_packets[AP_FACTORS_TYPE_PRICE], 
			buffer, FALSE, NULL, 0,
			&f->price.npc_price, /* npc price */
			&f->price.pc_price, /* pc price */
			&f->price.money_high, /* money high */
			&f->price.money_low); /* money low */
		break;
	default:
		assert(0);
		break;
	}
}

static struct ap_factors_attribute * attr_from_detail(
	struct ap_factor * f,
	const struct ap_factors_detail * d,
	uint32_t subtype)
{
	return (struct ap_factors_attribute *)((uintptr_t)f + 
		d->offset[subtype]);
}

static void set_attribute(
	struct ap_factors_attribute * attribute, 
	uint32_t subtype, 
	const char * value)
{
	float f = strtof(value, NULL);
	switch (subtype) {
	case AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL:
		attribute->physical = f;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_MAGIC:
		attribute->magic = f;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_WATER:
		attribute->water = f;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_FIRE:
		attribute->fire = f;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_EARTH:
		attribute->earth = f;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_AIR:
		attribute->air = f;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_POISON:
		attribute->poison = f;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_LIGHTENING:
		attribute->lightning = f;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_ICE:
		attribute->ice = f;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL_BLOCK:
		attribute->physical_block = f;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_SKILL_BLOCK:
		attribute->skill_block = f;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_HEROIC:
		attribute->heroic = f;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_HEROIC_MELEE:
		attribute->heroic_melee = f;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_HEROIC_RANGE:
		attribute->heroic_ranged = f;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_HEROIC_MAGIC:
		attribute->heroic_magic = f;
		break;	
	default:
		assert(0);
		break;
	}
}

static boolean read_attribute(
	struct ap_factors_attribute * attribute,
	struct ap_module_stream * stream)
{
	const char * value_name;
	const char * value;
	uint32_t i;
	const struct ap_factors_detail * d = 
		&g_Detail[AP_FACTORS_TYPE_ATTRIBUTE];
	while (TRUE) {
		value_name = ap_module_stream_get_value_name(stream);
		value = ap_module_stream_get_value(stream);
		for (i = 0; i < d->count; i++) {
			if (!g_IniFactorName[AP_FACTORS_TYPE_ATTRIBUTE][i])
				continue;
			if (strcmp(g_IniFactorName[AP_FACTORS_TYPE_ATTRIBUTE][i], 
					value_name) == 0) {
				set_attribute(attribute, i, value);
				break;
			}
		}
		if (i == d->count || 
			!ap_module_stream_read_next_value(stream)) {
			break;
		}
	}
	return TRUE;
}

static boolean read_factor(
	struct ap_factor * factor,
	struct ap_module_stream * stream,
	enum ap_factors_type type)
{
	const char * value_name;
	const char * value;
	uint32_t i;
	if (g_Detail[type].factor_type == AP_FACTORS_TYPE_NONE) {
		while (TRUE) {
			value_name = ap_module_stream_get_value_name(stream);
			value = ap_module_stream_get_value(stream);
			for (i = 0; i < g_Detail[type].count; i++) {
				if (!g_IniFactorName[type][i])
					continue;
				if (strcmp(g_IniFactorName[type][i], 
						value_name) == 0) {
					ap_factors_set_value(factor, type, i, value);
					break;
				}
			}
			if (i == g_Detail[type].count || 
				!ap_module_stream_read_next_value(stream)) {
				break;
			}
		}
	}
	else if (g_Detail[type].factor_type == AP_FACTORS_TYPE_ATTRIBUTE) {
		while (TRUE) {
			value_name = ap_module_stream_get_value_name(stream);
			value = ap_module_stream_get_value(stream);
			for (i = 0; i < g_Detail[type].count; i++) {
				if (!g_IniFactorName[type][i])
					continue;
				if (strcmp(g_IniFactorName[type][i], 
						value_name) == 0) {
					struct ap_factors_attribute * attr =
						attr_from_detail(factor, 
							&g_Detail[type], i);
					if (!ap_module_stream_read_next_value(stream))
						return FALSE;
					if (!read_attribute(attr, stream))
						return FALSE;
					break;
				}
			}
			if (i == g_Detail[type].count || 
				!ap_module_stream_read_next_value(stream)) {
				break;
			}
		}
	}
	else {
		assert(0);
		return FALSE;
	}
	return TRUE;
}

boolean cbchartypestreamread(
	struct ap_factors_module * mod,
	struct type_name_set * set,
	struct ap_module_stream * stream)
{
	enum au_race_type racemap[CHAR_TYPE_MAX] = { 0 };
	uint32_t racecount = 0;
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = 
			ap_module_stream_get_value_name(stream);
		const char * value = ap_module_stream_get_value(stream);
		if (strcmp(value_name, CHAR_TYPE_GENDER) == 0) {
			char truncated[CHAR_TYPE_NAME_SIZE];
			uint32_t num;
			char name[CHAR_TYPE_NAME_SIZE];
			strlcpy(truncated, value, sizeof(truncated));
			if (sscanf(truncated, "%d:%s", &num, name) != 2) {
				ERROR("Failed to read gender (%s).", truncated);
				return FALSE;
			}
			if (num >= COUNT_OF(mod->gender_type_name)) {
				ERROR("Invalid character gender (%u).", num);
				return FALSE;
			}
			strlcpy(mod->gender_type_name[num], name,
				sizeof(mod->gender_type_name[num]));
		}
		else if (strcmp(value_name, CHAR_TYPE_RACE) == 0) {
			char truncated[CHAR_TYPE_NAME_SIZE];
			uint32_t race;
			char name[CHAR_TYPE_NAME_SIZE];
			strlcpy(truncated, value, sizeof(truncated));
			if (sscanf(truncated, "%d:%s", &race, name) != 2) {
				ERROR("Failed to read race (%s).", truncated);
				return FALSE;
			}
			if (race >= COUNT_OF(mod->race_type_name)) {
				ERROR("Invalid character race (%u).", race);
				return FALSE;
			}
			if (racecount >= COUNT_OF(racemap)) {
				ERROR("Too many character race types.");
				return FALSE;
			}
			strlcpy(mod->race_type_name[race], name,
				sizeof(mod->race_type_name[race]));
			racemap[racecount++] = race;
		}
		else if (strcmp(value_name, CHAR_TYPE_CLASS) == 0) {
			uint32_t cs = strtoul(value, NULL, 10);
			const char * cursor;
			char buffer[1024];
			char * token;
			uint32_t cscount = 0;
			if (cs >= COUNT_OF(mod->class_type_name)) {
				ERROR("Invalid character class (%u).", cs);
				return FALSE;
			}
			cursor = strchr(value, ':');
			if (!cursor++)
				continue;
			strlcpy(buffer, cursor, sizeof(buffer));
			token = strtok(buffer, ":");
			while (token) {
				uint32_t r;
				if (cscount >= racecount) {
					ERROR("Class type out of bounds.");
					return FALSE;
				}
				r = racemap[cscount++];
				strlcpy(mod->class_type_name[r][cs], token, 
					sizeof(mod->class_type_name[r][cs]));
				token = strtok(NULL, ":");
			}
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

static boolean onregister(
	struct ap_factors_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	return TRUE;
}

struct ap_factors_module * ap_factors_create_module()
{
	struct ap_factors_module * mod = ap_module_instance_new(AP_FACTORS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint16_t),
		AU_PACKET_TYPE_PACKET, 1, /* Result */ 
		AU_PACKET_TYPE_PACKET, 1, /* CharStatus */
		AU_PACKET_TYPE_PACKET, 1, /* CharType */
		AU_PACKET_TYPE_PACKET, 1, /* CharPoint */
		AU_PACKET_TYPE_PACKET, 1, /* CharPointMax */
		AU_PACKET_TYPE_PACKET, 1, /* CharPointRecoveryRate */
		AU_PACKET_TYPE_PACKET, 1, /* Attribute */
		AU_PACKET_TYPE_PACKET, 1, /* Damage */
		AU_PACKET_TYPE_PACKET, 1, /* Defense */
		AU_PACKET_TYPE_PACKET, 1, /* Attack */
		AU_PACKET_TYPE_PACKET, 1, /* Item */
		AU_PACKET_TYPE_PACKET, 1, /* DIRT */
		AU_PACKET_TYPE_PACKET, 1, /* Price */
		AU_PACKET_TYPE_END);
	au_packet_init(
		&mod->factor_packets[AP_FACTORS_TYPE_RESULT], 
		sizeof(uint8_t),
		AU_PACKET_TYPE_PACKET, 1, /* Result */ 
		AU_PACKET_TYPE_END);
	au_packet_init(
		&mod->factor_packets[AP_FACTORS_TYPE_CHAR_STATUS], 
		sizeof(uint16_t),
		AU_PACKET_TYPE_INT32, 1, /* con */
		AU_PACKET_TYPE_INT32, 1, /* str */
		AU_PACKET_TYPE_INT32, 1, /* int */
		AU_PACKET_TYPE_INT32, 1, /* dex */
		AU_PACKET_TYPE_INT32, 1, /* cha */
		AU_PACKET_TYPE_INT32, 1, /* luk */
		AU_PACKET_TYPE_INT32, 1, /* wis */
		AU_PACKET_TYPE_INT32, 1, /* level */
		AU_PACKET_TYPE_INT32, 1, /* movement */
		AU_PACKET_TYPE_INT32, 1, /* movement fast */
		AU_PACKET_TYPE_INT32, 1, /* union rank */
		AU_PACKET_TYPE_INT32, 1, /* murderer point */
		AU_PACKET_TYPE_INT32, 1, /* mukza point */
		AU_PACKET_TYPE_END);
	au_packet_init(
		&mod->factor_packets[AP_FACTORS_TYPE_CHAR_TYPE], 
		sizeof(uint8_t),
		AU_PACKET_TYPE_INT32, 1, /* Race */ 
		AU_PACKET_TYPE_INT32, 1, /* Gender */ 
		AU_PACKET_TYPE_INT32, 1, /* Class */ 
		AU_PACKET_TYPE_END);
	au_packet_init(
		&mod->factor_packets[AP_FACTORS_TYPE_CHAR_POINT], 
		sizeof(uint32_t),
		AU_PACKET_TYPE_INT32, 1, /* HP */
		AU_PACKET_TYPE_INT32, 1, /* MP */
		AU_PACKET_TYPE_INT32, 1, /* SP */
		AU_PACKET_TYPE_INT32, 1, /* EXPLOW */
		AU_PACKET_TYPE_INT32, 1, /* EXPHIGH */
		AU_PACKET_TYPE_INT32, 1, /* Attack Point */
		AU_PACKET_TYPE_INT32, 1, /* Magic Attack Point */
		AU_PACKET_TYPE_INT32, 1, /* Magic Intensity */
		AU_PACKET_TYPE_INT32, 1, /* Agro Point */
		AU_PACKET_TYPE_INT32, 1, /* Dmg Normal */
		AU_PACKET_TYPE_INT32, 1, /* Dmg Attr Magic */
		AU_PACKET_TYPE_INT32, 1, /* Dmg Attr Water */
		AU_PACKET_TYPE_INT32, 1, /* Dmg Attr Fire */
		AU_PACKET_TYPE_INT32, 1, /* Dmg Attr Earth */
		AU_PACKET_TYPE_INT32, 1, /* Dmg Attr Air */
		AU_PACKET_TYPE_INT32, 1, /* Dmg Attr Poison */
		AU_PACKET_TYPE_INT32, 1, /* Dmg Attr Lightening */
		AU_PACKET_TYPE_INT32, 1, /* Dmg Attr Ice */
		AU_PACKET_TYPE_INT32, 1, /* Bonus Exp */
		AU_PACKET_TYPE_INT32, 1, /* Dmg Attr Heroic  */
		AU_PACKET_TYPE_END);
	au_packet_init(
		&mod->factor_packets[AP_FACTORS_TYPE_CHAR_POINT_MAX], 
		sizeof(uint16_t),
		AU_PACKET_TYPE_INT32, 1, /* HP */
		AU_PACKET_TYPE_INT32, 1, /* MP */
		AU_PACKET_TYPE_INT32, 1, /* SP */
		AU_PACKET_TYPE_INT32, 1, /* EXPLOW */
		AU_PACKET_TYPE_INT32, 1, /* EXPHIGH */
		AU_PACKET_TYPE_INT32, 1, /* Attack Point */
		AU_PACKET_TYPE_INT32, 1, /* Magic Attack Point */
		AU_PACKET_TYPE_INT32, 1, /* Magic Intensity */
		AU_PACKET_TYPE_INT32, 1, /* AR */
		AU_PACKET_TYPE_INT32, 1, /* DR */
		AU_PACKET_TYPE_INT32, 1, /* Base EXP */
		AU_PACKET_TYPE_END);
	au_packet_init(
		&mod->factor_packets[AP_FACTORS_TYPE_CHAR_POINT_RECOVERY_RATE], 
		sizeof(uint8_t),
		AU_PACKET_TYPE_INT32, 1, /* HP */
		AU_PACKET_TYPE_INT32, 1, /* MP */
		AU_PACKET_TYPE_INT32, 1, /* SP */
		AU_PACKET_TYPE_END);
	au_packet_init(
		&mod->factor_packets[AP_FACTORS_TYPE_ATTRIBUTE], 
		sizeof(uint16_t),
		AU_PACKET_TYPE_INT32, 1, /* Physical */
		AU_PACKET_TYPE_INT32, 1, /* Magic */
		AU_PACKET_TYPE_INT32, 1, /* Water */
		AU_PACKET_TYPE_INT32, 1, /* Fire */
		AU_PACKET_TYPE_INT32, 1, /* Earth */
		AU_PACKET_TYPE_INT32, 1, /* Air */
		AU_PACKET_TYPE_INT32, 1, /* Poison */
		AU_PACKET_TYPE_INT32, 1, /* Lightening */
		AU_PACKET_TYPE_INT32, 1, /* Ice */
		AU_PACKET_TYPE_INT32, 1, /* Physical Block */
		AU_PACKET_TYPE_INT32, 1, /* Skill Block */
		AU_PACKET_TYPE_INT32, 1, /* Heroic */
		AU_PACKET_TYPE_INT32, 1, /* Heroic_Melee */
		AU_PACKET_TYPE_INT32, 1, /* Heroic_Ranged */
		AU_PACKET_TYPE_INT32, 1, /* Heroic_Magic */
		AU_PACKET_TYPE_END);
	au_packet_init(
		&mod->factor_packets[AP_FACTORS_TYPE_DAMAGE], 
		sizeof(uint8_t),
		AU_PACKET_TYPE_PACKET, 1, /* Min Damage */
		AU_PACKET_TYPE_PACKET, 1, /* Max Damage */
		AU_PACKET_TYPE_END);
	au_packet_init(
		&mod->factor_packets[AP_FACTORS_TYPE_DEFENSE], 
		sizeof(uint8_t),
		AU_PACKET_TYPE_PACKET, 1, /* Defense Point */
		AU_PACKET_TYPE_PACKET, 1, /* Defense Rate */
		AU_PACKET_TYPE_END);
	au_packet_init(
		&mod->factor_packets[AP_FACTORS_TYPE_ATTACK], 
		sizeof(uint8_t),
		AU_PACKET_TYPE_INT32, 1, /* attack range */
		AU_PACKET_TYPE_INT32, 1, /* hit range */
		AU_PACKET_TYPE_INT32, 1, /* speed */
		AU_PACKET_TYPE_INT32, 1, /* skill cast */
		AU_PACKET_TYPE_INT32, 1, /* skill delay */
		AU_PACKET_TYPE_INT32, 1, /* hit rate */
		AU_PACKET_TYPE_INT32, 1, /* evade rate */
		AU_PACKET_TYPE_INT32, 1, /* dodge rate */
		AU_PACKET_TYPE_END);
	au_packet_init(
		&mod->factor_packets[AP_FACTORS_TYPE_ITEM], 
		sizeof(uint8_t),
		AU_PACKET_TYPE_INT32, 1, /* durability */
		AU_PACKET_TYPE_INT32, 1, /* hand */
		AU_PACKET_TYPE_INT32, 1, /* rank */
		AU_PACKET_TYPE_INT32, 1, /* physical rank */
		AU_PACKET_TYPE_INT32, 1, /* max durability */
		AU_PACKET_TYPE_END);
	au_packet_init(
		&mod->factor_packets[AP_FACTORS_TYPE_DIRT], 
		sizeof(uint8_t),
		AU_PACKET_TYPE_INT32, 1, /* duration */
		AU_PACKET_TYPE_INT32, 1, /* intensity */
		AU_PACKET_TYPE_INT32, 1, /* range */
		AU_PACKET_TYPE_INT32, 1, /* target */
		AU_PACKET_TYPE_INT32, 1, /* skill level */
		AU_PACKET_TYPE_INT32, 1, /* skill point */
		AU_PACKET_TYPE_INT32, 1, /* skill exp */
		AU_PACKET_TYPE_INT32, 1, /* heroic point */
		AU_PACKET_TYPE_END);
	au_packet_init(
		&mod->factor_packets[AP_FACTORS_TYPE_PRICE], 
		sizeof(uint8_t),
		AU_PACKET_TYPE_INT32, 1, /* npc price */
		AU_PACKET_TYPE_INT32, 1, /* pc price */
		AU_PACKET_TYPE_INT32, 1, /* money high */
		AU_PACKET_TYPE_INT32, 1, /* money low */
		AU_PACKET_TYPE_END);
	au_packet_init(
		&mod->factor_packets[AP_FACTORS_TYPE_OWNER], 
		sizeof(uint8_t),
		AU_PACKET_TYPE_END);
	ap_module_stream_add_callback(mod, 0, AP_FACTORS_MODULE_NAME, mod,
		cbchartypestreamread, NULL);
	return mod;
}

boolean ap_factors_read_char_type(
	struct ap_factors_module * mod,
	const char * file_path,
	boolean decrypt)
{
	struct ap_module_stream * stream;
	uint32_t count;
	uint32_t i;
	stream = ap_module_stream_create();
	if (!ap_module_stream_open(stream, file_path, 0, decrypt)) {
		ERROR("Failed to open stream (%s).", file_path);
		ap_module_stream_destroy(stream);
		return FALSE;
	}
	count = ap_module_stream_get_section_count(stream);
	for (i = 0; i < count; i++) {
		uint32_t tid = strtoul(
			ap_module_stream_read_section_name(stream, i), NULL, 10);
		if (!ap_module_stream_enum_read(mod, stream, 0, mod)) {
			ERROR("Failed to read char type stream.");
			ap_module_stream_destroy(stream);
			return FALSE;
		}
	}
	ap_module_stream_destroy(stream);
	return TRUE;
}

boolean ap_factors_get_character_race(
	struct ap_factors_module * mod,
	const char * name,
	enum au_race_type * race)
{
	uint32_t i;
	if (strisempty(name))
		return FALSE;
	for (i = 0; i < AU_RACE_TYPE_COUNT; i++) {
		if (strcmp(mod->race_type_name[i], name) == 0) {
			*race = i;
			return TRUE;
		}
	}
	return FALSE;
}

boolean ap_factors_get_character_class(
	struct ap_factors_module * mod,
	const char * name,
	enum au_char_class_type * class_)
{
	uint32_t i;
	if (strisempty(name))
		return FALSE;
	for (i = 0; i < AU_RACE_TYPE_COUNT; i++) {
		uint32_t j;
		for (j = 0; j < AU_CHARCLASS_TYPE_COUNT; j++) {
			if (strcmp(mod->class_type_name[i][j], name) == 0) {
				*class_ = i;
				return TRUE;
			}
		}
	}
	return FALSE;
}

void ap_factors_enable(
	struct ap_factor * factor,
	enum ap_factors_type type)
{
	if (invalid_type(type))
		return;
	switch (type) {
	case AP_FACTORS_TYPE_CHAR_STATUS:
		factor->factors[type] = &factor->char_status;
		break;
	case AP_FACTORS_TYPE_CHAR_TYPE:
		factor->factors[type] = &factor->char_type;
		break;
	case AP_FACTORS_TYPE_CHAR_POINT:
		factor->factors[type] = &factor->char_point;
		break;
	case AP_FACTORS_TYPE_CHAR_POINT_MAX:
		factor->factors[type] = &factor->char_point_max;
		break;
	case AP_FACTORS_TYPE_CHAR_POINT_RECOVERY_RATE:
		factor->factors[type] = &factor->char_point_recovery_rate;
		break;
	case AP_FACTORS_TYPE_ATTRIBUTE:
		assert(0);
		factor->factors[type] = &factor->attribute;
		break;
	case AP_FACTORS_TYPE_DAMAGE:
		factor->factors[type] = &factor->damage;
		break;
	case AP_FACTORS_TYPE_DEFENSE:
		factor->factors[type] = &factor->defense;
		break;
	case AP_FACTORS_TYPE_ATTACK:
		factor->factors[type] = &factor->attack;
		break;
	case AP_FACTORS_TYPE_ITEM:
		factor->factors[type] = &factor->item;
		break;
	case AP_FACTORS_TYPE_PRICE:
		factor->factors[type] = &factor->price;
		break;
	case AP_FACTORS_TYPE_DIRT:
		factor->factors[type] = &factor->dirt;
		break;
	case AP_FACTORS_TYPE_OWNER:
		factor->factors[type] = &factor->owner;
		break;
	case AP_FACTORS_TYPE_AGRO:
		factor->factors[type] = &factor->agro;
		break;
	}
}

void ap_factors_disable(
	struct ap_factor * factor,
	enum ap_factors_type type)
{
	if (!invalid_type(type))
		factor->factors[type] = NULL;
}

void ap_factors_set_value(
	struct ap_factor * factor, 
	enum ap_factors_type type, 
	uint32_t subtype, 
	const char * value)
{
	int32_t d = strtol(value, NULL, 10);
	float f = strtof(value, NULL);
	switch (type) {
	case AP_FACTORS_TYPE_CHAR_STATUS:
		switch (subtype) {
		case AP_FACTORS_CHAR_STATUS_TYPE_CON:
			factor->char_status.con = f;
			break;
		case AP_FACTORS_CHAR_STATUS_TYPE_STR:
			factor->char_status.str = f;
			break;
		case AP_FACTORS_CHAR_STATUS_TYPE_INT:
			factor->char_status.intel = f;
			break;
		case AP_FACTORS_CHAR_STATUS_TYPE_DEX:
			factor->char_status.agi = f;
			break;
		case AP_FACTORS_CHAR_STATUS_TYPE_CHA:
			factor->char_status.cha = f;
			break;
		case AP_FACTORS_CHAR_STATUS_TYPE_LUK:
			factor->char_status.luk = f;
			break;
		case AP_FACTORS_CHAR_STATUS_TYPE_WIS:
			factor->char_status.wis = f;
			break;
		case AP_FACTORS_CHAR_STATUS_TYPE_LEVEL:
			factor->char_status.level = d;
			break;
		case AP_FACTORS_CHAR_STATUS_TYPE_MOVEMENT:
			factor->char_status.movement = d;
			break;
		case AP_FACTORS_CHAR_STATUS_TYPE_MOVEMENT_FAST:
			factor->char_status.movement_fast = d;
			break;
		case AP_FACTORS_CHAR_STATUS_TYPE_UNION_RANK:
			factor->char_status.union_rank = d;
			break;
		case AP_FACTORS_CHAR_STATUS_TYPE_MURDERER:
			factor->char_status.murderer = d;
			break;
		case AP_FACTORS_CHAR_STATUS_TYPE_MUKZA:
			factor->char_status.mukza = d;
			break;
		case AP_FACTORS_CHAR_STATUS_TYPE_BEFORELEVEL:
			factor->char_status.before_level = d;
			break;
		case AP_FACTORS_CHAR_STATUS_TYPE_LIMITEDLEVEL:
			factor->char_status.limited_level = d;
			break;
		default:
			assert(0);
			break;
		}
		break;
	case AP_FACTORS_TYPE_CHAR_TYPE:
		switch (subtype) {
		case AP_FACTORS_CHAR_TYPE_TYPE_RACE:
			factor->char_type.race = d;
			break;
		case AP_FACTORS_CHAR_TYPE_TYPE_GENDER:
			factor->char_type.gender = d;
			break;
		case AP_FACTORS_CHAR_TYPE_TYPE_CLASS:
			factor->char_type.cs = d;
			break;
		default:
			assert(0);
			break;
		}
		break;
	case AP_FACTORS_TYPE_CHAR_POINT:
		switch (subtype) {
		case AP_FACTORS_CHAR_POINT_TYPE_HP:
			factor->char_point.hp = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_MP:
			factor->char_point.mp = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_SP:
			factor->char_point.sp = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_EXP_HIGH:
			factor->char_point.exp_high = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_EXP_LOW:
			factor->char_point.exp_low = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_AP:
			factor->char_point.ap = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_MAP:
			factor->char_point.map = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_MI:
			factor->char_point.mi = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_AGRO:
			factor->char_point.agro = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_DMG_NORMAL:
			factor->char_point.dmg_normal = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_DMG_ATTR_MAGIC:
			factor->char_point.dmg_attr_magic = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_DMG_ATTR_WATER:
			factor->char_point.dmg_attr_water = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_DMG_ATTR_FIRE:
			factor->char_point.dmg_attr_fire = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_DMG_ATTR_EARTH:
			factor->char_point.dmg_attr_earth = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_DMG_ATTR_AIR:
			factor->char_point.dmg_attr_air = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_DMG_ATTR_POISON:
			factor->char_point.dmg_attr_poison = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_DMG_ATTR_LIGHTENING:
			factor->char_point.dmg_attr_lightning = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_DMG_ATTR_ICE:
			factor->char_point.dmg_attr_ice = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_BONUS_EXP:
			factor->char_point.bonus_exp = d;
			break;
		case AP_FACTORS_CHAR_POINT_TYPE_DMG_HEROIC:
			factor->char_point.dmg_heroic = d;
			break;
		default:
			assert(0);
			break;
		}
		break;
	case AP_FACTORS_TYPE_CHAR_POINT_MAX:
		switch (subtype) {
		case AP_FACTORS_CHAR_POINT_MAX_TYPE_HP:
			factor->char_point_max.hp = d;
			break;
		case AP_FACTORS_CHAR_POINT_MAX_TYPE_MP:
			factor->char_point_max.mp = d;
			break;
		case AP_FACTORS_CHAR_POINT_MAX_TYPE_SP:
			factor->char_point_max.sp = d;
			break;
		case AP_FACTORS_CHAR_POINT_MAX_TYPE_EXP_HIGH:
			factor->char_point_max.exp_high = d;
			break;
		case AP_FACTORS_CHAR_POINT_MAX_TYPE_EXP_LOW:
			factor->char_point_max.exp_low = d;
			break;
		case AP_FACTORS_CHAR_POINT_MAX_TYPE_AP:
			factor->char_point_max.ap = d;
			break;
		case AP_FACTORS_CHAR_POINT_MAX_TYPE_MAP:
			factor->char_point_max.map = d;
			break;
		case AP_FACTORS_CHAR_POINT_MAX_TYPE_MI:
			factor->char_point_max.mi = d;
			break;
		case AP_FACTORS_CHAR_POINT_MAX_TYPE_AR:
			factor->char_point_max.attack_rating = d;
			break;
		case AP_FACTORS_CHAR_POINT_MAX_TYPE_DR:
			factor->char_point_max.defense_rating = d;
			break;
		case AP_FACTORS_CHAR_POINT_MAX_TYPE_MAR:
			factor->char_point_max.mar = d;
			break;
		case AP_FACTORS_CHAR_POINT_MAX_TYPE_MDR:
			factor->char_point_max.mdr = d;
			break;
		case AP_FACTORS_CHAR_POINT_MAX_TYPE_BASE_EXP:
			factor->char_point_max.base_exp = d;
			break;
		case AP_FACTORS_CHAR_POINT_MAX_TYPE_ADD_MAX_HP:
			factor->char_point_max.add_max_hp = d;
			break;
		case AP_FACTORS_CHAR_POINT_MAX_TYPE_ADD_MAX_MP:
			factor->char_point_max.add_max_mp = d;
			break;
		case AP_FACTORS_CHAR_POINT_MAX_TYPE_ADD_MAX_SP:
			factor->char_point_max.add_max_sp = d;
			break;
		default:
			assert(0);
			break;
		}
		break;
	case AP_FACTORS_TYPE_CHAR_POINT_RECOVERY_RATE:
		switch (subtype) {
		case AP_FACTORS_CHAR_POINT_RECOVERY_RATE_TYPE_HP:
			factor->char_point_recovery_rate.hp = d;
			break;
		case AP_FACTORS_CHAR_POINT_RECOVERY_RATE_TYPE_MP:
			factor->char_point_recovery_rate.mp = d;
			break;
		case AP_FACTORS_CHAR_POINT_RECOVERY_RATE_TYPE_SP:
			factor->char_point_recovery_rate.sp = d;
			break;
		default:
			assert(0);
			break;
		}
		break;
	case AP_FACTORS_TYPE_ATTACK:
		switch (subtype) {
		case AP_FACTORS_ATTACK_TYPE_ATTACKRANGE:
			factor->attack.attack_range = d;
			break;
		case AP_FACTORS_ATTACK_TYPE_HITRANGE:
			factor->attack.hit_range = d;
			break;
		case AP_FACTORS_ATTACK_TYPE_SPEED:
			factor->attack.attack_speed = d;
			break;
		case AP_FACTORS_ATTACK_TYPE_SKILL_CAST:
			factor->attack.skill_cast = d;
			break;
		case AP_FACTORS_ATTACK_TYPE_SKILL_DELAY:
			factor->attack.skill_cooldown = d;
			break;
		case AP_FACTORS_ATTACK_TYPE_HIT_RATE:
			factor->attack.accuracy = d;
			break;
		case AP_FACTORS_ATTACK_TYPE_EVADE_RATE:
			factor->attack.evade_rate = d;
			break;
		case AP_FACTORS_ATTACK_TYPE_DODGE_RATE:
			factor->attack.dodge_rate = d;
			break;
		default:
			assert(0);
			break;
		}
		break;
	case AP_FACTORS_TYPE_ITEM:
		switch (subtype) {
		case AP_FACTORS_ITEM_TYPE_DURABILITY:
			factor->item.durability = d;
			break;
		case AP_FACTORS_ITEM_TYPE_HAND:
			factor->item.hand = d;
			break;
		case AP_FACTORS_ITEM_TYPE_RANK:
			factor->item.rank = d;
			break;
		case AP_FACTORS_ITEM_TYPE_PHYSICAL_RANK:
			factor->item.physical_rank = d;
			break;
		case AP_FACTORS_ITEM_TYPE_MAX_DURABILITY:
			factor->item.max_durability = d;
			break;
		case AP_FACTORS_ITEM_TYPE_GACHA:
			factor->item.gacha = d;
			break;
		default:
			assert(0);
			break;
		}
		break;
	case AP_FACTORS_TYPE_DIRT:
		switch (subtype) {
		case AP_FACTORS_DIRT_TYPE_DURATION:
			factor->dirt.duration = d;
			break;
		case AP_FACTORS_DIRT_TYPE_INTENSITY:
			factor->dirt.intensity = d;
			break;
		case AP_FACTORS_DIRT_TYPE_RANGE:
			factor->dirt.range = d;
			break;
		case AP_FACTORS_DIRT_TYPE_TARGET:
			factor->dirt.target = d;
			break;
		case AP_FACTORS_DIRT_TYPE_SKILL_LEVEL:
			factor->dirt.skill_level = d;
			break;
		case AP_FACTORS_DIRT_TYPE_SKILL_POINT:
			factor->dirt.skill_point = d;
			break;
		case AP_FACTORS_DIRT_TYPE_SKILL_EXP:
			factor->dirt.skill_exp = d;
			break;
		case AP_FACTORS_DIRT_TYPE_HEROIC_POINT:
			factor->dirt.heroic_point = d;
			break;
		default:
			assert(0);
			break;
		}
		break;
	case AP_FACTORS_TYPE_PRICE:
		switch (subtype) {
		case AP_FACTORS_PRICE_TYPE_NPC_PRICE:
			factor->price.npc_price = d;
			break;
		case AP_FACTORS_PRICE_TYPE_PC_PRICE:
			factor->price.pc_price = d;
			break;
		case AP_FACTORS_PRICE_TYPE_MONEY_HIGH:
			factor->price.money_high = d;
			break;
		case AP_FACTORS_PRICE_TYPE_MONEY_LOW:
			factor->price.money_low = d;
			break;
		default:
			assert(0);
			break;
		}
		break;
	case AP_FACTORS_TYPE_OWNER:
		switch (subtype) {
		default:
			assert(0);
			break;
		}
		break;
	case AP_FACTORS_TYPE_AGRO:
		switch (subtype) {
		default:
			assert(0);
			break;
		}
		break;
	default:
		assert(0);
		break;
	}
}

void ap_factors_set_attribute(struct ap_factor * factor,
	enum ap_factors_type type,
	uint32_t subtype,
	enum ap_factors_attribute_type attr_type,
	const char * value)
{
	switch (type) {
	case AP_FACTORS_TYPE_DAMAGE:
		switch (subtype) {
		case AP_FACTORS_DAMAGE_TYPE_MIN:
			set_attribute(&factor->damage.min, attr_type, value);
			break;
		case AP_FACTORS_DAMAGE_TYPE_MAX:
			set_attribute(&factor->damage.max, attr_type, value);
			break;
		default:
			assert(0);
			break;
		}
		break;
	case AP_FACTORS_TYPE_DEFENSE:
		switch (subtype) {
		case AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT:
			set_attribute(&factor->defense.point, attr_type, value);
			break;
		case AP_FACTORS_DEFENSE_TYPE_DEFENSE_RATE:
			set_attribute(&factor->defense.rate, attr_type, value);
			break;
		default:
			assert(0);
			break;
		}
		break;
	default:
		assert(0);
		break;
	}
}

boolean ap_factors_stream_read(
	struct ap_factors_module * mod,
	struct ap_factor * factor,
	struct ap_module_stream * stream)
{
	if (!ap_module_stream_read_next_value(stream))
		return FALSE;
	while (TRUE) {
		const char * value_name = 
			ap_module_stream_get_value_name(stream);
		uint32_t i;
		if (strcmp(value_name, AP_FACTORS_INI_END) == 0)
			break;
		for (i = 0; i < AP_FACTORS_TYPE_COUNT; i++) {
			if (g_IniName[i][0] && 
				strcmp(g_IniName[i][0], value_name) == 0) {
				if (!ap_module_stream_read_next_value(stream) ||
					!read_factor(factor, stream, i)) {
					return FALSE;
				}
			}
		}
		if (!ap_module_stream_read_next_value(stream))
			break;
	}
	return TRUE;
}

void ap_factors_copy(
	struct ap_factor * dst, 
	const struct ap_factor * src)
{
	uint32_t i;
	memcpy(dst, src, sizeof(*src));
	for (i = 0; i < AP_FACTORS_TYPE_COUNT; i++) {
		if (src->factors[i])
			ap_factors_enable(dst, i);
		else
			ap_factors_disable(dst, i);
	}
}

void ap_factors_make_packet(
	struct ap_factors_module * mod,
	void * buffer,
	const struct ap_factor * factor,
	uint64_t bits)
{
	void * charstatus = NULL;
	void * chartype = NULL;
	void * charpoint = NULL;
	void * charpointmax = NULL;
	void * charpointrr = NULL;
	void * damage = NULL;
	void * defense = NULL;
	void * attack = NULL;
	void * item = NULL;
	void * price = NULL;
	void * dirt = NULL;
	uint32_t count = 0;
	uint16_t length;
	if (bits & AP_FACTORS_BIT_CHAR_STATUS) {
		charstatus = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, charstatus, factor,
			AP_FACTORS_TYPE_CHAR_STATUS, bits);
	}
	if (bits & AP_FACTORS_BIT_CHAR_TYPE) {
		chartype = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, chartype, factor,
			AP_FACTORS_TYPE_CHAR_TYPE, bits);
	}
	if (bits & AP_FACTORS_BIT_CHAR_POINT) {
		charpoint = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, charpoint, factor,
			AP_FACTORS_TYPE_CHAR_POINT, bits);
	}
	if (bits & AP_FACTORS_BIT_CHAR_POINT_MAX) {
		charpointmax = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, charpointmax, factor,
			AP_FACTORS_TYPE_CHAR_POINT_MAX, bits);
	}
	if (bits & AP_FACTORS_BIT_DAMAGE) {
		damage = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, damage, factor,
			AP_FACTORS_TYPE_DAMAGE, bits);
	}
	if (bits & AP_FACTORS_BIT_DEFENSE) {
		defense = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, defense, factor,
			AP_FACTORS_TYPE_DEFENSE, bits);
	}
	if (bits & AP_FACTORS_BIT_ATTACK) {
		attack = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, attack, factor,
			AP_FACTORS_TYPE_ATTACK, bits);
	}
	if (bits & AP_FACTORS_BIT_ITEM) {
		item = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, item, factor,
			AP_FACTORS_TYPE_ITEM, bits);
	}
	if (bits & AP_FACTORS_BIT_PRICE) {
		price = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, price, factor,
			AP_FACTORS_TYPE_PRICE, bits);
	}
	if (bits & AP_FACTORS_BIT_DIRT) {
		dirt = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, dirt, factor,
			AP_FACTORS_TYPE_DIRT, bits);
	}
	au_packet_make_packet(&mod->packet, buffer,
		FALSE, &length, 0,
		NULL, /* Result */ 
		charstatus, /* CharStatus */
		chartype, /* CharType */
		charpoint, /* CharPoint */
		charpointmax, /* CharPointMax */
		charpointrr, /* CharPointRecoveryRate */
		NULL, /* Attribute */
		damage, /* Damage */
		defense, /* Defense */
		attack, /* Attack */
		item, /* Item */
		dirt, /* DIRT */
		price); /* Price */
	ap_packet_pop_temp_buffers(mod->ap_packet, count);
}

void ap_factors_make_result_packet(
	struct ap_factors_module * mod,
	void * buffer,
	const struct ap_factor * factor,
	uint64_t bits)
{
	void * result = ap_packet_get_temp_buffer(mod->ap_packet);
	void * charstatus = NULL;
	void * chartype = NULL;
	void * charpoint = NULL;
	void * charpointmax = NULL;
	void * charpointrr = NULL;
	void * damage = NULL;
	void * defense = NULL;
	void * attack = NULL;
	void * item = NULL;
	void * price = NULL;
	void * dirt = NULL;
	uint32_t count = 0;
	uint16_t length;
	if (bits & AP_FACTORS_BIT_CHAR_STATUS) {
		charstatus = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, charstatus, factor,
			AP_FACTORS_TYPE_CHAR_STATUS, bits);
	}
	if (bits & AP_FACTORS_BIT_CHAR_TYPE) {
		chartype = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, chartype, factor,
			AP_FACTORS_TYPE_CHAR_TYPE, bits);
	}
	if (bits & AP_FACTORS_BIT_CHAR_POINT) {
		charpoint = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, charpoint, factor,
			AP_FACTORS_TYPE_CHAR_POINT, bits);
	}
	if (bits & AP_FACTORS_BIT_CHAR_POINT_MAX) {
		charpointmax = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, charpointmax, factor,
			AP_FACTORS_TYPE_CHAR_POINT_MAX, bits);
	}
	if (bits & AP_FACTORS_BIT_DAMAGE) {
		damage = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, damage, factor,
			AP_FACTORS_TYPE_DAMAGE, bits);
	}
	if (bits & AP_FACTORS_BIT_DEFENSE) {
		defense = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, defense, factor,
			AP_FACTORS_TYPE_DEFENSE, bits);
	}
	if (bits & AP_FACTORS_BIT_ATTACK) {
		attack = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, attack, factor,
			AP_FACTORS_TYPE_ATTACK, bits);
	}
	if (bits & AP_FACTORS_BIT_ITEM) {
		item = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, item, factor,
			AP_FACTORS_TYPE_ITEM, bits);
	}
	if (bits & AP_FACTORS_BIT_PRICE) {
		price = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, price, factor,
			AP_FACTORS_TYPE_PRICE, bits);
	}
	if (bits & AP_FACTORS_BIT_DIRT) {
		dirt = ap_packet_get_temp_buffer(mod->ap_packet);
		count++;
		make_packet(mod, dirt, factor,
			AP_FACTORS_TYPE_DIRT, bits);
	}
	au_packet_make_packet(&mod->packet, result,
		FALSE, NULL, 0,
		NULL, /* Result */ 
		charstatus, /* CharStatus */
		chartype, /* CharType */
		charpoint, /* CharPoint */
		charpointmax, /* CharPointMax */
		charpointrr, /* CharPointRecoveryRate */
		NULL, /* Attribute */
		damage, /* Damage */
		defense, /* Defense */
		attack, /* Attack */
		item, /* Item */
		dirt, /* DIRT */
		price); /* Price */
	au_packet_make_packet(&mod->packet, buffer,
		FALSE, &length, 0,
		result, /* Result */ 
		NULL, /* CharStatus */
		NULL, /* CharType */
		NULL, /* CharPoint */
		NULL, /* CharPointMax */
		NULL, /* CharPointRecoveryRate */
		NULL, /* Attribute */
		NULL, /* Damage */
		NULL, /* Defense */
		NULL, /* Attack */
		NULL, /* Item */
		NULL, /* DIRT */
		NULL); /* Price */
	ap_packet_pop_temp_buffers(mod->ap_packet, count);
}

void ap_factors_make_char_view_packet(
	struct ap_factors_module * mod,
	void * buffer,
	const struct ap_factor * factor)
{
	void * result = ap_packet_get_temp_buffer(mod->ap_packet);
	void * charstatus = ap_packet_get_temp_buffer(mod->ap_packet);
	void * chartype = ap_packet_get_temp_buffer(mod->ap_packet);
	void * charpoint = ap_packet_get_temp_buffer(mod->ap_packet);
	void * charpointmax = ap_packet_get_temp_buffer(mod->ap_packet);
	void * attack = ap_packet_get_temp_buffer(mod->ap_packet);
	uint16_t length;
	au_packet_make_packet(
		&mod->factor_packets[AP_FACTORS_TYPE_CHAR_STATUS],
		charstatus, FALSE, NULL, 0,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		&factor->char_status.level,
		&factor->char_status.movement,
		&factor->char_status.movement_fast,
		NULL,
		&factor->char_status.murderer,
		NULL);
	make_packet(mod, chartype, factor, 
		AP_FACTORS_TYPE_CHAR_TYPE, -1);
	make_packet(mod, charpoint, factor, 
		AP_FACTORS_TYPE_CHAR_POINT, -1);
	make_packet(mod, charpointmax, factor, 
		AP_FACTORS_TYPE_CHAR_POINT_MAX, -1);
	make_packet(mod, attack, factor, 
		AP_FACTORS_TYPE_ATTACK, -1);
	au_packet_make_packet(&mod->packet, result,
		FALSE, NULL, 0,
		NULL, /* Result */ 
		charstatus, /* CharStatus */
		chartype, /* CharType */
		charpoint, /* CharPoint */
		charpointmax, /* CharPointMax */
		NULL, /* CharPointRecoveryRate */
		NULL, /* Attribute */
		NULL, /* Damage */
		NULL, /* Defense */
		attack, /* Attack */
		NULL, /* Item */
		NULL, /* DIRT */
		NULL); /* Price */
	au_packet_make_packet(&mod->packet, buffer,
		FALSE, &length, 0,
		result, /* Result */ 
		NULL, /* CharStatus */
		NULL, /* CharType */
		NULL, /* CharPoint */
		NULL, /* CharPointMax */
		NULL, /* CharPointRecoveryRate */
		NULL, /* Attribute */
		NULL, /* Damage */
		NULL, /* Defense */
		NULL, /* Attack */
		NULL, /* Item */
		NULL, /* DIRT */
		NULL); /* Price */
	ap_packet_pop_temp_buffers(mod->ap_packet, 6);
}

void ap_factors_make_damage_packet(
	struct ap_factors_module * mod,
	void * buffer, 
	const struct ap_factors_char_point * factor)
{
	au_packet_make_packet(
		&mod->factor_packets[AP_FACTORS_TYPE_CHAR_POINT], 
		buffer, FALSE, NULL, 0,
		&factor->hp, /* HP */
		&factor->mp, /* MP */
		NULL, /* SP */
		NULL, /* EXPHIGH */
		NULL, /* EXPLOW */
		NULL, /* Attack Point */
		NULL, /* Magic Attack Point */
		NULL, /* Magic Intensity */
		NULL, /* Agro Point */
		&factor->dmg_normal, /* Dmg Normal */
		&factor->dmg_attr_magic, /* Dmg Attr Magic */
		&factor->dmg_attr_water, /* Dmg Attr Water */
		&factor->dmg_attr_fire, /* Dmg Attr Fire */
		&factor->dmg_attr_earth, /* Dmg Attr Earth */
		&factor->dmg_attr_air, /* Dmg Attr Air */
		&factor->dmg_attr_poison, /* Dmg Attr Poison */
		&factor->dmg_attr_lightning, /* Dmg Attr Lightening */
		&factor->dmg_attr_ice, /* Dmg Attr Ice */
		NULL, /* Bonus Exp */
		&factor->dmg_heroic); /* Dmg Attr Heroic  */
}

void ap_factors_make_result_damage_packet(
	struct ap_factors_module * mod,
	void * buffer, 
	const struct ap_factors_char_point * factor)
{
	void * result = ap_packet_get_temp_buffer(mod->ap_packet);
	void * charpoint = ap_packet_get_temp_buffer(mod->ap_packet);
	uint16_t length;
	au_packet_make_packet(&mod->factor_packets[AP_FACTORS_TYPE_CHAR_POINT], 
		charpoint, FALSE, NULL, 0,
		&factor->hp, /* HP */
		&factor->mp, /* MP */
		NULL, /* SP */
		NULL, /* EXPHIGH */
		NULL, /* EXPLOW */
		NULL, /* Attack Point */
		NULL, /* Magic Attack Point */
		NULL, /* Magic Intensity */
		NULL, /* Agro Point */
		&factor->dmg_normal, /* Dmg Normal */
		&factor->dmg_attr_magic, /* Dmg Attr Magic */
		&factor->dmg_attr_water, /* Dmg Attr Water */
		&factor->dmg_attr_fire, /* Dmg Attr Fire */
		&factor->dmg_attr_earth, /* Dmg Attr Earth */
		&factor->dmg_attr_air, /* Dmg Attr Air */
		&factor->dmg_attr_poison, /* Dmg Attr Poison */
		&factor->dmg_attr_lightning, /* Dmg Attr Lightening */
		&factor->dmg_attr_ice, /* Dmg Attr Ice */
		NULL, /* Bonus Exp */
		&factor->dmg_heroic); /* Dmg Attr Heroic  */
	au_packet_make_packet(&mod->packet, 
		result, FALSE, NULL, 0,
		NULL, /* Result */ 
		NULL, /* CharStatus */
		NULL, /* CharType */
		charpoint, /* CharPoint */
		NULL, /* CharPointMax */
		NULL, /* CharPointRecoveryRate */
		NULL, /* Attribute */
		NULL, /* Damage */
		NULL, /* Defense */
		NULL, /* Attack */
		NULL, /* Item */
		NULL, /* DIRT */
		NULL); /* Price */
	au_packet_make_packet(&mod->packet, 
		buffer, FALSE, &length, 0,
		result, /* Result */ 
		NULL, /* CharStatus */
		NULL, /* CharType */
		NULL, /* CharPoint */
		NULL, /* CharPointMax */
		NULL, /* CharPointRecoveryRate */
		NULL, /* Attribute */
		NULL, /* Damage */
		NULL, /* Defense */
		NULL, /* Attack */
		NULL, /* Item */
		NULL, /* DIRT */
		NULL); /* Price */
	ap_packet_pop_temp_buffers(mod->ap_packet, 2);
}

void ap_factors_make_skill_result_packet(
	struct ap_factors_module * mod,
	void * buffer, 
	const struct ap_factor * factor)
{
	void * dirt = ap_packet_get_temp_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->factor_packets[AP_FACTORS_TYPE_DIRT], 
		dirt, FALSE, NULL, 0,
		NULL, /* duration */
		NULL, /* intensity */
		NULL, /* range */
		NULL, /* target */
		&factor->dirt.skill_level, /* skill level */
		&factor->dirt.skill_point, /* skill point */
		&factor->dirt.skill_exp, /* skill exp */
		NULL); /* heroic point */
	au_packet_make_packet(&mod->packet, 
		buffer, FALSE, NULL, 0,
		NULL, /* Result */ 
		NULL, /* CharStatus */
		NULL, /* CharType */
		NULL, /* CharPoint */
		NULL, /* CharPointMax */
		NULL, /* CharPointRecoveryRate */
		NULL, /* Attribute */
		NULL, /* Damage */
		NULL, /* Defense */
		NULL, /* Attack */
		NULL, /* Item */
		dirt, /* DIRT */
		NULL); /* Price */
	ap_packet_pop_temp_buffers(mod->ap_packet, 1);
}
