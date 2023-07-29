#include <assert.h>

#include "core/log.h"
#include "core/malloc.h"
#include "core/vector.h"

#include "public/ap_admin.h"
#include "public/ap_random.h"
#include "public/ap_tick.h"

#include "server/as_character.h"
#include "server/as_map.h"
#include "server/as_spawn.h"

struct sector_id {
	uint32_t x;
	uint32_t z;
};

struct as_spawn_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_random_module * ap_random;
	struct ap_spawn_module * ap_spawn;
	struct ap_tick_module * ap_tick;
	struct as_character_module * as_character;
	struct as_map_module * as_map;
	size_t character_offset;
	size_t sector_offset;
	struct sector_id * activate;
	struct sector_id * deactivate;
	struct sector_id * sectors;
	struct ap_character ** remove_list;
	struct ap_character ** respawn_list;
};

static float randomizepoint(
	struct as_spawn_module * mod, 
	float point,
	float r)
{
	float d = (float)ap_random_get(mod->ap_random, (uint32_t)r * 2);
	return (point + d - r);
}

static void randomizepos(
	struct as_spawn_module * mod,
	struct ap_spawn_instance * instance)
{
	uint32_t i;
	assert(instance->pos_count != 0);
	if (instance->pos_count > 1) {
		for (i = 0; i < COUNT_OF(instance->random_positions); i++) {
			instance->random_positions[i] = 
				instance->pos[i % instance->pos_count];
		}
	}
	else if (instance->radius > 0.0f) {
		struct au_pos rp[COUNT_OF(instance->random_positions)] = { 0 };
		uint32_t count = 0;
		for (i = 0; i < COUNT_OF(rp); i++) {
			struct au_pos * p = &rp[count];
			struct as_map_tile_info tile;
			p->x = randomizepoint(mod, instance->pos[0].x, 
				instance->radius);
			p->y = instance->pos[0].y;
			p->z = randomizepoint(mod, instance->pos[0].z, 
				instance->radius);
			tile = as_map_get_tile(mod->as_map, p);
			if (!(tile.geometry_block & AS_MAP_GB_GROUND))
				count++;
		}
		if (!count)
			rp[count++] = instance->pos[0];
		for (i = 0; i < COUNT_OF(instance->random_positions); i++)
			instance->random_positions[i] = rp[i % count];
	}
	else {
		for (i = 0; i < COUNT_OF(instance->random_positions); i++)
			instance->random_positions[i] = instance->pos[0];
	}
}

static void getrandompos(
	struct as_spawn_module * mod,
	const struct ap_spawn_instance * instance,
	struct au_pos * pos)
{
	uint32_t i = 
		ap_random_get(mod->ap_random, COUNT_OF(instance->random_positions));
	*pos = instance->random_positions[i];
}

static void randomizerotation(
	struct as_spawn_module * mod, 
	float * roty)
{
	float d = (float)ap_random_get(mod->ap_random, 360);
	*roty = d - 180.0f;
}

void setspawnpos(
	struct as_spawn_module * mod,
	struct ap_character * c, 
	struct as_spawn_character * sc)
{
	/* Monster type is checked through templates because 
	 * all spawns are set as monsters by default. */
	if (c->temp->char_type & AP_CHARACTER_TYPE_MONSTER) {
		getrandompos(mod, sc->instance, &c->pos);
		randomizerotation(mod, &c->rotation_y);
	}
	else {
		c->pos = sc->instance->pos[0];
		c->rotation_x = sc->instance->rotation[0];
		c->rotation_y = sc->instance->rotation[1];
	}
	sc->spawn_pos = c->pos;
}

static void activatesector(
	struct as_spawn_module * mod,
	struct as_spawn_map_sector * s)
{
	uint32_t i;
	uint32_t count = vec_count(s->bound_monsters);
	assert(s->status == AS_SPAWN_MAP_SECTOR_STATUS_AWAIT_ACTIVATION);
	for (i = 0; i < count; i++) {
		struct ap_character * monster = 
			s->bound_monsters[i];
		struct as_spawn_character * sc = 
			as_spawn_get_character(mod, monster);
		sc->is_active = TRUE;
		/* If monster is scheduled to be respawned, 
		 * do not add it to the map ahead of time. */
		if (sc->respawn_tick)
			continue;
		if (!as_map_add_character(mod->as_map, monster)) {
			ERROR("Failed to add bound monster (%s).",
				monster->temp->name);
		}
	}
	s->status = AS_SPAWN_MAP_SECTOR_STATUS_ACTIVE;
}

static void deactivatesector(
	struct as_spawn_module * mod,
	struct as_spawn_map_sector * s)
{
	uint32_t i;
	uint32_t count = vec_count(s->bound_monsters);
	assert(s->status == AS_SPAWN_MAP_SECTOR_STATUS_AWAIT_DEACTIVATION);
	for (i = 0; i < count; i++) {
		struct ap_character * monster = 
			s->bound_monsters[i];
		struct as_spawn_character * sc = 
			as_spawn_get_character(mod, monster);
		sc->is_active = FALSE;
		/* If monster is scheduled to be respawned, 
		 * do not remove it from the map ahead of time. */
		if (sc->respawn_tick)
			continue;
		if (!as_map_remove_character(mod->as_map, monster)) {
			ERROR("Failed to remove bound monster (%s).",
				monster->temp->name);
		}
	}
	s->status = AS_SPAWN_MAP_SECTOR_STATUS_DEACTIVE;
}

static void addunique(
	struct sector_id ** list,
	struct as_map_sector * s)
{
	uint32_t i;
	const struct sector_id * l = *list;
	uint32_t count = vec_count(l);
	struct sector_id id = { s->index_x, s->index_z };
	for (i = 0; i < count; i++) {
		if (l[i].x == s->index_x && l[i].z == s->index_z)
			return;
	}
	vec_push_back(list, &id);
}

static boolean initinstance(
	struct as_spawn_module * mod,
	const struct ap_spawn_data * data,
	struct ap_spawn_instance * instance)
{
	uint32_t i;
	struct as_map_sector * sector = 
		as_map_get_sector_at(mod->as_map, &instance->pos[0]);
	struct as_spawn_map_sector * s = 
		as_spawn_get_map_sector(mod, sector);
	addunique(&mod->sectors, sector);
	randomizepos(mod, instance);
	for (i = 0; i < data->monster_count; i++) {
		struct ap_character * c;
		struct ap_character_template * temp = 
			ap_character_get_template(mod->ap_character, data->monster_tid);
		struct as_spawn_character * sc;
		if (!temp) {
			ERROR("Invalid monster template id (spawn = %s).",
				data->name);
			return FALSE;
		}
		c = ap_character_new(mod->ap_character);
		sc = as_spawn_get_character(mod, c);
		sc->data = data;
		sc->instance = instance;
		ap_character_set_template(mod->ap_character, c, temp);
		c->char_type = AP_CHARACTER_TYPE_MONSTER |
			AP_CHARACTER_TYPE_ATTACKABLE |
			AP_CHARACTER_TYPE_TARGETABLE;
		setspawnpos(mod, c, sc);
		c->factor.char_point_max.hp = temp->factor.char_point_max.hp;
		c->factor.char_point_max.mp = temp->factor.char_point_max.mp;
		ap_character_set_level(mod->ap_character, c, data->monster_level);
		c->login_status = AP_CHARACTER_STATUS_IN_GAME_WORLD;
		ap_character_update_factor(mod->ap_character, c, AP_FACTORS_BIT_ALL);
		ap_character_restore_hpmp(c);
		vec_push_back((void **)&s->bound_monsters, &c);
	}
	return TRUE;
}

static boolean cbinstantiate(
	struct as_spawn_module * mod,
	struct ap_spawn_cb_instantiate * cb)
{
	switch (cb->instance->data->spawn_type) {
	case AP_SPAWN_TYPE_NORMAL:
		return initinstance(mod, cb->instance->data, cb->instance);
	default:
		/** \todo Handle different spawn types (Siege, BG, etc.) */
		break;
	}
	return TRUE;
}

static void removefromlist(
	struct sector_id * list, 
	const struct as_map_sector * s)
{
	uint32_t i;
	uint32_t count = vec_count(list);
	for (i = 0; i < count; i++) {
		if (list[i].x == s->index_x && list[i].z == s->index_z) {
			vec_erase(list, &list[i]);
			return;
		}
	}
}

static boolean cbmapentersectorview(
	struct as_spawn_module * mod,
	struct as_map_cb_enter_sector_view * cb)
{
	struct as_spawn_map_sector * s = 
		as_spawn_get_map_sector(mod, cb->sector);
	if (cb->character->char_type & AP_CHARACTER_TYPE_PC) {
		if (s->player_count++ == 0) {
			/* The first player to enter this sector.
			 * Monsters in this sector need to be added to map. */
			assert(s->status == AS_SPAWN_MAP_SECTOR_STATUS_DEACTIVE ||
				s->status == AS_SPAWN_MAP_SECTOR_STATUS_AWAIT_DEACTIVATION);
			switch (s->status) {
			case AS_SPAWN_MAP_SECTOR_STATUS_AWAIT_DEACTIVATION:
				removefromlist(mod->deactivate, cb->sector);
				s->status = AS_SPAWN_MAP_SECTOR_STATUS_ACTIVE;
				break;
			case AS_SPAWN_MAP_SECTOR_STATUS_DEACTIVE: {
				struct sector_id id = { cb->sector->index_x,
					cb->sector->index_z };
				vec_push_back(&mod->activate, &id);
				s->status = AS_SPAWN_MAP_SECTOR_STATUS_AWAIT_ACTIVATION;
				break;
			}
			}
		}
	}
	return TRUE;
}

static boolean cbmapleavesectorview(
	struct as_spawn_module * mod,
	struct as_map_cb_leave_sector_view * cb)
{
	struct as_spawn_map_sector * s = 
		as_spawn_get_map_sector(mod, cb->sector);
	if (cb->character->char_type & AP_CHARACTER_TYPE_PC) {
		assert(s->player_count != 0);
		if (--s->player_count == 0) {
			/* The last player to leave this sector.
			 * Monsters in this sector need to be removed 
			 * from map. */
			switch (s->status) {
			case AS_SPAWN_MAP_SECTOR_STATUS_AWAIT_ACTIVATION:
				removefromlist(mod->activate, cb->sector);
				s->status = AS_SPAWN_MAP_SECTOR_STATUS_DEACTIVE;
				break;
			case AS_SPAWN_MAP_SECTOR_STATUS_ACTIVE: {
				struct sector_id id = { cb->sector->index_x,
					cb->sector->index_z };
				vec_push_back(&mod->deactivate, &id);
				s->status = AS_SPAWN_MAP_SECTOR_STATUS_AWAIT_DEACTIVATION;
				break;
			}
			default:
				/* It is possible for this sector to have been
				 * deactivated already when the module was closed 
				 * so adding an assertion is likely to give 
				 * false errors. */
				break;
			}
		}
	}
	return TRUE;
}

static boolean mapsectorctor(
	struct as_spawn_module * mod, 
	struct as_map_sector * sector)
{
	struct as_spawn_map_sector * s = as_spawn_get_map_sector(mod, sector);
	s->bound_monsters = vec_new(sizeof(*s->bound_monsters));
	return TRUE;
}

static boolean mapsectordtor(
	struct as_spawn_module * mod,
	struct as_map_sector * sector)
{
	struct as_spawn_map_sector * s = as_spawn_get_map_sector(mod, sector);
	vec_free(s->bound_monsters);
	return TRUE;
}

static boolean cbchardeath(
	struct as_spawn_module * mod,
	struct ap_character_cb_death * cb)
{
	struct ap_character * c = cb->character;
	struct as_spawn_character * sc;
	uint64_t tick;
	if (!CHECK_BIT(c->char_type, AP_CHARACTER_TYPE_MONSTER))
		return TRUE;
	sc = as_spawn_get_character(mod, c);
	if (!sc->instance)
		return TRUE;
	assert(sc->remove_tick == 0);
	assert(sc->respawn_tick == 0);
	tick = ap_tick_get(mod->ap_tick);
	sc->remove_tick = tick + 20000;
	if (sc->instance->min_respawn_time < sc->instance->max_respawn_time) {
		sc->respawn_tick = sc->remove_tick + 
			(uint64_t)ap_random_between(mod->ap_random,
				sc->instance->min_respawn_time, 
				sc->instance->max_respawn_time) * 1000;
	}
	else {
		sc->respawn_tick = sc->remove_tick + 
			(uint64_t)sc->data->respawn_time * 1000;
	}
	vec_push_back((void **)&mod->remove_list, &c);
	return TRUE;
}

static boolean onregister(
	struct as_spawn_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_random, AP_RANDOM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_spawn, AP_SPAWN_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	mod->character_offset = ap_character_attach_data(mod->ap_character, 
		AP_CHARACTER_MDI_CHAR, sizeof(struct as_spawn_character),
		mod, NULL, NULL);
	if (mod->character_offset == SIZE_MAX) {
		ERROR("Failed to attach character data.");
		return FALSE;
	}
	mod->sector_offset = as_map_attach_data(mod->as_map, 
		AS_MAP_MDI_SECTOR, sizeof(struct as_spawn_map_sector),
		mod, mapsectorctor, mapsectordtor);
	if (mod->sector_offset == SIZE_MAX) {
		ERROR("Failed to attach map sector data.");
		return FALSE;
	}
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_DEATH,
		mod, cbchardeath);
	ap_spawn_add_callback(mod->ap_spawn, AP_SPAWN_CB_INSTANTIATE, 
		mod, cbinstantiate);
	as_map_add_callback(mod->as_map, AS_MAP_CB_ENTER_SECTOR_VIEW, 
		mod, cbmapentersectorview);
	as_map_add_callback(mod->as_map, AS_MAP_CB_LEAVE_SECTOR_VIEW, 
		mod, cbmapleavesectorview);
	return TRUE;
}

static void onclose(struct as_spawn_module * mod)
{
	uint32_t i;
	uint32_t count = vec_count(mod->sectors);
	/* First, deactivate all sectors. */
	vec_clear(mod->deactivate);
	for (i = 0; i < count; i++) {
		struct as_map_sector * sector = as_map_get_sector(mod->as_map, 
			mod->sectors[i].x, mod->sectors[i].z);
		struct as_spawn_map_sector * s = 
			as_spawn_get_map_sector(mod, sector);
		switch (s->status) {
		case AS_SPAWN_MAP_SECTOR_STATUS_ACTIVE:
			s->status = AS_SPAWN_MAP_SECTOR_STATUS_AWAIT_DEACTIVATION;
		case AS_SPAWN_MAP_SECTOR_STATUS_AWAIT_DEACTIVATION:
			deactivatesector(mod, s);
			break;
		}
	}
	count = vec_count(mod->remove_list);
	for (i = 0; i < count; i++) {
		struct ap_character * c = mod->remove_list[i];
		struct as_spawn_character * sc = as_spawn_get_character(mod, c);
		assert(sc->remove_tick != 0);
		sc->remove_tick = 0;
		if (!as_map_remove_character(mod->as_map, c)) {
			ERROR("Failed to remove dead monster ([%u] %s) from map.",
				c->tid, c->temp->name);
		}
	}
	vec_clear(mod->remove_list);
	vec_clear(mod->respawn_list);
	/* After all characters are removed from the map, they 
	 * can be destroyed. */
	count = vec_count(mod->sectors);
	for (i = 0; i < count; i++) {
		struct as_map_sector * sector = as_map_get_sector(mod->as_map, 
			mod->sectors[i].x, mod->sectors[i].z);
		struct as_spawn_map_sector * s = as_spawn_get_map_sector(mod, sector);
		uint32_t j;
		uint32_t count2 = vec_count(s->bound_monsters);
		for (j = 0; j < count2; j++)
			ap_character_free(mod->ap_character, s->bound_monsters[j]);
		vec_clear(s->bound_monsters);
	}
}

static void onshutdown(struct as_spawn_module * mod)
{
	vec_free(mod->activate);
	vec_free(mod->deactivate);
	vec_free(mod->sectors);
}

struct as_spawn_module * as_spawn_create_module()
{
	struct as_spawn_module * mod = ap_module_instance_new(AS_SPAWN_MODULE_NAME,
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	mod->activate = vec_new(sizeof(*mod->activate));
	mod->deactivate = vec_new(sizeof(*mod->deactivate));
	mod->sectors = vec_new(sizeof(*mod->sectors));
	mod->remove_list = vec_new_reserved(
		sizeof(*mod->remove_list), 128);
	mod->respawn_list = vec_new_reserved(
		sizeof(*mod->respawn_list), 128);
	return mod;
}

struct as_spawn_character * as_spawn_get_character(
	struct as_spawn_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->character_offset);
}

struct as_spawn_map_sector * as_spawn_get_map_sector(
	struct as_spawn_module * mod,
	struct as_map_sector * sector)
{
	return ap_module_get_attached_data(sector, mod->sector_offset);
}

void as_spawn_process(struct as_spawn_module * mod, uint64_t tick)
{
	uint32_t i;
	uint32_t count = vec_count(mod->activate);
	for (i = 0; i < count; i++) {
		struct as_map_sector * s = as_map_get_sector(mod->as_map, 
			mod->activate[i].x, mod->activate[i].z);
		activatesector(mod, as_spawn_get_map_sector(mod, s));
	}
	vec_clear(mod->activate);
	count = vec_count(mod->deactivate);
	for (i = 0; i < count; i++) {
		struct as_map_sector * s = as_map_get_sector(mod->as_map, 
			mod->deactivate[i].x, mod->deactivate[i].z);
		deactivatesector(mod, as_spawn_get_map_sector(mod, s));
	}
	vec_clear(mod->deactivate);
	count = vec_count(mod->respawn_list);
	for (i = 0; i < count;) {
		struct ap_character * c = mod->respawn_list[i];
		struct as_spawn_character * sc = as_spawn_get_character(mod, c);
		assert(sc->respawn_tick != 0);
		if (tick >= sc->respawn_tick) {
			sc->respawn_tick = 0;
			setspawnpos(mod, c, sc);
			c->factor.char_point.hp = c->factor.char_point_max.hp;
			c->factor.char_point.mp = c->factor.char_point_max.mp;
			c->action_status = AP_CHARACTER_ACTION_STATUS_NORMAL;
			vec_erase(mod->respawn_list, &mod->respawn_list[i]);
			i--;
			count--;
			/* Do not add to map if monsters in that sector 
			 * are not active. */
			if (sc->is_active && !as_map_add_character(mod->as_map, c)) {
				ERROR("Failed to respawn monster ([%u] %s) to map.",
					c->tid, c->temp->name);
			}
		}
		else {
			i++;
		}
	}
	count = vec_count(mod->remove_list);
	for (i = 0; i < count;) {
		struct ap_character * c = mod->remove_list[i];
		struct as_spawn_character * sc = as_spawn_get_character(mod, c);
		assert(sc->remove_tick != 0);
		if (tick >= sc->remove_tick) {
			sc->remove_tick = 0;
			if (!as_map_remove_character(mod->as_map, c)) {
				ERROR("Failed to remove dead monster ([%u] %s) from map.",
					c->tid, c->temp->name);
			}
			vec_erase(mod->remove_list, &mod->remove_list[i]);
			i--;
			count--;
			vec_push_back((void **)&mod->respawn_list, &c);
		}
		else {
			i++;
		}
	}
}
