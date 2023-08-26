#include "server/as_character_process.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/vector.h"

#include "public/ap_ai2.h"
#include "public/ap_chat.h"
#include "public/ap_config.h"
#include "public/ap_event_teleport.h"
#include "public/ap_item.h"
#include "public/ap_module.h"
#include "public/ap_optimized_packet2.h"
#include "public/ap_pvp.h"
#include "public/ap_random.h"
#include "public/ap_skill.h"
#include "public/ap_tick.h"

#include "server/as_event_binding.h"
#include "server/as_item.h"
#include "server/as_map.h"
#include "server/as_player.h"
#include "server/as_skill.h"

#include <assert.h>

struct as_character_process_module {
	struct ap_module_instance instance;
	struct ap_ai2_module * ap_ai2;
	struct ap_character_module * ap_character;
	struct ap_chat_module * ap_chat;
	struct ap_config_module * ap_config;
	struct ap_event_teleport_module * ap_event_teleport;
	struct ap_item_module * ap_item;
	struct ap_optimized_packet2_module * ap_optimized_packet2;
	struct ap_pvp_module * ap_pvp;
	struct ap_random_module * ap_random;
	struct ap_skill_module * ap_skill;
	struct ap_tick_module * ap_tick;
	struct as_character_module * as_character;
	struct as_event_binding_module * as_event_binding;
	struct as_item_module * as_item;
	struct as_map_module * as_map;
	struct as_player_module * as_player;
	struct as_skill_module * as_skill;
	struct ap_character ** list;
	uint32_t * process_queue;
	uint32_t queue_cursor;
	int recent_processed_count_min;
	uint32_t recent_processed_count_min_queue;
	int recent_processed_count_max;
	uint32_t recent_processed_count_max_queue;
	uint64_t recent_processed_report_tick;
	float exp_rate;
};

static inline void stopmove(struct ap_character * c)
{
	c->is_moving = FALSE;
	c->is_moving_fast = FALSE;
}

static void processmoveinput(
	struct as_character_process_module * mod,
	struct ap_character * c, 
	struct as_character * sc)
{
	struct as_character_move_input * input = &sc->move_input;
	if (input->move_flags & AP_CHARACTER_MOVE_FLAG_STOP) {
		ap_character_stop_movement(mod->ap_character, c);
	}
	else if (!(input->move_flags & AP_CHARACTER_MOVE_FLAG_DIRECTION)) {
		uint8_t moveflags = 0;
		c->dst_pos = input->dst_pos;
		c->is_moving = TRUE;
		if (CHECK_BIT(input->move_flags,
				AP_CHARACTER_MOVE_FLAG_FAST)) {
			c->is_moving_fast = TRUE;
			moveflags |= AP_CHARACTER_MOVE_FLAG_FAST;
		}
		else {
			c->is_moving_fast = FALSE;
		}
		c->is_following = FALSE;
		/** \todo Turn (rotate) character. */
		ap_optimized_packet2_make_char_move_packet(mod->ap_optimized_packet2, c,
			&c->pos, &c->dst_pos, 
			moveflags, 0);
		as_map_broadcast(mod->as_map, c);
	}
	input->not_processed = FALSE;
}

static void processmove(
	struct as_character_process_module * mod,
	struct ap_character * c, 
	uint64_t tick,
	float dt)
{
	struct au_pos newpos = c->pos;
	float dtotal;
	float dx;
	float dz;
	float movtotal;
	float movspeed;
	boolean stop = FALSE;
	boolean ispc;
	if (c->action_status != AP_CHARACTER_ACTION_STATUS_NORMAL ||
		(c->special_status & AP_CHARACTER_SPECIAL_STATUS_STUN) ||
		(c->special_status & AP_CHARACTER_SPECIAL_STATUS_SLEEP)) {
		return;
	}
	if (c->is_following) {
		struct ap_character * t = 
			as_map_get_character(mod->as_map, c->follow_id);
		if (t) {
			if (au_distance2d(&c->pos, &t->pos) <= (float)c->follow_distance) {
				return;
			}
			c->dst_pos = t->pos;
		}
		else {
			ap_character_stop_movement(mod->ap_character, c);
			return;
		}
	}
	dx = c->dst_pos.x - c->pos.x;
	dz = c->dst_pos.z - c->pos.z;
	dtotal = sqrtf(dx * dx + dz * dz);
	if (dtotal <= 0.0f) {
		stopmove(c);
		return;
	}
	movspeed = (c->is_moving_fast) ? 
		(float)c->factor.char_status.movement_fast : 
		(float)c->factor.char_status.movement;
	movspeed = MAX(movspeed, 0.f);
	movtotal = movspeed * dt / 10.f;
	newpos.x += (movtotal * (dx / dtotal));
	newpos.z += (movtotal * (dz / dtotal));
	ispc = (c->char_type & AP_CHARACTER_TYPE_PC) != 0;
	if (ispc) {
		struct as_map_tile_info tile = as_map_get_tile(mod->as_map, &newpos);
		if (tile.geometry_block & AS_MAP_GB_GROUND) {
			/* TODO: Check if sky is blocked, this is only 
			 * applicable for ArchLord when it is mounted. */
			stop = 1;
		}
	}
	if (!stop) {
		if (movtotal >= dtotal) {
			c->pos = c->dst_pos;
			ap_character_move(mod->ap_character, c, &c->dst_pos);
			stop = 1;
		}
		else {
			ap_character_move(mod->ap_character, c, &newpos);
		}
	}
	if (stop) {
		stopmove(c);
		if (!ispc) {
			/* Sometimes monsters do not stop at destination, 
			 * this packet is broadcast in order to prevent 
			 * such occurences. */
			ap_optimized_packet2_make_char_move_packet(mod->ap_optimized_packet2, c,
				&c->pos, &c->dst_pos, 
				AP_CHARACTER_MOVE_FLAG_STOP, 0);
			as_map_broadcast(mod->as_map, c);
		}
	}
}

static void processaction(
	struct as_character_process_module * mod,
	struct ap_character * c, 
	struct as_character * sc,
	uint64_t tick,
	float dt)
{
	struct ap_character * t;
	int32_t hitrange;
	struct ap_character_action_info action = { 0 };
	boolean killed = FALSE;
	struct ap_skill_character * skillattachment;
	struct ap_skill_character * targetskillattachment;
	struct ap_skill_buffed_combat_effect_arg * combateffect;
	struct ap_skill_buffed_combat_effect_arg * targetcombateffect;
	uint32_t total = 0;
	struct ap_item * consumables[2] = { 0 };
	uint32_t consumablecount = 0;
	struct as_skill_character * srvskillattachment = 
		as_skill_get_character(mod->as_skill, c);
	if (srvskillattachment->cast_queue_count || srvskillattachment->next_cast.skill_id) {
		/* Give priority to skil casts. */
		return;
	}
	t = as_map_get_character(mod->as_map, sc->attack_target_id);
	/* There is a division by attack speed later so make 
	 * sure that it is not zero. */
	if (!t || 
		!ap_character_is_valid_attack_target(mod->ap_character, c, t, FALSE) ||
		!c->factor.attack.attack_speed) {
		/** \todo Callback to consume arrow/mana/lens stone. */
		ap_character_stop_action(mod->ap_character, c);
		return;
	}
	hitrange = c->factor.attack.attack_range + t->temp->factor.attack.hit_range;
	if (au_distance2d(&c->pos, &t->pos) > hitrange) {
		/* Throttle the average number of movement updates 
		 * with a timer. */
		if (tick < sc->next_attack_movement_tick)
			return;
		vec2 dir = { t->pos.x - c->pos.x, t->pos.z - c->pos.z };
		struct au_pos dst = { 0 };
		glm_vec2_normalize(dir);
		dst.x = t->pos.x - 
			dir[0] * t->temp->factor.attack.hit_range / 2.0f;
		dst.y = t->pos.y;
		dst.z = t->pos.z - 
			dir[1] * t->temp->factor.attack.hit_range / 2.0f;
		ap_character_set_movement(mod->ap_character, c, &dst, TRUE);
		sc->next_attack_movement_tick = tick + 50;
		return;
	}
	/* Stop character movement in a silent way.
	 * Using `ap_character_stop_movement` will cause a 
	 * slight but very noticable jitter.
	 *
	 * Client does not need to be informed because starting 
	 * attack animation will stop character movement. */
	stopmove(c);
	action.defense_penalty = 
		ap_character_calc_defense_penalty(mod->ap_character, c, t);
	skillattachment = ap_skill_get_character(mod->ap_skill, c);
	combateffect = &skillattachment->combat_effect_arg;
	targetskillattachment = ap_skill_get_character(mod->ap_skill, t);
	targetcombateffect = &skillattachment->combat_effect_arg;
	if (ap_character_attempt_attack(mod->ap_character, c, t, &action)) {
		uint32_t i;
		uint32_t physical;
		const enum ap_factors_attribute_type spirit[] = {
			AP_FACTORS_ATTRIBUTE_TYPE_MAGIC,
			AP_FACTORS_ATTRIBUTE_TYPE_WATER,
			AP_FACTORS_ATTRIBUTE_TYPE_FIRE,
			AP_FACTORS_ATTRIBUTE_TYPE_EARTH,
			AP_FACTORS_ATTRIBUTE_TYPE_AIR,
			AP_FACTORS_ATTRIBUTE_TYPE_POISON,
			AP_FACTORS_ATTRIBUTE_TYPE_LIGHTENING,
			AP_FACTORS_ATTRIBUTE_TYPE_ICE };
		action.result = AP_CHARACTER_ACTION_RESULT_TYPE_SUCCESS;
		action.broadcast_factor = TRUE;
		physical = ap_character_calc_physical_attack(mod->ap_character, c, t, 
			&action);
		for (i = 0; i < COUNT_OF(spirit); i++) {
			total += ap_character_calc_spirit_attack(mod->ap_character, c, t, 
				&action, spirit[i]);
		}
		total += ap_character_calc_heroic_attack(mod->ap_character, c, t, &action);
		if ((int)ap_random_get(mod->ap_random, 100) < combateffect->melee_critical_rate) {
			physical = (uint32_t)((physical * 
				combateffect->melee_critical_damage_adjust_rate / 100.0f) * 
				((100 - (targetcombateffect->critical_hit_resistance - 
					action.defense_penalty)) / 100.f));
			/* When critical damage is displayed, only physical damage is 
			 * shown. Include all damage types in physical damage. */
			action.target.dmg_normal = -(int)(total + physical);
			action.result = AP_CHARACTER_ACTION_RESULT_TYPE_CRITICAL;
		}
		total += physical;
		if (action.result == AP_CHARACTER_ACTION_RESULT_TYPE_SUCCESS &&
			(int)ap_random_get(mod->ap_random, 100) < MIN(targetcombateffect->damage_to_hp_rate[0], 80)) {
			int regen = (int)(total * 
				targetcombateffect->damage_to_hp_amount[0] / 100.f);
			action.additional_effect |= AP_CHARACTER_ADDITIONAL_EFFECT_CONVERT_HP;
			action.result = AP_CHARACTER_ACTION_RESULT_TYPE_MISS;
			if (regen > 0) {
				t->factor.char_point.hp += regen;
				ap_character_fix_hp(t);
			}
		}
		else if (action.result == AP_CHARACTER_ACTION_RESULT_TYPE_SUCCESS &&
			(int)ap_random_get(mod->ap_random, 100) < MIN(targetcombateffect->damage_to_mp_rate[0], 80)) {
			int regen = (int)(total * 
				targetcombateffect->damage_to_mp_amount[0] / 100.f);
			action.additional_effect |= AP_CHARACTER_ADDITIONAL_EFFECT_CONVERT_MP;
			action.result = AP_CHARACTER_ACTION_RESULT_TYPE_MISS;
			if (regen > 0) {
				t->factor.char_point.mp += regen;
				ap_character_fix_mp(t);
			}
		}
		else {
			if (ap_character_is_pc(c)) {
				struct ap_item * lensstone = 
					ap_item_find_item_in_use_by_cash_item_type(mod->ap_item, c, 
						AP_ITEM_CASH_ITEM_TYPE_ONE_ATTACK, 
						1, AP_ITEM_STATUS_CASH_INVENTORY);
				if (lensstone) {
					as_item_consume(mod->as_item, c, lensstone, 1);
					action.additional_effect |= 
						AP_CHARACTER_ADDITIONAL_EFFECT_LENS_STONE;
				}
			}
			ap_character_special_status_off(mod->ap_character, t, 
				AP_CHARACTER_SPECIAL_STATUS_SLEEP);
			ap_ai2_add_aggro_damage(mod->ap_ai2, t, c->id, total);
			if (total >= t->factor.char_point.hp) {
				killed = TRUE;
				t->factor.char_point.hp = 0;
			}
			else {
				t->factor.char_point.hp -= total;
			}
		}
	}
	action.target.hp = t->factor.char_point.hp;
	action.target.mp = t->factor.char_point.mp;
	c->action_end_tick = tick + (60000 / c->factor.attack.attack_speed);
	c->combat_end_tick = tick + 10000;
	t->combat_end_tick = c->combat_end_tick;
	ap_character_special_status_off(mod->ap_character, c, 
		AP_CHARACTER_SPECIAL_STATUS_INVINCIBLE);
	ap_optimized_packet2_make_char_action_packet(mod->ap_optimized_packet2, c->id, 
		t->id, &action);
	as_map_broadcast(mod->as_map, c);
	if (killed) {
		ap_character_stop_action(mod->ap_character, c);
		ap_character_kill(mod->ap_character, t, c, AP_CHARACTER_DEATH_BY_ATTACK);
	}
}

static void recoverhpmp(
	struct as_character_process_module * mod,
	struct ap_character * c, 
	struct as_character * sc,
	uint64_t tick)
{
	uint64_t flags = 0;
	if (tick < sc->next_recover_tick ||
		c->action_status == AP_CHARACTER_ACTION_STATUS_DEAD) {
		/* TODO: Check if character is a boss or a siege struct. */
		return;
	}
	sc->next_recover_tick = tick + 10000;
	if (c->factor.char_point.hp < c->factor.char_point_max.hp) {
		uint32_t regen = 
			(uint32_t)(c->factor.char_point_max.hp * 0.015f) +
				c->factor.char_point_recovery_rate.hp + 5;
		c->factor.char_point.hp += regen;
		if (c->factor.char_point.hp > c->factor.char_point_max.hp)
			c->factor.char_point.hp = c->factor.char_point_max.hp;
		flags |= AP_FACTORS_BIT_CHAR_POINT;
	}
	if (c->factor.char_point.mp < c->factor.char_point_max.mp) {
		uint32_t regen = 
			(uint32_t)(c->factor.char_point_max.mp * 0.015f) +
				c->factor.char_point_recovery_rate.mp + 5;
		c->factor.char_point.mp += regen;
		if (c->factor.char_point.mp > c->factor.char_point_max.mp)
			c->factor.char_point.mp = c->factor.char_point_max.mp;
		flags |= AP_FACTORS_BIT_CHAR_POINT;
	}
	if (flags) {
		ap_character_make_update_packet(mod->ap_character, c, 
			AP_FACTORS_BIT_CHAR_POINT);
		as_map_broadcast(mod->as_map, c);
	}
}

static void processmonster(
	struct as_character_process_module * mod,
	struct ap_character * c,
	uint64_t tick,
	float dt)
{
	ap_character_process_monster(mod->ap_character, c, tick, dt);
}

static void processchar(
	struct as_character_process_module * mod,
	struct ap_character * c, 
	uint64_t tick, 
	float dt)
{
	struct as_character * sc = as_character_get(mod->as_character, c);
	uint32_t i;
	if (tick >= c->action_end_tick && 
		c->action_status == AP_CHARACTER_ACTION_STATUS_NORMAL) {
		if (sc->move_input.not_processed)
			processmoveinput(mod, c, sc);
		if (c->is_moving)
			processmove(mod, c, tick, dt);
		if (sc->is_attacking)
			processaction(mod, c, sc, tick, dt);
	}
	recoverhpmp(mod, c, sc, tick);
	ap_character_process(mod->ap_character, c, tick, dt);
	if (c->char_type & AGPMCHARACTER_TYPE_MONSTER)
		processmonster(mod, c, tick, dt);
	for (i = 0; i < 64; i++) {
		if ((c->special_status & (1ull << i)) &&
			c->special_status_end_tick[i] &&
			tick >= c->special_status_end_tick[i]) {
			ap_character_special_status_off(mod->ap_character, c, 1ull << i);
		}
	}
}

static boolean cbreceive(
	struct as_character_process_module * mod,
	struct ap_character_cb_receive * cb)
{
	struct ap_character * character = cb->user_data;
	switch (cb->type) {
	case AP_CHARACTER_PACKET_TYPE_MOVE_BANKMONEY: {
		int64_t gold = character->inventory_gold - cb->bank_gold;
		int64_t bankgold = character->bank_gold + cb->bank_gold;
		if (gold < 0 || gold > AP_CHARACTER_MAX_INVENTORY_GOLD)
			break;
		if (bankgold < 0 || bankgold > AP_CHARACTER_MAX_BANK_GOLD)
			break;
		character->inventory_gold = gold;
		character->bank_gold = bankgold;
		as_player_get_character_ad(mod->as_player, 
			character)->account->bank_gold = bankgold;
		ap_character_update(mod->ap_character, character, 
			AP_CHARACTER_BIT_INVENTORY_GOLD | AP_CHARACTER_BIT_BANK_GOLD, TRUE);
		break;
	}
	case AP_CHARACTER_PACKET_TYPE_REQUEST_RESURRECTION_TOWN: {
		const struct au_pos * destination;
		if (character->action_status != AP_CHARACTER_ACTION_STATUS_DEAD)
			break;
		destination = as_event_binding_find_by_type(mod->as_event_binding, 
			character->bound_region_id, AP_EVENT_BINDING_TYPE_RESURRECTION,
			NULL);
		if (!destination) {
			WARN("No resurrection point available in region (%u).", 
				character->bound_region_id);
			destination = &character->pos;
		}
		if (0) {
			/* \todo Should character lose experience? */
		}
		ap_character_set_action_status(mod->ap_character, character, 
			AP_CHARACTER_ACTION_STATUS_NORMAL);
		character->factor.char_point.hp = character->factor.char_point_max.hp;
		character->factor.char_point.mp = character->factor.char_point_max.mp;
		ap_character_update(mod->ap_character, character, 
			AP_FACTORS_BIT_HP | AP_FACTORS_BIT_MP, FALSE);
		ap_character_special_status_on(mod->ap_character, character, 
			AP_CHARACTER_SPECIAL_STATUS_INVINCIBLE, 10000);
		ap_event_teleport_start(mod->ap_event_teleport, character, destination);
		break;
	}
	case AP_CHARACTER_PACKET_TYPE_UPDATE_GAMEPLAY_OPTION: {
		const int * options = (const int *)cb->skill_init;
		character->stop_experience = options[0] != 0;
		character->auto_attack_after_skill_cast = options[1] != 0;
		ap_character_make_update_gameplay_option_packet(mod->ap_character, character);
		as_player_send_packet(mod->as_player, character);
		break;
	}
	}
	return TRUE;
}

static boolean cbcharsetmovement(
	struct as_character_process_module * mod,
	struct ap_character_cb_set_movement * cb)
{
	struct ap_character * c = cb->character;
	struct as_character * sc = as_character_get(mod->as_character, c);
	uint8_t moveflags = 0;
	sc->move_input.not_processed = FALSE;
	c->dst_pos = cb->dst;
	c->is_moving = TRUE;
	if (cb->move_fast) {
		c->is_moving_fast = TRUE;
		moveflags |= AP_CHARACTER_MOVE_FLAG_FAST;
	}
	else {
		c->is_moving_fast = FALSE;
	}
	c->is_following = FALSE;
	/** \todo Turn (rotate) character. */
	ap_optimized_packet2_make_char_move_packet(mod->ap_optimized_packet2, c,
		&c->pos, &c->dst_pos, 
		moveflags, 0);
	as_map_broadcast(mod->as_map, c);
	return TRUE;
}

static boolean cbcharfollow(
	struct as_character_process_module * mod,
	struct ap_character_cb_follow * cb)
{
	struct ap_character * c = cb->character;
	struct ap_character * t = cb->target;
	struct as_character * sc;
	uint8_t moveflags = AP_CHARACTER_MOVE_FLAG_FOLLOW;
	if (c->is_following && 
		c->follow_id == t->id && 
		c->follow_distance == cb->distance &&
		c->is_moving_fast == cb->move_fast) {
		return TRUE;
	}
	sc = as_character_get(mod->as_character, c);
	sc->move_input.not_processed = FALSE;
	c->dst_pos = t->pos;
	c->is_moving = TRUE;
	if (cb->move_fast) {
		c->is_moving_fast = TRUE;
		moveflags |= AP_CHARACTER_MOVE_FLAG_FAST;
	}
	else {
		c->is_moving_fast = FALSE;
	}
	c->is_following = TRUE;
	c->follow_id = t->id;
	c->follow_distance = cb->distance;
	/** \todo Turn (rotate) character. */
	ap_optimized_packet2_make_char_move_packet(mod->ap_optimized_packet2, c,
		&c->pos, &c->dst_pos, 
		moveflags, 0);
	as_map_broadcast(mod->as_map, c);
	return TRUE;
}

static boolean cbcharstopmovement(
	struct as_character_process_module * mod,
	struct ap_character_cb_stop_movement * cb)
{
	struct ap_character * c = cb->character;
	if (cb->was_moving) {
		ap_optimized_packet2_make_char_move_packet(mod->ap_optimized_packet2,
			c, &c->pos, NULL, 
			AP_CHARACTER_MOVE_FLAG_STOP, 0);
		as_map_broadcast(mod->as_map, c);
	}
	return TRUE;
}

static boolean cbcharstopaction(
	struct as_character_process_module * mod,
	struct ap_character_cb_stop_action * cb)
{
	struct ap_character * c = cb->character;
	struct as_character * sc = as_character_get(mod->as_character, c);
	sc->is_attacking = FALSE;
	sc->attack_target_id = 0;
	sc->next_attack_movement_tick = 0;
	return TRUE;
}

static boolean cbcharisvalidattacktarget(
	struct as_character_process_module * mod,
	struct ap_character_cb_is_valid_attack_target * cb)
{
	struct ap_character * c = cb->character;
	struct ap_character * t = cb->target;
	/** \todo
	 * Check:
	 * - If character or target is mutated.
	 * - Siege related conditions that invalidate target.
	 * - If target is in the same party as character.
	 * - Battleground race restrictions.
	 * - Battle (arena) state. */
	if (!CHECK_BIT(t->char_type, AP_CHARACTER_TYPE_TARGETABLE))
		return FALSE;
	if (!CHECK_BIT(t->char_type, AP_CHARACTER_TYPE_ATTACKABLE))
		return FALSE;
	if (c->action_status == AP_CHARACTER_ACTION_STATUS_DEAD)
		return FALSE;
	if (t->action_status == AP_CHARACTER_ACTION_STATUS_DEAD)
		return FALSE;
	if (CHECK_BIT(c->special_status, AP_CHARACTER_SPECIAL_STATUS_DISABLE_NORMAL_ATK))
		return FALSE;
	if (CHECK_BIT(t->special_status, AP_CHARACTER_SPECIAL_STATUS_INVINCIBLE))
		return FALSE;
	if (CHECK_BIT(t->special_status, AP_CHARACTER_SPECIAL_STATUS_TRANSPARENT))
		return FALSE;
	if (c->summoned_by == t || t->summoned_by == c)
		return FALSE;
	return TRUE;
}

static boolean cbchardeath(
	struct as_character_process_module * mod,
	struct ap_character_cb_death * cb)
{
	struct ap_character * character = cb->character;
	if (CHECK_BIT(character->char_type, AP_CHARACTER_TYPE_MONSTER) && 
		cb->killer &&
		ap_character_is_pc(cb->killer)) {
		struct ap_skill_buffed_factor_effect_arg * buffedfactor = 
			&ap_skill_get_character(mod->ap_skill, cb->killer)->factor_effect_arg;
		uint64_t amount;
		/* \todo Distribute experience according to damage contribution. 
		 * If killer is in a party, use selected party experience 
		 * distribution mode. */
		if (ap_character_is_target_out_of_level_range(cb->killer, character) && 0) {
			/* \todo Temporarily disabled branch for testing purposes. */
			amount = 1;
		}
		else {
			amount = character->temp->factor.char_point_max.base_exp;
			amount = (uint64_t)(amount * mod->exp_rate * (100.0f + 
				buffedfactor->bonus_exp_rate) / 100.0f);
			if (character->factor.char_status.murderer >= AP_CHARACTER_MURDERER_LEVEL3_POINT) {
				/* Halve the experience gained for Lv3 rogues. */
				amount /= 2;
			}
		}
		ap_character_gain_experience(mod->ap_character, cb->killer, amount);
	}
	return TRUE;
}

static boolean cbchargainexperience(
	struct as_character_process_module * mod,
	struct ap_character_cb_gain_experience * cb)
{
	struct ap_character * character = cb->character;
	uint64_t currentexp;
	uint64_t maxexp;
	uint32_t currentlevel;
	uint32_t nextlevel;
	struct as_map_region * region;
	if (CHECK_BIT(character->special_status, AP_CHARACTER_SPECIAL_STATUS_LEVELLIMIT))
		return TRUE;
	region = as_map_get_character_ad(mod->as_map, character)->region;
	if (region && 
		region->temp->level_limit && 
		region->temp->level_limit <= ap_character_get_absolute_level(character)) {
		return TRUE;
	}
	currentexp = ap_factors_get_current_exp(&character->factor);
	maxexp = ap_factors_get_max_exp(&character->factor);
	currentlevel = ap_character_get_level(character);
	nextlevel = currentlevel;
	character->factor.char_point.bonus_exp = (uint32_t)(cb->amount * 100);
	ap_character_make_update_packet(mod->ap_character, character, 
		AP_FACTORS_BIT_EXP | AP_FACTORS_BIT_BONUS_EXP);
	as_player_send_packet(mod->as_player, character);
	while (currentexp >= maxexp && nextlevel < AP_CHARACTER_MAX_LEVEL) {
		nextlevel++;
		currentexp -= maxexp;
	}
	if (currentlevel != nextlevel) {
		ap_factors_set_current_exp(&character->factor, currentexp);
		ap_character_set_level(mod->ap_character, character, nextlevel);
		ap_character_restore_hpmp(character);
		ap_character_make_packet(mod->ap_character, 
			AP_CHARACTER_PACKET_TYPE_LEVEL_UP, character->id);
		as_map_broadcast(mod->as_map, character);
		ap_character_update(mod->ap_character, character, 
			AP_FACTORS_BIT_HP | AP_FACTORS_BIT_MP | AP_FACTORS_BIT_EXP, FALSE);
	}
	return TRUE;
}

static boolean cbcharsetactionstatus(
	struct as_character_process_module * mod,
	struct ap_character_cb_set_action_status * cb)
{
	struct ap_character * character = cb->character;
	ap_character_update(mod->ap_character, character, 
		AP_CHARACTER_BIT_ACTION_STATUS, FALSE);
	return TRUE;
}

static boolean cbcharmove(
	struct as_character_process_module * mod,
	struct ap_optimized_packet2_cb_receive_char_move * cb)
{
	struct ap_character * c = cb->user_data;
	struct as_character * sc = as_character_get(mod->as_character, c);
	struct as_character_move_input * input = &sc->move_input;
	ap_character_stop_action(mod->ap_character, c);
	if (cb->move_flags & AP_CHARACTER_MOVE_FLAG_CANCEL_ACTION)
		return TRUE;
	input->not_processed = TRUE;
	input->pos = cb->pos;
	input->dst_pos = cb->dst_pos;
	input->move_flags = cb->move_flags;
	input->move_direction = cb->move_direction;
	input->next_action_type = cb->next_action_type;
	return TRUE;
}

static boolean cbcharaction(
	struct as_character_process_module * mod,
	struct ap_optimized_packet2_cb_receive_char_action * cb)
{
	struct ap_character * c = cb->user_data;
	struct as_character * sc;
	struct ap_character * t = as_map_get_character(mod->as_map, cb->target_id);
	if (!t)
		return TRUE;
	if (!ap_character_is_valid_attack_target(mod->ap_character, c, t, cb->force_attack))
		return TRUE;
	sc = as_character_get(mod->as_character, c);
	sc->is_attacking = TRUE;
	sc->attack_target_id = cb->target_id;
	return TRUE;
}

static void cbchatgoadmin(
	struct as_character_process_module * mod,
	struct ap_character * c,
	uint32_t argc,
	const char * const * argv)
{
	struct ap_character * target;
	if (argc < 1)
		return;
	target = as_player_get_by_name(mod->as_player, argv[0]);
	if (target && c != target)
		ap_event_teleport_start(mod->ap_event_teleport, c, &target->pos);
}

static void cbchatgold(
	struct as_character_process_module * mod,
	struct ap_character * c,
	uint32_t argc,
	const char * const * argv)
{
	struct ap_character * target = NULL;
	int32_t add = 0;
	uint32_t prev = 0;
	int64_t balance;
	switch (argc) {
	case 1:
		target = c;
		add = strtol(argv[0], NULL, 10);
		break;
	case 2:
		target = as_player_get_by_name(mod->as_player, argv[0]);
		add = strtol(argv[1], NULL, 10);
		break;
	default:
		return;
	}
	if (!target)
		return;
	balance = target->inventory_gold + add;
	if (balance < 0 || balance > AP_CHARACTER_MAX_INVENTORY_GOLD)
		return;
	target->inventory_gold = (uint64_t)balance;
	ap_character_update(mod->ap_character, target, AP_CHARACTER_BIT_INVENTORY_GOLD, 
		TRUE);
}

static void cbchathp(
	struct as_character_process_module * mod,
	struct ap_character * c,
	uint32_t argc,
	const char * const * argv)
{
	if (argc >= 1) {
		c->factor_point.char_point_max.hp += strtol(argv[0], NULL, 10);
		ap_character_update_factor(mod->ap_character, c, AP_FACTORS_BIT_MAX_HP);
	}
}

static void cbchatkill(
	struct as_character_process_module * mod,
	struct ap_character * c,
	uint32_t argc,
	const char * const * argv)
{
	struct ap_character * target = NULL;
	if (argc != 1)
		return;
	target = as_player_get_by_name(mod->as_player, argv[0]);
	if (!target || target->action_status == AP_CHARACTER_ACTION_STATUS_DEAD)
		return;
	ap_character_kill(mod->ap_character, target, c, AP_CHARACTER_DEATH_BY_GM);
}

static void cbchatmove(
	struct as_character_process_module * mod,
	struct ap_character * c,
	uint32_t argc,
	const char * const * argv)
{
	struct au_pos pos = { 0 };
	const char * cursor;
	if (argc < 1)
		return;
	cursor = strchr(argv[0], ',');
	if (!cursor++)
		return;
	pos.x = strtof(argv[0], NULL);
	pos.z = strtof(cursor, NULL);
	ap_event_teleport_start(mod->ap_event_teleport, c, &pos);
}

static void cbchatpull(
	struct as_character_process_module * mod,
	struct ap_character * c,
	uint32_t argc,
	const char * const * argv)
{
	struct ap_character * target;
	if (argc < 1)
		return;
	target = as_player_get_by_name(mod->as_player, argv[0]);
	if (target && c != target)
		ap_event_teleport_start(mod->ap_event_teleport, target, &c->pos);
}

static void cbchatsetlevel(
	struct as_character_process_module * mod,
	struct ap_character * c,
	uint32_t argc,
	const char * const * argv)
{
	struct ap_character * target = NULL;
	uint32_t level = 0;
	switch (argc) {
	case 1:
		target = c;
		level = strtoul(argv[0], NULL, 10);
		break;
	case 2:
		target = as_player_get_by_name(mod->as_player, argv[0]);
		level = strtoul(argv[1], NULL, 10);
		break;
	default:
		return;
	}
	if (!level || level > AP_CHARACTER_MAX_LEVEL || !target)
		return;
	ap_character_set_level(mod->ap_character, target, level);
}

static void cbchatsetexprate(
	struct as_character_process_module * mod,
	struct ap_character * c,
	uint32_t argc,
	const char * const * argv)
{
	if (argc == 1)
		mod->exp_rate = strtof(argv[0], NULL);
}

static boolean onregister(
	struct as_character_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_ai2, AP_AI2_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_chat, AP_CHAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_teleport, AP_EVENT_TELEPORT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_optimized_packet2, AP_OPTIMIZED_PACKET2_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_pvp, AP_PVP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_random, AP_RANDOM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_skill, AP_SKILL_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_event_binding, AS_EVENT_BINDING_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_item, AS_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_skill, AS_SKILL_MODULE_NAME);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_RECEIVE, mod, cbreceive);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_SET_MOVEMENT, mod, cbcharsetmovement);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_FOLLOW, mod, cbcharfollow);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_STOP_MOVEMENT, mod, cbcharstopmovement);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_STOP_ACTION, mod, cbcharstopaction);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_IS_VALID_ATTACK_TARGET, mod, cbcharisvalidattacktarget);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_DEATH, mod, cbchardeath);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_GAIN_EXPERIENCE, mod, cbchargainexperience);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_SET_ACTION_STATUS, mod, cbcharsetactionstatus);
	ap_optimized_packet2_add_callback(mod->ap_optimized_packet2, AP_OPTIMIZED_PACKET2_CB_RECEIVE_CHAR_MOVE, mod, cbcharmove);
	ap_optimized_packet2_add_callback(mod->ap_optimized_packet2, AP_OPTIMIZED_PACKET2_CB_RECEIVE_CHAR_ACTION, mod, cbcharaction);
	ap_chat_add_command(mod->ap_chat, "/goadmin", mod, cbchatgoadmin);
	ap_chat_add_command(mod->ap_chat, "/gold", mod, cbchatgold);
	ap_chat_add_command(mod->ap_chat, "/hp", mod, cbchathp);
	ap_chat_add_command(mod->ap_chat, "/kill", mod, cbchatkill);
	ap_chat_add_command(mod->ap_chat, "/move", mod, cbchatmove);
	ap_chat_add_command(mod->ap_chat, "/pull", mod, cbchatpull);
	ap_chat_add_command(mod->ap_chat, "/setlevel", mod, cbchatsetlevel);
	ap_chat_add_command(mod->ap_chat, "/setexprate", mod, cbchatsetexprate);
	mod->exp_rate = strtof(ap_config_get(mod->ap_config, "ExpRate"), NULL);
	return TRUE;
}

static void onshutdown(struct as_character_process_module * mod)
{
	vec_free(mod->list);
	vec_free(mod->process_queue);
}

struct as_character_process_module * as_character_process_create_module()
{
	struct as_character_process_module * mod = ap_module_instance_new(AS_CHARACTER_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	mod->list = vec_new_reserved(sizeof(*mod->list), 128);
	mod->process_queue = vec_new_reserved(sizeof(*mod->process_queue), 128);
	mod->recent_processed_count_min = -1;
	mod->recent_processed_count_max = -1;
	return mod;
}

void as_character_process_iterate_all(
	struct as_character_process_module * mod, 
	float dt)
{
	uint32_t i;
	uint32_t count;
	uint32_t queuecount;
	uint32_t remaincount;
	int totalcount = 0;
	uint64_t begintick;
	uint64_t tick;
	/* It is expensive to process all characters in the world each frame. 
	 * Instead, a queue is used and a fixed number of characters
	 * are processed each frame. */
	queuecount = vec_count(mod->process_queue);
	if (!queuecount) {
		/* Queue is inserted only when it is empty. */
		size_t index = 0;
		uint64_t tick = ap_tick_get(mod->ap_tick);
		vec_clear(mod->list);
		while (TRUE) {
			struct ap_character * c = as_map_iterate_characters(mod->as_map, &index);
			if (!c)
				break;
			vec_push_back(&mod->process_queue, &c->id);
		}
		queuecount = vec_count(mod->process_queue);
		mod->queue_cursor = 0;
	}
	remaincount = queuecount - mod->queue_cursor;
	tick = ap_tick_get(mod->ap_tick);
	begintick = tick;
	/* Allocate 5ms to processing characters each frame. */
	do {
		/* Process a maximum of 10 characters at a time. */
		count = queuecount - mod->queue_cursor;
		if (count > 10)
			count = 10;
		for (i = 0; i < count; i++) {
			struct ap_character * character = as_map_get_character(mod->as_map,
				mod->process_queue[mod->queue_cursor + i]);
			if (character) {
				assert(tick >= character->last_process_tick);
				processchar(mod, character, tick, 
					(tick - character->last_process_tick) / 1000.0f);
				character->last_process_tick = tick;
			}
		}
		mod->queue_cursor += count;
		totalcount += count;
		remaincount -= count;
		tick = ap_tick_get(mod->ap_tick);
		if (!remaincount) {
			vec_clear(mod->process_queue);
			break;
		}
	}
	while (tick < begintick + 5);
	/* Collect and report performance for later profiling. */
	if (remaincount) {
		if (mod->recent_processed_count_min == -1 ||
			mod->recent_processed_count_min > totalcount) {
			mod->recent_processed_count_min = totalcount;
			mod->recent_processed_count_min_queue = queuecount;
		}
		if (mod->recent_processed_count_max == -1 ||
			mod->recent_processed_count_max < totalcount) {
			mod->recent_processed_count_max = totalcount;
			mod->recent_processed_count_max_queue = queuecount;
		}
	}
	if (!mod->recent_processed_report_tick) {
		mod->recent_processed_report_tick = tick;
	}
	else if (tick >= mod->recent_processed_report_tick + 60000) {
		if (mod->recent_processed_count_min != -1) {
			TRACE("Min. processed character count: %d/%u.",
				mod->recent_processed_count_min, 
				mod->recent_processed_count_min_queue);
		}
		if (mod->recent_processed_count_max != -1) {
			TRACE("Max. processed character count: %d/%u.",
				mod->recent_processed_count_max, 
				mod->recent_processed_count_max_queue);
		}
		mod->recent_processed_count_min = -1;
		mod->recent_processed_count_max = -1;
		mod->recent_processed_report_tick = tick;
	}
}
