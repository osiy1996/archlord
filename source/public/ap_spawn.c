#include <assert.h>
#include <stdlib.h>
#include <time.h>

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_admin.h"
#include "public/ap_character.h"
#include "public/ap_spawn.h"

#include "utility/au_table.h"

enum data_column_id {
	DCID_GROUPNAME,
	DCID_NAME,
	DCID_TOTALMOBCOUNT,
	DCID_SPAWNTIME,
	DCID_MOBTID,
	DCID_MOBLV,
	DCID_RATE,
	DCID_MAXMOBCOUNT,
	DCID_MOBAITID,
	DCID_SIEGEWARTYPE,
	DCID_ISBOSS,
};

enum instance_column_id {
	ICID_NAME,
	ICID_POSITION,
	ICID_ROTATION,
	ICID_RADIUS,
};

struct ap_spawn_module {
	struct ap_module_instance instance;
	struct ap_admin data_admin;
	struct ap_spawn_instance * instances;
};

static void onshutdown(struct ap_spawn_module * mod)
{
	vec_free(mod->instances);
	ap_admin_destroy(&mod->data_admin);
}

struct ap_spawn_module * ap_spawn_create_module()
{
	struct ap_spawn_module * mod = ap_module_instance_new(AP_SPAWN_MODULE_NAME,
		sizeof(*mod), NULL, NULL, NULL, onshutdown);
	ap_admin_init(&mod->data_admin, sizeof(struct ap_spawn_data), 128);
	mod->instances = vec_new_reserved(sizeof(struct ap_spawn_instance), 512);
	return mod;
}

boolean ap_spawn_read_data(
	struct ap_spawn_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	r &= au_table_set_column(table,
		"GroupName", DCID_GROUPNAME);
	r &= au_table_set_column(table,
		"Name", DCID_NAME);
	r &= au_table_set_column(table,
		"TotalMobCount", DCID_TOTALMOBCOUNT);
	r &= au_table_set_column(table,
		"Spawntime", DCID_SPAWNTIME);
	r &= au_table_set_column(table,
		"MobTID", DCID_MOBTID);
	r &= au_table_set_column(table,
		"MobLV", DCID_MOBLV);
	r &= au_table_set_column(table,
		"Rate", DCID_RATE);
	r &= au_table_set_column(table,
		"MaxMobCount", DCID_MAXMOBCOUNT);
	r &= au_table_set_column(table,
		"MobAITID", DCID_MOBAITID);
	r &= au_table_set_column(table,
		"SiegeWarType", DCID_SIEGEWARTYPE);
	r &= au_table_set_column(table,
		"IsBoss", DCID_ISBOSS);
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		struct ap_spawn_data * data = NULL;
		char groupname[AP_SPAWN_MAX_GROUP_NAME_SIZE] = { 0 };
		while (au_table_read_next_column(table)) {
			enum data_col_id id = au_table_get_column(table);
			const char * value = au_table_get_value(table);
			if (id == DCID_GROUPNAME) {
				strlcpy(groupname, value, sizeof(groupname));
				continue;
			}
			else if (id == DCID_NAME) {
				char name[AP_SPAWN_MAX_NAME_SIZE];
				strlcpy(name, value, sizeof(name));
				data = ap_admin_add_object_by_name(
					&mod->data_admin, name);
				if (!data) {
					ERROR("Failed to add spawn data (%s).",
						name);
					au_table_destroy(table);
					return FALSE;
				}
				strlcpy(data->group_name, groupname, 
					sizeof(data->group_name));
				strlcpy(data->name, name, sizeof(data->name));
				continue;
			}
			if (!data) {
				assert(0);
				continue;
			}
			switch (id) {
			case DCID_TOTALMOBCOUNT:
				data->monster_count = strtoul(value, NULL, 10);
				break;
			case DCID_SPAWNTIME:
				data->respawn_time = strtoul(value, NULL, 10);
				break;
			case DCID_MOBTID:
				data->monster_tid = strtoul(value, NULL, 10);
				break;
			case DCID_MOBLV:
				data->monster_level = strtoul(value, NULL, 10);
				break;
			case DCID_RATE:
				data->rate = strtoul(value, NULL, 10);
				break;
			case DCID_MAXMOBCOUNT:
				data->max_monster_count = strtoul(value, NULL, 10);
				break;
			case DCID_MOBAITID:
				data->monster_ai_tid = strtoul(value, NULL, 10);
				break;
			case DCID_SIEGEWARTYPE:
				data->spawn_type = strtoul(value, NULL, 10);
				break;
			case DCID_ISBOSS:
				data->is_boss = strtol(value, NULL, 10) != 0;
				break;
			default:
				ERROR("Invalid column id (%u).", id);
				au_table_destroy(table);
				return FALSE;
			}
		}
	}
	au_table_destroy(table);
	return TRUE;
}

boolean ap_spawn_read_instances(
	struct ap_spawn_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	uint32_t i;
	uint32_t count;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	r &= au_table_set_column(table,
		"Name", ICID_NAME);
	r &= au_table_set_column(table,
		"Position", ICID_POSITION);
	r &= au_table_set_column(table,
		"Rotation", ICID_ROTATION);
	r &= au_table_set_column(table,
		"Radius", ICID_RADIUS);
	while (au_table_read_next_line(table)) {
		struct ap_spawn_instance * instance = NULL;
		while (au_table_read_next_column(table)) {
			enum data_col_id id = au_table_get_column(table);
			const char * value = au_table_get_value(table);
			if (id == ICID_NAME) {
				const struct ap_spawn_data * data = 
					ap_spawn_get_data(mod, value);
				if (!data) {
					WARN("Invalid spawn instance (%s).", value);
					break;
				}
				instance = vec_add_empty(&mod->instances);
				strlcpy(instance->name, data->name, 
					sizeof(instance->name));
				instance->data = data;
				continue;
			}
			if (!instance) {
				assert(0);
				continue;
			}
			switch (id) {
			case ICID_POSITION: {
				struct au_pos * pos = &instance->pos[0];
				if (sscanf(value, "%g,%g,%g", 
						&pos->x, &pos->y, &pos->z) != 3) {
					ERROR("Failed to read spawn position (%s).",
						instance->name);
					au_table_destroy(table);
					return FALSE;
				}
				instance->pos_count++;
				break;
			}
			case ICID_ROTATION:
				if (sscanf(value, "%hd,%hd", 
						&instance->rotation[0], 
						&instance->rotation[1]) != 2) {
					ERROR("Failed to read spawn rotation (%s).",
						instance->name);
					au_table_destroy(table);
					return FALSE;
				}
				break;
			case ICID_RADIUS:
				instance->radius = strtof(value, NULL);
				break;
			default:
				ERROR("Invalid column id (%u).", id);
				au_table_destroy(table);
				return FALSE;
			}
		}
	}
	au_table_destroy(table);
	count = vec_count(mod->instances);
	for (i = 0; i < count; i++) {
		struct ap_spawn_cb_instantiate cb = { &mod->instances[i] };
		if (!ap_module_enum_callback(mod, AP_SPAWN_CB_INSTANTIATE, &cb)) {
			ERROR("Failed to instantiate spawn instance (%s).",
				mod->instances[i].name);
			return FALSE;
		}
	}
	return TRUE;
}

void ap_spawn_add_callback(
	struct ap_spawn_module * mod,
	enum ap_spawn_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t ap_spawn_attach_data(
	struct ap_spawn_module * mod,
	enum ap_spawn_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

struct ap_spawn_data * ap_spawn_get_data(
	struct ap_spawn_module * mod, 
	const char * name)
{
	return ap_admin_get_object_by_name(&mod->data_admin, name);
}
