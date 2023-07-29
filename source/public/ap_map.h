#ifndef _AP_MAP_H_
#define _AP_MAP_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module_instance.h"

#define AP_MAP_MODULE_NAME "ApmMap"

#define AP_MAP_MAX_REGION_ID 349
#define AP_MAP_MAX_REGION_COUNT (AP_MAP_MAX_REGION_ID + 1)

BEGIN_DECLS

enum ap_map_material_type {
	AP_MAP_MATERIAL_SOIL,
	AP_MAP_MATERIAL_SWAMP,
	AP_MAP_MATERIAL_GRASS,
	AP_MAP_MATERIAL_SAND,
	AP_MAP_MATERIAL_LEAF,
	AP_MAP_MATERIAL_SNOW,
	AP_MAP_MATERIAL_WATER,
	AP_MAP_MATERIAL_STONE,
	AP_MAP_MATERIAL_WOOD,
	AP_MAP_MATERIAL_METAL,
	AP_MAP_MATERIAL_BONE,
	AP_MAP_MATERIAL_MUD,
	AP_MAP_MATERIAL_SOILGRASS,
	AP_MAP_MATERIAL_SOLIDSOIL,
	AP_MAP_MATERIAL_SPORE,
	AP_MAP_MATERIAL_MOSS,
	AP_MAP_MATERIAL_GRANITE,
	AP_MAP_MATERIAL_COUNT
};

enum ap_map_field_type {
	AP_MAP_FT_FIELD = 0,
	AP_MAP_FT_TOWN = 1,
	AP_MAP_FT_PVP = 2,
};

enum ap_map_safety_type {
	AP_MAP_ST_SAFE = 0,
	AP_MAP_ST_FREE = 1,
	AP_MAP_ST_DANGER = 2,
};

enum ap_map_region_peculiarity {
	AP_MAP_RP_DISABLE_SUMMON = 1u << 0,
	AP_MAP_RP_DISABLE_GO = 1u << 1,
	AP_MAP_RP_DISABLE_PARTY = 1u << 2,
	AP_MAP_RP_DISABLE_LOGIN = 1u << 3,
};

enum ap_map_callback_id {
	/** \brief Triggered after region templates are read. */
	AP_MAP_CB_INIT_REGION,
};

struct ap_map_region_properties {
	enum ap_map_field_type field_type : 2;
	enum ap_map_safety_type safety_type : 2;
	uint8_t is_ridable : 1;
	uint8_t allow_pets : 1;
	uint8_t allow_round_trip : 1;
	uint8_t allow_potion : 1;
	uint8_t allow_resurrection : 1;
	uint8_t disable_minimap : 1;
	uint8_t is_jail : 1;
	uint8_t zone_load_area : 1;
	uint8_t block_characters : 1;
	uint8_t allow_potion_type2 : 1;
	uint8_t allow_guild_potion : 1;
	uint8_t is_interior : 1;
	uint8_t allow_summon_orb : 1;
	uint8_t allow_vitality_potion : 1;
	uint8_t allow_cure_potion : 1;
	uint8_t allow_recovery_potion : 1;
	uint8_t reserved2;
	uint8_t reserved3;
};

union ap_map_region_type {
	uint32_t value;
	struct ap_map_region_properties props;
};

struct ap_map_region_template {
	boolean in_use;
	uint32_t id;
	int32_t parent_id;
	char name[64];
	char comment[64];
	const char * glossary;
	union ap_map_region_type type;
	uint32_t peculiarity;
	uint32_t world_map_index;
	uint32_t sky_index;
	float visible_distance;
	float max_camera_height;
	uint32_t level_limit;
	uint32_t level_min;
	uint32_t disabled_item_section;
	float resurrection_pos[2];
	float zone_src[2];
	float zone_dst[2];
	float zone_height;
	float zone_radius;
};

struct ap_map_region_glossary {
	char name[64];
	char label[64];
};

struct ap_map_module * ap_map_create_module();

void ap_map_add_callback(
	struct ap_map_module * mod,
	enum ap_map_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

/*
 * Reads region templates from file.
 *
 * `region_tmp` is expected to be an array of 
 * size `AP_MAP_MAX_REGION_ID + 1`.
 */
boolean ap_map_read_region_tmp(
	struct ap_map_module * mod,
	const char * file_path,
	struct ap_map_region_template * region_tmp,
	boolean decrypt);

/*
 * Writes region templates to file.
 *
 * `region_tmp` is expected to be an array of 
 * size `AP_MAP_MAX_REGION_ID + 1`.
 */
boolean ap_map_write_region_tmp(
	struct ap_map_module * mod,
	const char * file_path,
	const struct ap_map_region_template * region_tmp,
	boolean encrypt);

/*
 * Reads region glossary file.
 * 
 * If successful, returns vector of glossary objects.
 * 
 * If not successful, returns NULL.
 */
struct ap_map_region_glossary * ap_map_read_region_glossary(
	struct ap_map_module * mod,
	const char * file_path,
	boolean decrypt);

struct ap_map_region_template * ap_map_get_region_template(
	struct ap_map_module * mod,
	uint32_t region_id);

struct ap_map_region_template * ap_map_get_region_template_by_name(
	struct ap_map_module * mod,
	const char * region_name);

END_DECLS

#endif /* _AP_MAP_H_ */
