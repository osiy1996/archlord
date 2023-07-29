#ifndef _AR_TERRAIN_INTERNAL_H_
#define _AR_TERRAIN_INTERNAL_H_

#include <core/macros.h>
#include <core/types.h>

#include <al_public/ap_callback.h>
#include <al_public/ap_sector.h>

#include <al_runtime/ar_terrain.h>
#include <al_runtime/ar_mesh.h>

#include <vendor/bgfx/c99/bgfx.h>
#include <vendor/cglm/cglm.h>

BEGIN_DECLS

enum task_state {
	TASK_STATE_IDLE,
	TASK_STATE_LOAD_SECTOR,
	TASK_STATE_PRE_DRAW_BUFFER,
	TASK_STATE_DRAW_BUFFER,
	TASK_STATE_FLUSH,
};

struct packed_file {
	char name[32];
	void * data;
	uint32_t size;
	uint32_t decompressed_size;
};

struct pack {
	char path[512];
	struct packed_file files[256];
};

struct draw_batch {
	uint32_t first_index;
	uint32_t index_count;
	struct ar_mesh_material material;
};

struct draw_buffer {
	uint32_t vertex_count;
	struct ar_mesh_vertex * vertices;
	uint32_t index_count;
	uint32_t * indices;
	struct draw_batch * batch;
	bgfx_vertex_buffer_handle_t vertex_buffer;
	bgfx_index_buffer_handle_t index_buffer;
};

struct sector_load_data {
	uint32_t x;
	uint32_t z;
	size_t work_size;
	void * work_mem;
	struct ar_trn_segment_info * segment_info;
	struct ar_mesh_geometry * geometry;
	struct sector_load_data * next;
};

struct draw_construct_data {
	struct draw_buffer buf;
	struct ar_trn_sector ** sector_list;
	uint32_t * vertex_offsets;
};

struct terrain_ctx {
	float view_distance;
	vec2 last_sync_pos;
	struct ar_trn_sector sectors[AP_SECTOR_WORLD_INDEX_WIDTH][AP_SECTOR_WORLD_INDEX_HEIGHT];
	struct ap_scr_index * visible_sectors;
	struct ap_scr_index * visible_sectors_tmp;
	struct ap_scr_index * pending_unload_sectors;
	struct sector_load_data * load_data_freelist;
	struct draw_construct_data draw_construct_data;
	struct task_descriptor * task_freelist;
	struct draw_buffer draw_buf;
	bgfx_texture_handle_t null_tex;
	bgfx_uniform_handle_t sampler[5];
	bgfx_program_handle_t program;
	uint32_t ongoing_load_task_count;
	enum task_state task_state;
	uint32_t update_draw_buffer_in;
	uint32_t save_task_count;
	uint32_t completed_save_task_count;
	ap_cb_context cb_context;
};

struct terrain_ctx * get_ctx();

struct task_descriptor * alloc_task(struct terrain_ctx * ctx);

void free_task(
	struct terrain_ctx * ctx,
	struct task_descriptor * task);

struct pack * create_pack();

boolean unpack_terrain_file(
	const char * pathfmt,
	boolean decompress,
	struct pack ** pack,
	...);

boolean pack_terrain_file(
	const char * pathfmt,
	const struct pack * pack,
	...);

void destroy_pack(struct pack * pack);

boolean decompress_packed_file(
	struct packed_file * file,
	void ** work_mem,
	size_t * work_size);

inline uint32_t sector_map_offset(uint32_t index)
{
	return (index - (index / 16) * 16);
}

void create_sector_load_task(
	struct terrain_ctx * ctx,
	const struct ap_scr_index * index);

void create_sector_save_tasks(struct terrain_ctx * ctx);

static inline struct ar_trn_segment * get_segment(
	struct ar_trn_segment * s,
	uint32_t x,
	uint32_t z)
{
	return &s[x + z * 16];
}

END_DECLS

#endif /* _AR_TERRAIN_INTERNAL_H_ */