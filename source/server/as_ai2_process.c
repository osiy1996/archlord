#include "server/as_ai2_process.h"

#include "core/log.h"
#include "core/vector.h"

#include "public/ap_ai2.h"
#include "public/ap_character.h"
#include "public/ap_event_teleport.h"

#include "server/as_character.h"
#include "server/as_spawn.h"

#include "vendor/pcg/pcg_basic.h"

#include <assert.h>
#include <time.h>

#define MAX_PATROL_IN_FRAME 16
#define PATROL_INTERVAL_MIN 15000
#define PATROL_INTERVAL_MAX 45000

struct as_ai2_process_module {
	struct ap_module_instance instance;
	struct ap_ai2_module * ap_ai2;
	struct ap_character_module * ap_character;
	struct ap_event_teleport_module * ap_event_teleport;
	struct as_character_module * as_character;
	struct as_map_module * as_map;
	struct as_spawn_module * as_spawn;
	pcg32_random_t rng;
	/* Use a counter to limit the number of 
	 * patrol events per-frame in order to ensure 
	 * consistent frame times. */
	uint32_t patrol_in_frame;
};

static uint32_t getrandom(
	struct as_ai2_process_module * mod, 
	uint32_t nmin, 
	uint32_t nmax)
{
	assert(nmin < nmax);
	return nmin + pcg32_boundedrand_r(&mod->rng, nmax - nmin);
}

static float randomizepoint(
	struct as_ai2_process_module * mod, 
	float point,
	float r)
{
	float d = (float)pcg32_boundedrand_r(&mod->rng, 
		(uint32_t)r * 2);
	return (point + d - r);
}

static boolean makepatrolpos(
	struct as_ai2_process_module * mod,
	struct as_spawn_character * sc,
	struct au_pos * pos)
{
	struct as_map_tile_info tile;
	pos->x = randomizepoint(mod, sc->spawn_pos.x, 
		sc->instance->radius);
	pos->y = sc->spawn_pos.y;
	pos->z = randomizepoint(mod, sc->spawn_pos.z, 
		sc->instance->radius);
	tile = as_map_get_tile(mod->as_map, pos);
	return !(tile.geometry_block & AS_MAP_GB_GROUND);
}

static inline boolean canpatrol(
	struct ap_character * c,
	const struct ap_spawn_instance * instance)
{
	/** \todo 
	 * Check if monster is idle (not attacking, not casting 
	 * skills, not stunned, not immobilized, etc). */
	if (!(c->temp->char_type & AP_CHARACTER_TYPE_MONSTER))
		return FALSE;
	if (instance->radius <= 0.0f)
		return FALSE;
	return TRUE;
}

static boolean cbprocessmonster(
	struct as_ai2_process_module * mod,
	struct ap_character_cb_process_monster * cb)
{
	struct ap_character * c = cb->character;
	struct as_spawn_character * s = as_spawn_get_character(mod->as_spawn, c);
	struct ap_ai2_character_attachment * attachment = 
		ap_ai2_get_character_attachment(mod->ap_ai2, c);
	uint32_t i;
	uint32_t count;
	struct ap_character * target = NULL;
	/* All monsters must be added through spawn instances. */
	assert(s->instance != NULL);
	if (au_distance2d(&c->pos, &s->spawn_pos) > 10000.0f) {
		/* Monster has moved too far out of its zone, teleport it to its
		 * spawn position and regenerate. */
		c->factor.char_point.hp = c->factor.char_point_max.hp;
		ap_character_update(mod->ap_character, c, AP_FACTORS_BIT_HP, FALSE);
		ap_event_teleport_start(mod->ap_event_teleport, c, &s->spawn_pos);
	}
	if (attachment->is_scheduled_to_clear_aggro) {
		vec_clear(attachment->aggro_targets);
		attachment->is_scheduled_to_clear_aggro = FALSE;
		attachment->target_id = 0;
	}
	else {
		/* Clear expired aggro targets. */
		count = vec_count(attachment->aggro_targets);
		for (i = 0; i < count;) {
			struct ap_ai2_aggro_target * aggrotarget = &attachment->aggro_targets[i];
			if (cb->tick >= aggrotarget->end_aggro_tick) {
				i = vec_erase_iterator(attachment->aggro_targets, i);
				count = vec_count(attachment->aggro_targets);
			}
			else {
				i++;
			}
		}
	}
	if (cb->tick < c->action_end_tick ||
		c->action_status != AP_CHARACTER_ACTION_STATUS_NORMAL ||
		CHECK_BIT(c->special_status, AP_CHARACTER_SPECIAL_STATUS_STUN) ||
		CHECK_BIT(c->special_status, AP_CHARACTER_SPECIAL_STATUS_SLEEP)) {
		return TRUE;
	}
	if (cb->tick >= attachment->next_target_scan_tick) {
		uint32_t maxdamage = 0;
		attachment->target_id = 0;
		attachment->next_target_scan_tick = cb->tick + 500;
		count = vec_count(attachment->aggro_targets);
		for (i = 0; i < count; i++) {
			struct ap_ai2_aggro_target * aggrotarget = &attachment->aggro_targets[i];
			struct ap_character * candidate;
			if (aggrotarget->damage < maxdamage)
				continue;
			if (cb->tick < aggrotarget->begin_aggro_tick)
				continue;
			candidate = as_map_get_character(mod->as_map, aggrotarget->character_id);
			if (!candidate)
				continue;
			if (au_distance2d(&c->pos, &candidate->pos) > 5000.0f)
				continue;
			if (!ap_character_is_valid_attack_target(mod->ap_character, c, 
					candidate, FALSE)) {
				continue;
			}
			target = candidate;
			maxdamage = aggrotarget->damage;
		}
		if (target)
			attachment->target_id = target->id;
	}
	else if (attachment->target_id) {
		target = as_map_get_character(mod->as_map, attachment->target_id);
		if (!target || 
			au_distance2d(&c->pos, &target->pos) > 5000.0f ||
			!ap_character_is_valid_attack_target(mod->ap_character, 
				c, target, FALSE)) {
			/* Previous target has become invalid.
			 * Clear target and look for a new target in next process. */
			attachment->target_id = 0;
			attachment->next_target_scan_tick = 0;
			target = NULL;
		}
	}
	if (target) {
		struct as_character * sc = as_character_get(mod->as_character, c);
		sc->is_attacking = TRUE;
		sc->attack_target_id = target->id;
		ap_character_follow(mod->ap_character, c, target, TRUE, 
			c->factor.attack.attack_range);
	}
	else if (canpatrol(c, s->instance) && cb->tick >= attachment->next_patrol_tick) {
		/* In case the speed at which we process patrol events 
		 * falls behind the speed at which patrol events 
		 * regenerate, ignore the limit if patrol is 30 seconds 
		 * behind scheduled time. */
		if (mod->patrol_in_frame < MAX_PATROL_IN_FRAME ||
			cb->tick >= attachment->next_patrol_tick + 30000) {
			struct au_pos pos;
			attachment->next_patrol_tick = cb->tick + getrandom(mod, 
				PATROL_INTERVAL_MIN, PATROL_INTERVAL_MAX);
			mod->patrol_in_frame++;
			if (makepatrolpos(mod, s, &pos))
				ap_character_set_movement(mod->ap_character, c, &pos, FALSE);
			return TRUE;
		}
	}
	return TRUE;
}

static boolean cbchardeath(
	struct as_ai2_process_module * mod,
	struct ap_character_cb_death * cb)
{
	struct ap_ai2_character_attachment * attachment = 
		ap_ai2_get_character_attachment(mod->ap_ai2, cb->character);
	attachment->is_scheduled_to_clear_aggro = TRUE;
	return TRUE;
}

static boolean onregister(
	struct as_ai2_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_ai2, AP_AI2_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_teleport, AP_EVENT_TELEPORT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_spawn, AS_SPAWN_MODULE_NAME);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_PROCESS_MONSTER, mod, cbprocessmonster);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_DEATH, mod, cbchardeath);
	return TRUE;
}

struct as_ai2_process_module * as_ai2_process_create_module()
{
	struct as_ai2_process_module * mod = ap_module_instance_new(AS_AI2_PROCESS_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	pcg32_srandom_r(&mod->rng, (uint64_t)time(NULL), 
		(uint64_t)time);
	return mod;
}

void as_ai2_process_add_callback(
	struct as_ai2_process_module * mod,
	enum as_ai2_process_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

void as_ai2_process_end_frame(struct as_ai2_process_module * mod)
{
	mod->patrol_in_frame = 0;
}
