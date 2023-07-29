#ifndef _AS_SPAWN_H_
#define _AS_SPAWN_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_spawn.h"

#include "server/as_map.h"

#define AS_SPAWN_MODULE_NAME "AgsmSpawn"

enum as_spawn_map_sector_status {
	AS_SPAWN_MAP_SECTOR_STATUS_DEACTIVE,
	AS_SPAWN_MAP_SECTOR_STATUS_ACTIVE,
	AS_SPAWN_MAP_SECTOR_STATUS_AWAIT_ACTIVATION,
	AS_SPAWN_MAP_SECTOR_STATUS_AWAIT_DEACTIVATION,
};

BEGIN_DECLS

/** \brief ap_character attachment. */
struct as_spawn_character {
	const struct ap_spawn_data * data;
	const struct ap_spawn_instance * instance;
	struct au_pos spawn_pos;
	boolean is_active;
	uint64_t remove_tick;
	uint64_t respawn_tick;
};

/** \brief as_map_sector attachment. */
struct as_spawn_map_sector {
	struct ap_character ** bound_monsters;
	uint32_t player_count;
	enum as_spawn_map_sector_status status;
};

struct as_spawn_module * as_spawn_create_module();

struct as_spawn_character * as_spawn_get_character(
	struct as_spawn_module * mod,
	struct ap_character * character);

struct as_spawn_map_sector * as_spawn_get_map_sector(
	struct as_spawn_module * mod,
	struct as_map_sector * sector);

void as_spawn_process(struct as_spawn_module * mod, uint64_t tick);

END_DECLS

#endif /* _AS_SPAWN_H_ */
