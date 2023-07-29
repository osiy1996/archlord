#ifndef _AP_SPAWN_H_
#define _AP_SPAWN_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_module_instance.h"

#define AP_SPAWN_MODULE_NAME "AgpmSpawn"

#define AP_SPAWN_MAX_GROUP_NAME_SIZE 32
#define AP_SPAWN_MAX_NAME_SIZE 64

BEGIN_DECLS

struct ap_spawn_module;

enum ap_spawn_callback_id {
	AP_SPAWN_CB_INSTANTIATE,
};

enum ap_spawn_mode_data_index {
	AP_SPAWN_MDI_SPAWN_DATA = 0,
};

enum ap_spawn_type {
	AP_SPAWN_TYPE_NORMAL = 0,
	AP_SPAWN_TYPE_SIEGEWAR_OBJECT,
	AP_SPAWN_TYPE_SIEGEWAR_NPC_GUILD,
	AP_SPAWN_TYPE_SECRET_DUNGEON,
	AP_SPAWN_TYPE_DEKAIN = 4,
	AP_SPAWN_TYPE_BATTLEGROUND_NORMAL = 10,
	AP_SPAWN_TYPE_BATTLEGROUND_ABILITY = 11,
};

struct ap_spawn_data {
	char group_name[AP_SPAWN_MAX_GROUP_NAME_SIZE];
	char name[AP_SPAWN_MAX_NAME_SIZE];
	uint32_t monster_tid;
	uint32_t monster_ai_tid;
	uint32_t monster_level;
	uint32_t monster_count;
	uint32_t max_monster_count;
	/** \brief Respawn time in seconds. */
	uint32_t respawn_time;
	enum ap_spawn_type spawn_type;
	uint32_t rate;
	boolean is_boss;
};

struct ap_spawn_instance {
	char name[64];
	const struct ap_spawn_data * data;
	struct au_pos pos[16];
	uint32_t pos_count;
	struct au_pos random_positions[32];
	int16_t rotation[2];
	float radius;
	/** \brief Min. respawn time in seconds. */
	uint32_t min_respawn_time;
	/** \brief Max. respawn time in seconds. */
	uint32_t max_respawn_time;
	boolean make_announcements;
	boolean spawn_delayed;
};

struct ap_spawn_cb_instantiate {
	struct ap_spawn_instance * instance;
};

struct ap_spawn_module * ap_spawn_create_module();

boolean ap_spawn_read_data(
	struct ap_spawn_module * mod, 
	const char * file_path, 
	boolean decrypt);

boolean ap_spawn_read_instances(
	struct ap_spawn_module * mod,
	const char * file_path, 
	boolean decrypt);

void ap_spawn_add_callback(
	struct ap_spawn_module * mod,
	enum ap_spawn_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t ap_spawn_attach_data(
	struct ap_spawn_module * mod,
	enum ap_spawn_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

struct ap_spawn_data * ap_spawn_get_data(
	struct ap_spawn_module * mod, 
	const char * name);

END_DECLS

#endif /* _AP_SPAWN_H_ */