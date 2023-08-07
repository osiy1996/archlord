#ifndef _AC_TERRAIN_H_
#define _AC_TERRAIN_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module.h"

#include "utility/au_math.h"

#include "vendor/cglm/cglm.h"
#include "vendor/bgfx/c99/bgfx.h"

#define AC_TERRAIN_MODULE_NAME "AgcmTerrain"

BEGIN_DECLS

struct ac_mesh_geometry;
struct ac_mesh_material;
struct ac_camera;

enum ac_terrain_sector_flag_bits {
	AC_TERRAIN_SECTOR_NONE = 0,
	AC_TERRAIN_SECTOR_SEGMENT_IS_LOADED = 1u << 0,
	AC_TERRAIN_SECTOR_ROUGH_IS_LOADED = 1u << 1,
	AC_TERRAIN_SECTOR_DETAIL_IS_LOADED = 1u << 2,
	/* When editor makes changes to sector segments, 
	 * it will set this flag to retain sector state until 
	 * changes are commited and this flag is removed. */
	AC_TERRAIN_SECTOR_HAS_SEGMENT_CHANGES = 1u << 3,
	/* When editor makes changes to sector geometry, 
	 * it will set this flag to retain sector state until 
	 * changes are commited and this flag is removed. */
	AC_TERRAIN_SECTOR_HAS_DETAIL_CHANGES = 1u << 4,
};

enum ac_terrain_tile_type{
	AC_TERRAIN_TT_SOIL,
	AC_TERRAIN_TT_SWAMP,
	AC_TERRAIN_TT_GRASS,
	AC_TERRAIN_TT_SAND,
	AC_TERRAIN_TT_LEAF,
	AC_TERRAIN_TT_SNOW,
	AC_TERRAIN_TT_WATER,
	AC_TERRAIN_TT_STONE,
	AC_TERRAIN_TT_WOOD,
	AC_TERRAIN_TT_METAL,
	AC_TERRAIN_TT_BONE,
	AC_TERRAIN_TT_MUD,
	AC_TERRAIN_TT_SOILGRASS,
	AC_TERRAIN_TT_SOLIDSOIL,
	AC_TERRAIN_TT_SPORE,
	AC_TERRAIN_TT_MOSS,
	AC_TERRAIN_TT_GRANITE,
	AC_TERRAIN_TT_COUNT
};

enum ac_terrain_tile_block {
	AC_TERRAIN_TB_NONE = 0,
	AC_TERRAIN_TB_GEOMETRY = 1u << 0,
	AC_TERRAIN_TB_OBJECT = 1u << 1,
	AC_TERRAIN_TB_SKY = 1u << 2,
};

enum ac_terrain_geometry_block {
	AC_TERRAIN_GB_NONE = 0,
	AC_TERRAIN_GB_GROUND = 1u << 0,
	AC_TERRAIN_GB_SKY = 1u << 1,
};

enum ac_terrain_callback_id {
	AC_TERRAIN_CB_POST_LOAD_SECTOR,
	AC_TERRAIN_CB_SEGMENT_MODIFICATION,
};

struct ac_terrain_tile_info {
	uint8_t is_edge_turn : 1;
	uint8_t tile_type : 7;
	uint8_t geometry_block : 4;
	uint8_t has_no_layer : 1;
	uint8_t reserved : 3;
};

struct ac_terrain_segment {
	struct ac_terrain_tile_info tile;
	uint16_t region_id;
};

struct ac_terrain_line_block {
	vec3 start;
	vec3 end;
	uint32_t serial_number;
};

struct ac_terrain_segment_info {
	uint32_t flags;
	struct ac_terrain_segment segments[16][16];
	struct ac_terrain_line_block * line_blocks;
	uint32_t line_block_count;
};

struct ac_terrain_sector {
	uint32_t index_x;
	uint32_t index_z;
	vec2 extent_start;
	vec2 extent_end;
	uint32_t flags;
	struct ac_terrain_segment_info * segment_info;
	struct ac_mesh_geometry * rough_geometry;
	struct ac_mesh_geometry * geometry;
};

/* Callback data for AC_TERRAIN_CB_POST_LOAD_SECTOR. */
struct ac_terrain_cb_post_load_sector {
	vec2 sync_pos;
	float view_distance;
	struct ac_terrain_sector ** sectors;
	uint32_t sector_count;
};

/* Callback data for AC_TERRAIN_CB_SEGMENT_MODIFICATION. */
struct ac_terrain_cb_segment_modification {
	struct ac_terrain_sector ** visible_sectors;
	uint32_t visible_sector_count;
};

struct ac_terrain_cb_custom_render {
	uint64_t state;
	bgfx_program_handle_t program;
};

struct ac_terrain_module * ac_terrain_create_module();

void ac_terrain_add_callback(
	struct ac_terrain_module * mod,
	enum ac_terrain_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

struct ac_terrain_sector * ac_terrain_get_sector(
	struct ac_terrain_module * mod, 
	uint32_t x, 
	uint32_t z);

struct ac_terrain_sector * ac_terrain_get_sector_at(
	struct ac_terrain_module * mod, 
	const vec3 p);

struct ac_terrain_segment ac_terrain_get_segment(
	struct ac_terrain_module * mod, 
	const struct au_pos * position);

void ac_terrain_set_view_distance(
	struct ac_terrain_module * mod, 
	float view_distance);

float ac_terrain_get_view_distance(struct ac_terrain_module * mod);

void ac_terrain_sync(
	struct ac_terrain_module * mod, 
	const float * pos, 
	boolean force);

void ac_terrain_update(struct ac_terrain_module * mod, float dt);

void ac_terrain_render(struct ac_terrain_module * mod);

void ac_terrain_custom_render(
	struct ac_terrain_module * mod,
	ap_module_t callback_module,
	ap_module_default_t callback);

void ac_terrain_set_tex(
	struct ac_terrain_module * mod, 
	uint8_t stage, 
	bgfx_texture_handle_t tex);

boolean ac_terrain_raycast(
	struct ac_terrain_module * mod,
	const float * origin,
	const float * dir,
	float * hit_point);

boolean ac_terrain_get_quad(
	struct ac_terrain_module * mod,
	const vec3 start,
	float dimension,
	struct ac_mesh_vertex * vertices,
	struct ac_mesh_material * materials);

boolean ac_terrain_get_triangle(
	struct ac_terrain_module * mod,
	const vec3 pos,
	struct ac_mesh_vertex * vertices,
	struct ac_mesh_material * material);

boolean ac_terrain_set_triangle(
	struct ac_terrain_module * mod,
	uint32_t triangle_count,
	const struct ac_mesh_vertex * vertices,
	const struct ac_mesh_material * materials);

boolean ac_terrain_set_base(
	struct ac_terrain_module * mod,
	uint32_t triangle_count,
	uint32_t tile_ratio,
	const struct ac_mesh_vertex * vertices,
	const struct ac_mesh_material * materials);

boolean ac_terrain_set_layer(
	struct ac_terrain_module * mod,
	uint32_t layer,
	uint32_t triangle_count,
	uint32_t tile_ratio,
	const struct ac_mesh_vertex * vertices,
	const struct ac_mesh_material * materials);

void ac_terrain_adjust_height(
	struct ac_terrain_module * mod,
	vec3 pos,
	float radius,
	float d,
	enum interp_type interp_type);

void ac_terrain_level(
	struct ac_terrain_module * mod,
	vec3 pos,
	float radius,
	float d,
	enum interp_type interp_type);

void ac_terrain_set_region_id(
	struct ac_terrain_module * mod,
	vec3 pos,
	uint32_t size,
	uint32_t region_id);

void ac_terrain_set_tile_info(
	struct ac_terrain_module * mod,
	vec3 pos,
	uint32_t size,
	enum ac_terrain_tile_type tile_type,
	uint8_t geometry_block);

void ac_terrain_commit_changes(struct ac_terrain_module * mod);

END_DECLS

#endif /* _AC_TERRAIN_H_ */