#include "server/as_skill_process.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/vector.h"

#include "public/ap_ai2.h"
#include "public/ap_chat.h"
#include "public/ap_guild.h"
#include "public/ap_module.h"
#include "public/ap_party.h"
#include "public/ap_pvp.h"
#include "public/ap_random.h"
#include "public/ap_skill.h"
#include "public/ap_tick.h"

#include "server/as_item.h"
#include "server/as_map.h"
#include "server/as_party.h"
#include "server/as_player.h"
#include "server/as_skill.h"

#include <assert.h>

struct as_skill_process_module {
	struct ap_module_instance instance;
	struct ap_ai2_module * ap_ai2;
	struct ap_character_module * ap_character;
	struct ap_chat_module * ap_chat;
	struct ap_guild_module * ap_guild;
	struct ap_item_module * ap_item;
	struct ap_party_module * ap_party;
	struct ap_pvp_module * ap_pvp;
	struct ap_random_module * ap_random;
	struct ap_skill_module * ap_skill;
	struct ap_tick_module * ap_tick;
	struct as_item_module * as_item;
	struct as_map_module * as_map;
	struct as_party_module * as_party;
	struct as_player_module * as_player;
	struct as_skill_module * as_skill;
	struct ap_character ** list;
};

static void addtarget(struct ap_skill_cast_info * cast, uint32_t id)
{
	if (cast->target_count < AP_SKILL_MAX_TARGET) {
		uint32_t i;
		for (i = 0; i < cast->target_count; i++) {
			if (cast->targets[i] == id)
				return;
		}
		cast->targets[cast->target_count++] = id;
	}
}

static void settargets(
	struct as_skill_process_module * mod,
	struct ap_character * character,
	struct ap_character * target,
	struct as_skill_character * attachment,
	struct ap_skill * skill,
	uint64_t tick)
{
	struct ap_skill_cast_info * cast = &attachment->cast;
	uint64_t range = skill->temp->range_type;
	uint64_t range2 = skill->temp->range_type2;
	const float * constfactor = ap_skill_get_const_factor(mod->ap_skill, 
		character, skill);
	if (!(range2 & AP_SKILL_RANGE2_GROUND)) {
		if (!ap_skill_is_valid_target(mod->ap_skill, character, target, skill, FALSE))
			return;
		cast->target_pos = target->pos;
	}
	vec_clear(mod->list);
	if ((range & AP_SKILL_RANGE_SPHERE) || (range2 & AP_SKILL_RANGE2_SPHERE)) {
		/* \todo Add increased skill range to radius. */
		if ((range & AP_SKILL_RANGE_BASE_TARGET) || (range2 & AP_SKILL_RANGE2_GROUND)) {
			as_map_get_characters_in_radius(mod->as_map, character, 
				&cast->target_pos, constfactor[AP_SKILL_CONST_TARGET_AREA_R],
				&mod->list);
		}
		else if (range & AP_SKILL_RANGE_BASE_SELF) {
			as_map_get_characters_in_radius(mod->as_map, character, 
				&character->pos, constfactor[AP_SKILL_CONST_TARGET_AREA_R],
				&mod->list);
		}
	}
	else if (range & AP_SKILL_RANGE_BOX) {
		vec2 dir = { cast->target_pos.x - character->pos.x, 
			cast->target_pos.z - character->pos.z };
		glm_vec2_normalize(dir);
		struct au_pos pos = { 
			character->pos.x - 10.0f * dir[0], 
			character->pos.y, 
			character->pos.z - 10.0f * dir[1] };
		float width = constfactor[AP_SKILL_CONST_TARGET_AREA_F2];
		float length = constfactor[AP_SKILL_CONST_TARGET_AREA_F1];
		if (width == 0.0f)
			width = 300.0f;
		if (length == 0.0f)
			length = 600.0f;
		as_map_get_characters_in_line(mod->as_map, character, &pos, 
			&cast->target_pos, width, length, &mod->list);
	}
	if (range & AP_SKILL_RANGE_TARGET_ONLY) {
		vec_push_back((void **)&mod->list, &target);
	}
	else if (range & AP_SKILL_RANGE_SELF_ONLY) {
		vec_push_back((void **)&mod->list, &character);
	}
	else if (range & AP_SKILL_RANGE_SUMMONS_ONLY) {
		/* \todo Summons.
		for (uint32_t i = 0; i < c->num_summons; i++) {
			list->push_back(c->summons[i]);
		}*/
	}
	if (range & AP_SKILL_RANGE_SPHERE_ONLY_PARTY_MEMBER) {
		struct ap_party * party = 
			ap_party_get_character_attachment(mod->ap_party, character)->party;
		if (party) {
			uint32_t i;
			uint32_t count = vec_count(mod->list);
			for (i = 0; i < count;) {
				struct ap_character * c = mod->list[i];
				struct ap_party * cparty = 
					ap_party_get_character_attachment(mod->ap_party, c)->party;
				if (character != c && cparty != party) {
					i = vec_erase_iterator(mod->list, i);
					count = vec_count(mod->list);
				}
				else {
					i++;
				}
			}
		}
		else {
			vec_clear(mod->list);
			vec_push_back((void **)&mod->list, &character);
		}
	}
	else if (range & AP_SKILL_RANGE_SPHERE_ONLY_SUMMONS) {
		/* \todo Filter summons. */
		vec_clear(mod->list);
	}
	else if (range & AP_SKILL_RANGE_TARGET_ONLY_INVOLVE_SELF) {
		vec_clear(mod->list);
		vec_push_back((void **)&mod->list, &character);
		if (character != target)
			vec_push_back((void **)&mod->list, &target);
	}
	else if (range & AP_SKILL_RANGE_TARGET_ONLY_FRIENDLY_UNITS) {
		/* \todo Filter friendly units. */
		vec_clear(mod->list);
	}
	else if (range2 & AP_SKILL_RANGE2_ONLY_GUILD_MEMBERS) {
		struct ap_guild * guild = 
			ap_guild_get_character(mod->ap_guild, character)->guild;
		if (guild) {
			uint32_t i;
			uint32_t count = vec_count(mod->list);
			for (i = 0; i < count;) {
				struct ap_character * c = mod->list[i];
				struct ap_guild_character * cguild = 
					ap_guild_get_character(mod->ap_guild, c);
				/* \todo 
				 * Check:
				 * - If target is in battleground.
				 * - If target is in an enemy in battle (arena). */
				if (c != character && 
					(guild != cguild->guild || 
						cguild->member->rank == AP_GUILD_MEMBER_RANK_JOIN_REQUEST)) {
					i = vec_erase_iterator(mod->list, i);
					count = vec_count(mod->list);
				}
				else {
					i++;
				}
			}
		}
		else {
			vec_clear(mod->list);
			vec_push_back((void **)&mod->list, &character);
		}
	}
	else if (range2 & AP_SKILL_RANGE2_TARGET_DEAD) {
		uint32_t i;
		uint32_t count = vec_count(mod->list);
		for (i = 0; i < count;) {
			struct ap_character * c = mod->list[i];
			if (c->action_status != AP_CHARACTER_ACTION_STATUS_DEAD) {
				i = vec_erase_iterator(mod->list, i);
				count = vec_count(mod->list);
			}
			else {
				i++;
			}
		}
	}
	{
		uint32_t i;
		uint32_t count = vec_count(mod->list);
		for (i = 0; i < count; i++) {
			struct ap_character * c = mod->list[i];
			if (ap_skill_is_valid_target(mod->ap_skill, character, c, skill, FALSE))
				addtarget(cast, c->id);
		}
	}
}

static inline void begincast(
	struct as_skill_process_module * mod,
	struct ap_character * c,
	struct ap_character * t,
	struct as_skill_character * sc,
	struct ap_skill * skill,
	uint64_t tick)
{
	struct ap_skill_cast_info * cast = &sc->cast;
	float cr = (float)MIN(c->factor.attack.skill_cast, 100);
	float cdr = (float)MIN(c->factor.attack.skill_cooldown, 100);
	const float * f = ap_skill_get_const_factor(mod->ap_skill, c, skill);
	memcpy(cast, &sc->next_cast, sizeof(*cast));
	settargets(mod, c, t, sc, skill, tick);
	cast->cast_time = (uint32_t)(f[AP_SKILL_CONST_CAST_TIME] * 
		(100.0f - cr) / 100.0f);
	cast->cooldown_time = 
		(uint32_t)(f[AP_SKILL_CONST_RECAST_TIME] * 
			(100.0f - cdr) / 100.0f);
	cast->duration_time = (uint32_t)f[AP_SKILL_CONST_DURATION];
	cast->complete_tick = tick + cast->cast_time;
	c->action_end_tick = cast->complete_tick;
	skill->cooldown_tick = tick + cast->cooldown_time;
	c->factor.char_point.hp -= (int32_t)f[AP_SKILL_CONST_COST_HP];
	c->factor.char_point.mp -= (int32_t)f[AP_SKILL_CONST_COST_MP];
	ap_skill_make_cast_packet(mod->ap_skill, skill, c, t, cast);
	as_map_broadcast(mod->as_map, c);
	ap_character_update(mod->ap_character, c, 
		AP_FACTORS_BIT_HP | AP_FACTORS_BIT_MP, FALSE);
	if (ap_skill_is_offensive(skill)) {
		ap_character_special_status_off(mod->ap_character, c, 
			AP_CHARACTER_SPECIAL_STATUS_INVINCIBLE);
	}
}

static uint32_t calcphysicalattack(
	struct as_skill_process_module *  mod,
	struct ap_character * character,
	struct ap_character * target,
	struct ap_skill_action_data * action)
{
	if (ap_character_is_pc(character)) {
		if (character->factor.char_type.cs == AU_CHARCLASS_TYPE_MAGE) {
			float x = character->factor.char_status.intel * 0.4f;
			float physicalmin = character->factor.damage.min.physical + x * 0.95f;
			float physicalmax = character->factor.damage.max.physical + x * 1.05f;
			int physical;
			if (character->adjust_combat_point & AP_CHARACTER_ADJUST_COMBAT_POINT_MAX_DAMAGE)
				physical = (int)physicalmax;
			else if (character->adjust_combat_point & AP_CHARACTER_ADJUST_COMBAT_POINT_MIN_DAMAGE)
				physical = (int)physicalmin;
			else
				physical = (int)ap_random_float(mod->ap_random, physicalmin, physicalmax);
			action->info.target.dmg_normal = (physical > 1) ? -physical : -1;
			return -action->info.target.dmg_normal;
		}
		else {
			uint32_t x = ap_character_calc_physical_attack(mod->ap_character,
				character, target, &action->info);
			if ((float)x > target->factor.defense.point.physical)
				x = (uint32_t)((float)x - target->factor.defense.point.physical);
			else
				x = 1;
			action->info.target.dmg_normal = -(int)x;
			return x;
		}
	}
	else {
		return ap_character_calc_physical_attack(mod->ap_character,
			character, target, &action->info);
	}
}

static uint32_t calcskillattack(
	struct as_skill_process_module *  mod,
	struct ap_skill * skill,
	struct ap_character * character,
	struct ap_character * target,
	struct ap_skill_action_data * action)
{
	uint32_t level = ap_character_get_level(character);
	uint32_t targetlevel = ap_character_get_level(target);
	int penalty = (targetlevel > level) ? (targetlevel - level) : 0;
	float resist = (100.0f - MIN(target->factor.defense.rate.physical, 80.0f)) / 100.0f;
	uint32_t x = calcphysicalattack(mod, character, target, action);
	const float * constfactor = ap_skill_get_const_factor(mod->ap_skill, 
		character, skill);
	if (ap_character_is_pc(character) && 
		(target->char_type & AP_CHARACTER_TYPE_MONSTER)) {
		penalty *= 5;
	}
	if (penalty > 80)
		penalty = 80;
	x = (uint32_t)((((float)x * (100.0f + constfactor[AP_SKILL_CONST_DAMAGE_A_PERCENT]) / 100.0f) + 
		constfactor[AP_SKILL_CONST_DAMAGE_A]) * ((100 - penalty) / 100.0f));
	if ((float)x <= target->factor.defense.point.physical)
		x = 1;
	else
		x = (uint32_t)((float)(x - target->factor.defense.point.physical) * resist);
	action->info.target.dmg_normal = -(int)x;
	return x;
}

static uint32_t calcspiritdamage(
	struct as_skill_process_module *  mod,
	struct ap_skill * skill,
	struct ap_character * character,
	struct ap_character * target,
	struct ap_skill_action_data * action,
	enum ap_factors_attribute_type attribute)
{
	uint32_t level = ap_character_get_level(character);
	uint32_t targetlevel = ap_character_get_level(target);
	int penalty = (targetlevel > level) ? (targetlevel - level) : 0;
	const float * constfactor = ap_skill_get_const_factor(mod->ap_skill, 
		character, skill);
	float resistance;
	float dmgmin;
	float dmgmax;
	float damage;
	float damagepercent;
	int * damagetarget;
	float x;
	switch (attribute) {
	case AP_FACTORS_ATTRIBUTE_TYPE_MAGIC:
		resistance = target->factor.defense.point.magic;
		dmgmin = character->factor.damage.min.magic;
		dmgmax = character->factor.damage.max.magic;
		damage = constfactor[AP_SKILL_CONST_DAMAGE_MAGIC];
		damagepercent = constfactor[AP_SKILL_CONST_DAMAGE_MAGIC_PERCENT];
		damagetarget = &action->info.target.dmg_attr_magic;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_WATER:
		resistance = target->factor.defense.point.water;
		dmgmin = character->factor.damage.min.water;
		dmgmax = character->factor.damage.max.water;
		damage = constfactor[AP_SKILL_CONST_DAMAGE_WATER];
		damagepercent = constfactor[AP_SKILL_CONST_DAMAGE_WATER_PERCENT];
		damagetarget = &action->info.target.dmg_attr_water;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_FIRE:
		resistance = target->factor.defense.point.fire;
		dmgmin = character->factor.damage.min.fire;
		dmgmax = character->factor.damage.max.fire;
		damage = constfactor[AP_SKILL_CONST_DAMAGE_FIRE];
		damagepercent = constfactor[AP_SKILL_CONST_DAMAGE_FIRE_PERCENT];
		damagetarget = &action->info.target.dmg_attr_fire;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_EARTH:
		resistance = target->factor.defense.point.earth;
		dmgmin = character->factor.damage.min.earth;
		dmgmax = character->factor.damage.max.earth;
		damage = constfactor[AP_SKILL_CONST_DAMAGE_EARTH];
		damagepercent = constfactor[AP_SKILL_CONST_DAMAGE_EARTH_PERCENT];
		damagetarget = &action->info.target.dmg_attr_earth;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_AIR:
		resistance = target->factor.defense.point.air;
		dmgmin = character->factor.damage.min.air;
		dmgmax = character->factor.damage.max.air;
		damage = constfactor[AP_SKILL_CONST_DAMAGE_AIR];
		damagepercent = constfactor[AP_SKILL_CONST_DAMAGE_AIR_PERCENT];
		damagetarget = &action->info.target.dmg_attr_air;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_POISON:
		resistance = target->factor.defense.point.poison;
		dmgmin = character->factor.damage.min.poison;
		dmgmax = character->factor.damage.max.poison;
		damage = constfactor[AP_SKILL_CONST_DAMAGE_POISON];
		damagepercent = constfactor[AP_SKILL_CONST_DAMAGE_POISON_PERCENT];
		damagetarget = &action->info.target.dmg_attr_poison;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_LIGHTENING:
		resistance = target->factor.defense.point.lightning;
		dmgmin = character->factor.damage.min.lightning;
		dmgmax = character->factor.damage.max.lightning;
		damage = constfactor[AP_SKILL_CONST_DAMAGE_THUNDER];
		damagepercent = constfactor[AP_SKILL_CONST_DAMAGE_THUNDER_PERCENT];
		damagetarget = &action->info.target.dmg_attr_lightning;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_ICE:
		resistance = target->factor.defense.point.ice;
		dmgmin = character->factor.damage.min.ice;
		dmgmax = character->factor.damage.max.ice;
		damage = constfactor[AP_SKILL_CONST_DAMAGE_ICE];
		damagepercent = constfactor[AP_SKILL_CONST_DAMAGE_ICE_PERCENT];
		damagetarget = &action->info.target.dmg_attr_ice;
		break;
	default:
		return 0;
	}
	if (resistance > 80.0f && ap_character_is_pc(target))
		resistance = 0.0f;
	x = ap_random_float(mod->ap_random, dmgmin, dmgmax);
	if (ap_character_is_pc(character) && 
		(target->char_type & AP_CHARACTER_TYPE_MONSTER)) {
		penalty *= 5;
	}
	if (penalty > 80)
		penalty = 80;
	x = (((float)x * (100.0f + damagepercent) / 100.0f) + damage) * 
		((100.0f - (float)penalty) / 100.0f);
	x = x * 2.0f * (100.0f - resistance) / 100.0f;
	if (x < 1.0f)
		x = 1.0f;
	*damagetarget = -(int)x;
	return (uint32_t)x;
}

static uint32_t calcheroicdamage(
	struct as_skill_process_module *  mod,
	struct ap_skill * skill,
	struct ap_character * character,
	struct ap_character * target,
	struct ap_skill_action_data * action)
{
	uint32_t level = ap_character_get_level(character);
	uint32_t targetlevel = ap_character_get_level(target);
	int penalty = (targetlevel > level) ? (targetlevel - level) : 0;
	uint32_t x = ap_character_calc_heroic_attack(mod->ap_character, character, 
		target, &action->info);
	const float * constfactor = ap_skill_get_const_factor(mod->ap_skill, 
		character, skill);
	if (penalty > 80)
		penalty = 80;
	x = (uint32_t)((((float)x * (100.0f + 
		constfactor[AP_SKILL_CONST_DAMAGE_HEROIC_PERCENT]) / 100.0f) + 
		constfactor[AP_SKILL_CONST_DAMAGE_HEROIC]) * ((100 - penalty) / 100.0f));
	action->info.target.dmg_heroic = -(int)x;
	return x;
}

static inline void completecast(
	struct as_skill_process_module * mod,
	struct ap_character * c,
	struct as_skill_character * sc,
	struct ap_skill * skill,
	uint64_t tick)
{
	uint32_t modifiedlevel = ap_skill_get_modified_level(mod->ap_skill, c, skill);
	const float * constfactor = ap_skill_get_const_factor(mod->ap_skill, c, skill);
	struct ap_skill_cast_info * cast = &sc->cast;
	uint32_t i;
	boolean isoffensive = ap_skill_is_offensive(skill);
	boolean isbuff = ap_skill_is_buff(skill->temp);
	boolean instantdamage = ap_skill_inflicts_instant_damage(skill);
	struct ap_item * lensstone;
	struct ap_skill_action_data action = { 0 };
	action.skill_id = skill->id;
	action.skill_tid = skill->tid;
	action.skilll_level = ap_skill_get_level(skill);
	lensstone = ap_item_find_item_in_use_by_cash_item_type(mod->ap_item,
		c, AP_ITEM_CASH_ITEM_TYPE_ONE_ATTACK, 1, AP_ITEM_STATUS_CASH_INVENTORY);
	if (lensstone) {
		action.additional_effect |= AP_CHARACTER_ADDITIONAL_EFFECT_LENS_STONE;
		as_item_consume(mod->as_item, c, lensstone, 1);
	}
	for (i = 0; i < cast->target_count; i++) {
		struct ap_character * target = as_map_get_character(mod->as_map,
			cast->targets[i]);
		struct ap_skill_character * targetattachment;
		if (!target || !ap_skill_is_valid_target(mod->ap_skill, c, target, skill, FALSE))
			continue;
		targetattachment = ap_skill_get_character(mod->ap_skill, target);
		action.action_type = AP_SKILL_ACTION_CAST_SKILL_RESULT;
		action.target_base[0] = target;
		memset(&action.info.target, 0, sizeof(action.info.target));
		action.info.target.hp = target->factor.char_point.hp;
		action.info.target.mp = target->factor.char_point.mp;
		if (isoffensive) {
			uint32_t skillblock = (uint32_t)MIN(target->factor.defense.rate.physical_block, 80.0f);
			uint32_t j;
			if (isbuff) {
				as_skill_clear_similar_buffs(mod->as_skill, target, skill->temp, 
					ap_skill_get_level(skill));
			}
			/* Extend combat duration. */
			c->combat_end_tick = tick + 10000;
			target->combat_end_tick = c->combat_end_tick;
			ap_pvp_extend_duration(mod->ap_pvp, c, target, 300000);
			if ((int)constfactor[AP_SKILL_CONST_SKILL_TYPE2] == 1 &&
				ap_random_get(mod->ap_random, 100) < skillblock) {
				action.action_type = AP_SKILL_ACTION_BLOCK_CAST_SKILL;
			}
			else {
				for (j = 0; j < targetattachment->buff_count; j++) {
					const struct ap_skill_buff_list * buff = &targetattachment->buff_list[j];
					if (!ap_skill_check_buff_condition(mod->ap_skill, buff))
						continue;
					if (instantdamage && 
						(buff->temp->special_status & AP_SKILL_SPECIAL_STATUS_SKILL_ATK_INVINCIBLE)) {
						action.action_type = AP_SKILL_ACTION_BLOCK_CAST_SKILL;
						break;
					}
				}
			}
			if (action.action_type == AP_SKILL_ACTION_BLOCK_CAST_SKILL) {
				ap_skill_make_cast_result_packet(mod->ap_skill, c, &action);
				as_player_send_packet(mod->as_player, c);
				as_player_send_packet(mod->as_player, target);
				continue;
			}
			if (!instantdamage)
				ap_ai2_add_aggro_damage(mod->ap_ai2, target, c->id, 0);
		}
		ap_character_special_status_off(mod->ap_character, target, 
			AP_CHARACTER_SPECIAL_STATUS_SLEEP);
		if (isbuff) {
			/* \todo If caster is ArchLord, its TID needs to be used instead. */
			struct ap_skill_buff_list * buff = as_skill_add_buff(mod->as_skill,
				target, skill->id, modifiedlevel, skill->temp,
				cast->duration_time, c->id, c->tid);
			if (buff) {
				/* If skill does not inflict damage and lens stone is on,
				 * broadcast additional effect. */;
				 if (lensstone && !instantdamage) {
					 ap_skill_make_additional_effect_packet(mod->ap_skill, skill,
						 c, target->id, AP_SKILL_ADDITIONAL_EFFECT_LENS_STONE);
					as_map_broadcast(mod->as_map, c);
				 }
			}
			else {
				action.action_type = AP_SKILL_ACTION_MISS_CAST_SKILL;
				ap_skill_make_cast_result_packet(mod->ap_skill, c, &action);
				as_map_broadcast(mod->as_map, c);
			}
		}
		if (instantdamage) {
			uint32_t total = 0;
			uint32_t j;
			const enum ap_factors_attribute_type spirit[] = {
				AP_FACTORS_ATTRIBUTE_TYPE_MAGIC,
				AP_FACTORS_ATTRIBUTE_TYPE_WATER,
				AP_FACTORS_ATTRIBUTE_TYPE_FIRE,
				AP_FACTORS_ATTRIBUTE_TYPE_EARTH,
				AP_FACTORS_ATTRIBUTE_TYPE_AIR,
				AP_FACTORS_ATTRIBUTE_TYPE_POISON,
				AP_FACTORS_ATTRIBUTE_TYPE_LIGHTENING,
				AP_FACTORS_ATTRIBUTE_TYPE_ICE };
			boolean killed = FALSE;
			action.info.defense_penalty = ap_character_calc_defense_penalty(mod->ap_character,
				c, target);
			total = calcskillattack(mod, skill, c, target, &action);
			total += calcheroicdamage(mod, skill, c, target, &action);
			for (j = 0; j < COUNT_OF(spirit); j++)
				total += calcspiritdamage(mod, skill, c, target, &action, spirit[j]);
			ap_ai2_add_aggro_damage(mod->ap_ai2, target, c->id, total);
			if (total >= target->factor.char_point.hp) {
				killed = TRUE;
				target->factor.char_point.hp = 0;
				action.info.target.hp = 0;
			}
			else {
				target->factor.char_point.hp -= total;
				action.info.target.hp = target->factor.char_point.hp;
			}
			ap_skill_make_cast_result_packet(mod->ap_skill, c, &action);
			as_map_broadcast(mod->as_map, target);
			if (killed) {
				ap_character_kill(mod->ap_character, target, c, 
					AP_CHARACTER_DEATH_BY_ATTACK);
			}
		}
	}
}

static inline float adjustrange(
	struct as_skill_process_module * mod,
	float range,
	const struct ap_skill_template * temp,
	struct ap_character * t)
{
	if (!CHECK_BIT(temp->range_type, AP_SKILL_RANGE_SPHERE))
		return range;
	if (CHECK_BIT(temp->range_type, AP_SKILL_RANGE_BASE_SELF) &&
		CHECK_BIT(temp->range_type2, AP_SKILL_RANGE2_SPHERE)) {
		return range;
	}
	return (range + t->factor.attack.hit_range);
}

static inline boolean preparecast(
	struct as_skill_process_module * mod,
	struct ap_character * c,
	struct as_skill_character * sc,
	struct ap_skill * skill,
	struct as_skill_cast_queue_entry * e)
{
	struct ap_skill_action_data action = { 0 };
	struct ap_skill_cast_info * cast;
	struct ap_character * t = NULL;
	if (!skill || !ap_skill_attempt_cast(mod->ap_skill, c, skill))
		return FALSE;
	cast = &sc->next_cast;
	cast->force_attack = e->forced_attack;
	cast->range = ap_skill_get_cast_range(mod->ap_skill, c, skill);
	if (!(skill->temp->range_type2 & AP_SKILL_RANGE2_GROUND)) {
		t = as_map_get_character(mod->as_map, e->target_id);
		if (!t || !ap_skill_is_valid_target(mod->ap_skill, c, t, skill, FALSE))
			return FALSE;
		cast->target_id = e->target_id;
		cast->follow_target = TRUE;
		cast->target_pos = t->pos;
		cast->range = adjustrange(mod, cast->range, skill->temp, t);
		if (au_distance2d(&c->pos, &t->pos) > cast->range)
			ap_character_follow(mod->ap_character, c, t, TRUE, (uint16_t)cast->range);
		else
			cast->within_range = TRUE;
	}
	else {
		cast->target_id = 0;
		cast->follow_target = FALSE;
		cast->target_pos = e->pos;
		if (au_distance2d(&c->pos, &e->pos) > cast->range)
			ap_character_set_movement(mod->ap_character, c, &e->pos, TRUE);
		else
			cast->within_range = TRUE;
	}
	cast->skill_id = e->skill_id;
	return TRUE;
}

static inline boolean handlecast(
	struct as_skill_process_module * mod,
	struct ap_character * c,
	struct as_skill_character * sc,
	struct ap_skill * skill,
	uint64_t tick)
{
	struct ap_skill_cast_info * cast = &sc->next_cast;
	struct ap_character * t = c;
	if (!ap_skill_attempt_cast(mod->ap_skill, c, skill))
		return TRUE;
	if (cast->follow_target) {
		t = as_map_get_character(mod->as_map, cast->target_id);
		if (!t || !ap_skill_is_valid_target(mod->ap_skill, c, t, skill, cast->force_attack))
			return TRUE;
		cast->target_pos = t->pos;
	}
	if (au_distance2d(&c->pos, &cast->target_pos) > cast->range) {
		return FALSE;
	}
	else {
		begincast(mod, c, t, sc, skill, tick);
		return TRUE;
	}
}

static boolean cbcharstopaction(
	struct as_skill_process_module * mod,
	struct ap_character_cb_stop_action * cb)
{
	struct ap_character * c = cb->character;
	struct as_skill_character * sc = as_skill_get_character(mod->as_skill, c);
	sc->cast_queue_count = 0;
	if (sc->next_cast.skill_id) {
		sc->next_cast.skill_id = 0;
		if (!sc->next_cast.within_range)
			ap_character_stop_movement(mod->ap_character, c);
	}
	return TRUE;
}

static void removeexpiredbuffs(
	struct as_skill_process_module * mod,
	struct ap_character * character,
	struct ap_skill_character * attachment,
	uint64_t tick)
{
	uint32_t i;
	for (i = 0; i < attachment->buff_count;) {
		struct ap_skill_buff_list * buff = &attachment->buff_list[i];
		if (buff->temp->attribute & AP_SKILL_ATTRIBUTE_PASSIVE) {
			i++;
			continue;
		}
		if (buff->temp->effect_type2 & AP_SKILL_EFFECT2_DURATION_TYPE) {
			if (ap_skill_has_detail(buff->temp, AP_SKILL_EFFECT_DETAIL_DURATION_TYPE,
					AP_SKILL_EFFECT_DETAIL_DURATION_TYPE2)) {
				/* Do not time-out item type buffs.
				 * They are managed by item process. */
				i++;
				continue;
			}
		}
		if (tick >= buff->end_tick)
			i = ap_skill_remove_buff(mod->ap_skill, character, i);
		else
			i++;
	}
}

static void syncpassive(
	struct as_skill_process_module * mod,
	struct ap_character * character,
	struct ap_skill_character * attachment,
	struct as_skill_character * srvattachment)
{
	const struct ap_skill_buffed_combat_effect_arg * buffedcombat = 
		&attachment->combat_effect_arg;
	if (srvattachment->sync_passive_buff_level_up == INT32_MAX) {
		uint32_t i;
		for (i = 0; i < attachment->skill_count; i++) {
			struct ap_skill * skill = attachment->skill[i];
			if (skill->temp->attribute & AP_SKILL_ATTRIBUTE_PASSIVE)
				as_skill_add_passive_buff(mod->as_skill, character, skill);
		}
	}
	else if (buffedcombat->skill_level_up_point != srvattachment->sync_passive_buff_level_up) {
		uint32_t i;
		for (i = 0; i < attachment->skill_count; i++) {
			struct ap_skill * skill = attachment->skill[i];
			uint32_t modifiedlevel;
			uint32_t j;
			boolean add = TRUE;
			if (!(skill->temp->attribute & AP_SKILL_ATTRIBUTE_PASSIVE))
				continue;
			modifiedlevel = ap_skill_get_modified_level(mod->ap_skill,
				character, skill);
			for (j = 0; j < attachment->buff_count; j++) {
				struct ap_skill_buff_list * buff = &attachment->buff_list[j];
				if (buff->skill_tid == skill->tid) {
					if (buff->skill_level != modifiedlevel)
						ap_skill_remove_buff(mod->ap_skill, character, j);
					else
						add = FALSE;
					break;
				}
			}
			if (add)
				as_skill_add_passive_buff(mod->as_skill, character, skill);
		}
	}
	else {
		return;
	}
	srvattachment->sync_passive_buff_level_up = buffedcombat->skill_level_up_point;
}

static boolean cbcharprocess(
	struct as_skill_process_module * mod,
	struct ap_character_cb_process * cb)
{
	struct ap_character * c = cb->character;
	struct as_skill_character * sc;
	struct ap_skill_character * attachment = ap_skill_get_character(mod->ap_skill, c);
	if (attachment->sync_combat_effect_arg) {
		ap_skill_make_update_attached_data_packet(mod->ap_skill, c);
		as_player_send_packet(mod->as_player, c);
		ap_skill_make_modified_skill_level_packet(mod->ap_skill, c);
		as_player_send_packet(mod->as_player, c);
		attachment->sync_combat_effect_arg = FALSE;
	}
	if (c->action_status == AP_CHARACTER_ACTION_STATUS_DEAD)
		return TRUE;
	if (cb->tick >= attachment->last_buff_process_tick + 1000) {
		removeexpiredbuffs(mod, c, attachment, cb->tick);
		attachment->last_buff_process_tick = cb->tick;
	}
	sc = as_skill_get_character(mod->as_skill, c);
	syncpassive(mod, c, attachment, sc);
	if (sc->cast.skill_id) {
		struct ap_skill * skill;
		if (cb->tick < sc->cast.complete_tick)
			return TRUE;
		skill = ap_skill_find(mod->ap_skill, c, sc->cast.skill_id);
		if (skill)
			completecast(mod, c, sc, skill, cb->tick);
		sc->cast.skill_id = 0;
	}
	if (cb->tick < c->action_end_tick)
		return TRUE;
	if (sc->next_cast.skill_id) {
		struct ap_skill * skill = 
			ap_skill_find(mod->ap_skill, c, sc->next_cast.skill_id);
		if (!skill || handlecast(mod, c, sc, skill, cb->tick)) {
			sc->next_cast.skill_id = 0;
			if (c->is_moving)
				ap_character_stop_movement(mod->ap_character, c);
		}
		return TRUE;
	}
	while (sc->cast_queue_count) {
		struct as_skill_cast_queue_entry * e = 
			&sc->cast_queue[sc->cast_queue_cursor++];
		struct ap_skill * skill = ap_skill_find(mod->ap_skill, c, e->skill_id);
		sc->cast_queue_count--;
		if (sc->cast_queue_cursor >= AS_SKILL_MAX_CAST_QUEUE_COUNT)
			sc->cast_queue_cursor = 0;
		if (preparecast(mod, c, sc, skill, e))
			break;
	}
	return TRUE;
}

static boolean cbcharattemptattack(
	struct as_skill_process_module * mod,
	struct ap_character_cb_attempt_attack * cb)
{
	struct ap_skill_character * attachment = ap_skill_get_character(mod->ap_skill, 
		cb->target);
	uint32_t i;
	for (i = 0; i < attachment->buff_count; i++) {
		struct ap_skill_buff_list * buff = &attachment->buff_list[i];
		if (ap_skill_check_buff_condition(mod->ap_skill, buff) &&
			CHECK_BIT(buff->temp->effect_type, AP_SKILL_EFFECT_REFLECT_DAMAGE_SHIELD)) {
			cb->action->result = AP_CHARACTER_ACTION_RESULT_TYPE_BLOCK;
			return FALSE;
		}
	}
	return TRUE;
}

static boolean cbchardeath(
	struct as_skill_process_module * mod,
	struct ap_character_cb_death * cb)
{
	uint32_t i;
	struct ap_skill_character * attachment = ap_skill_get_character(mod->ap_skill,
		cb->character);
	as_skill_reset_all_buffs(mod->as_skill, cb->character);
	/* Reset skill cooldowns upon death. */
	for (i = 0; i < attachment->skill_count; i++)
		attachment->skill[i]->cooldown_tick = 0;
	return TRUE;
}

static boolean cbreceive(
	struct as_skill_process_module * mod,
	struct ap_skill_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	struct ap_skill * skill = NULL;
	switch (cb->type) {
	case AP_SKILL_PACKET_TYPE_CAST_SKILL: {
		struct as_skill_cast_queue_entry * e;
		struct as_skill_character * sc;
		uint32_t index;
		if (!cb->action_packet || !cb->action_packet->target)
			break;
		skill = ap_skill_find(mod->ap_skill, c, cb->skill_id);
		if (!skill)
			break;
		if (!ap_skill_attempt_cast(mod->ap_skill, c, skill))
			break;
		sc = as_skill_get_character(mod->as_skill, c);
		if (sc->cast_queue_count >= AS_SKILL_MAX_CAST_QUEUE_COUNT)
			break;
		index = sc->cast_queue_cursor + sc->cast_queue_count++;
		if (index >= AS_SKILL_MAX_CAST_QUEUE_COUNT)
			index -= AS_SKILL_MAX_CAST_QUEUE_COUNT;
		assert(index < AS_SKILL_MAX_CAST_QUEUE_COUNT);
		e = &sc->cast_queue[index];
		e->skill_id = cb->skill_id;
		e->target_id = cb->action_packet->target->id;
		e->pos = cb->action_packet->dest_pos;
		e->forced_attack = cb->action_packet->forced_attack;
		break;
	}
	}
	return TRUE;
}

static void sendattachment(
	struct as_skill_process_module * mod,
	struct ap_character * character)
{
	ap_skill_make_update_attached_data_packet(mod->ap_skill, character);
	as_player_send_packet(mod->as_player, character);
}

static boolean cbskilladdbuff(
	struct as_skill_process_module * mod,
	struct ap_skill_cb_add_buff * data)
{
	struct ap_party * party;
	if (data->buff->temp->attribute & AP_SKILL_ATTRIBUTE_PASSIVE)
		return TRUE;
	party = ap_party_get_character_party(mod->ap_party, data->character);
	ap_skill_make_add_buffed_list_packet(mod->ap_skill, data->character, data->buff);
	as_map_broadcast(mod->as_map, data->character);
	if (party)
		as_party_send_packet_except(mod->as_party, party, data->character);
	sendattachment(mod, data->character);
	return TRUE;
}

static boolean cbskillremovebuff(
	struct as_skill_process_module * mod,
	struct ap_skill_cb_remove_buff * data)
{
	struct ap_party * party;
	if (data->buff->temp->attribute & AP_SKILL_ATTRIBUTE_PASSIVE)
		return TRUE;
	party = ap_party_get_character_party(mod->ap_party, data->character);
	ap_skill_make_remove_buffed_list_packet(mod->ap_skill, data->character, data->buff);
	as_map_broadcast(mod->as_map, data->character);
	if (party)
		as_party_send_packet_except(mod->as_party, party, data->character);
	sendattachment(mod, data->character);
	return TRUE;
}

static boolean onregister(
	struct as_skill_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_ai2, AP_AI2_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_chat, AP_CHAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_guild, AP_GUILD_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_party, AP_PARTY_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_pvp, AP_PVP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_random, AP_RANDOM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_skill, AP_SKILL_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_item, AS_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_party, AS_PARTY_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_skill, AS_SKILL_MODULE_NAME);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_STOP_ACTION, mod, cbcharstopaction);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_PROCESS, mod, cbcharprocess);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_ATTEMPT_ATTACK, mod, cbcharattemptattack);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_DEATH, mod, cbchardeath);
	ap_skill_add_callback(mod->ap_skill, AP_SKILL_CB_RECEIVE, mod, cbreceive);
	ap_skill_add_callback(mod->ap_skill, AP_SKILL_CB_ADD_BUFF, mod, cbskilladdbuff);
	ap_skill_add_callback(mod->ap_skill, AP_SKILL_CB_REMOVE_BUFF, mod, cbskillremovebuff);
	return TRUE;
}

static void onshutdown(struct as_skill_process_module * mod)
{
	vec_free(mod->list);
}

struct as_skill_process_module * as_skill_process_create_module()
{
	struct as_skill_process_module * mod = ap_module_instance_new(AS_SKILL_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	mod->list = vec_new_reserved(sizeof(*mod->list), 128);
	return mod;
}
