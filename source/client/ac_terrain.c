#include "client/ac_terrain.h"

#include "core/bin_stream.h"
#include "core/core.h"
#include "core/file_system.h"
#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "task/task.h"

#include "public/ap_config.h"
#include "public/ap_sector.h"

#include "client/ac_mesh.h"
#include "client/ac_render.h"
#include "client/ac_renderware.h"
#include "client/ac_texture.h"

#include "vendor/cglm/cglm.h"
#include "vendor/aplib/aplib.h"

#include <assert.h>
#include <string.h>

#define MAKE_VB_INVALID(h) \
	(h) = (bgfx_vertex_buffer_handle_t)BGFX_INVALID_HANDLE;
#define MAKE_IB_INVALID(h) \
	(h) = (bgfx_index_buffer_handle_t)BGFX_INVALID_HANDLE;

#define ALEF_COMPACT_FILE_VERSION	0x1000
#define ALEF_COMPACT_FILE_VERSION2	0x1001
#define ALEF_COMPACT_FILE_VERSION3	0x1002
#define ALEF_COMPACT_FILE_VERSION4	0x1003

struct sector_save_data {
	struct ac_terrain_module * mod;
	uint32_t x;
	uint32_t z;
	struct ac_mesh_geometry * geometry;
	boolean geometry_save_result;
	struct ac_terrain_segment_info * segment_info;
	boolean segment_save_result;
	struct sector_save_data * head;
};

struct normal_triangle {
	struct ac_mesh_triangle * t;
	struct ac_mesh_geometry * g;
	uint32_t gindex;
};

struct normal_vertex {
	struct ac_mesh_vertex * v;
	struct ac_mesh_geometry * g;
	uint32_t gindex;
};

struct normal_set {
	struct ac_mesh_triangle * t;
	struct ac_mesh_geometry * g;
	uint32_t gindex;
};

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
	struct ac_mesh_material material;
};

struct draw_buffer {
	uint32_t vertex_count;
	struct ac_mesh_vertex * vertices;
	uint32_t index_count;
	uint32_t * indices;
	struct draw_batch * batch;
	bgfx_vertex_buffer_handle_t vertex_buffer;
	bgfx_index_buffer_handle_t index_buffer;
};

struct sector_load_data {
	struct ac_terrain_module * mod;
	uint32_t x;
	uint32_t z;
	size_t work_size;
	void * work_mem;
	struct ac_terrain_segment_info * segment_info;
	struct ac_mesh_geometry * geometry;
	struct sector_load_data * next;
};

struct draw_construct_data {
	struct ac_terrain_module * mod;
	struct draw_buffer buf;
	struct ac_terrain_sector ** sector_list;
	uint32_t * vertex_offsets;
};

struct ac_terrain_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	ap_module_t ac_mesh;
	ap_module_t ac_texture;
	float view_distance;
	vec2 last_sync_pos;
	struct ac_terrain_sector sectors[AP_SECTOR_WORLD_INDEX_WIDTH][AP_SECTOR_WORLD_INDEX_HEIGHT];
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
};

inline uint32_t sector_map_offset(uint32_t index)
{
	return (index - (index / 16) * 16);
}

static inline struct ac_terrain_segment * get_segment(
	struct ac_terrain_segment * s,
	uint32_t x,
	uint32_t z)
{
	return &s[x + z * 16];
}

struct task_descriptor * alloc_task(struct ac_terrain_module * mod)
{
	struct task_descriptor * t;
	if (!mod->task_freelist) {
		t = alloc(sizeof(*t));
		memset(t, 0, sizeof(*t));
		return t;
	}
	t = mod->task_freelist;
	mod->task_freelist = t->next;
	t->next = NULL;
	return t;
}

void free_task(
	struct ac_terrain_module * mod,
	struct task_descriptor * task)
{
	task->next = mod->task_freelist;
	mod->task_freelist = task;
}

struct pack * create_pack()
{
	struct pack * p;
	p = alloc(sizeof(*p));
	memset(p, 0, sizeof(*p));
	return p;
}

void destroy_pack(struct pack * pack)
{
	uint32_t i;
	for (i = 0; i < COUNT_OF(pack->files); i++)
		dealloc(pack->files[i].data);
	dealloc(pack);
}

boolean unpack_terrain_file(
	const char * pathfmt,
	boolean decompress,
	struct pack ** pack,
	...)
{
	struct bin_stream * stream;
	char path[512];
	const char * header = "MagPack Ver 0.1a";
	char header_in_file[50] = "";
	va_list ap;
	size_t size;
	uint32_t packed_size[256];
	void * work_mem;
	struct pack * p;
	uint32_t i;
	memset(packed_size, 0, sizeof(packed_size));
	va_start(ap, pack);
	make_pathv(path, sizeof(path), pathfmt, ap);
	va_end(ap);
	if (!get_file_size(path, &size))
		return FALSE;
	if (!bstream_from_file(path, &stream))
		return FALSE;
	if (!bstream_read(stream, header_in_file, 50) ||
		strcmp(header_in_file, header) != 0) {
		bstream_destroy(stream);
		return FALSE;
	}
	p = create_pack();
	for (i = 0; i < 256; i++) {
		struct packed_file * s = &p->files[i];
		uint8_t name_len = 0;
		if (!bstream_read_u8(stream, &name_len) ||
			name_len >= COUNT_OF(s->name) ||
			!bstream_read(stream, s->name, name_len) ||
			!bstream_read_u32(stream, &packed_size[i])) {
			destroy_pack(p);
			bstream_destroy(stream);
			return FALSE;
		}
		s->name[name_len] = '\0';
		packed_size[i] ^= 0x00006969;
	}
	/* We skip a byte here, client loads until it comes 
	 * across a sector with '0' as its name_len. */
	bstream_advance(stream, 1);
	work_mem = alloc((size_t)1u << 24);
	for (i = 0; i < 256; i++) {
		struct packed_file * s = &p->files[i];
		if (!bstream_read_u32(stream, &s->decompressed_size)) {
			dealloc(work_mem);
			destroy_pack(p);
			bstream_destroy(stream);
			return FALSE;
		}
		s->decompressed_size ^= 0x00006969;
		if (decompress) {
			s->size = aP_depack_asm(
				(unsigned char *)stream->cursor,
				(unsigned char *)work_mem);
			if (s->size != s->decompressed_size) {
				dealloc(work_mem);
				destroy_pack(p);
				bstream_destroy(stream);
				return FALSE;
			}
			s->data = alloc(s->decompressed_size);
			memcpy(s->data, work_mem, s->size);
			if (!bstream_advance(stream, packed_size[i])) {
				dealloc(work_mem);
				destroy_pack(p);
				bstream_destroy(stream);
				return FALSE;
			}
		}
		else {
			s->data = alloc(packed_size[i]);
			s->size = packed_size[i];
			if (!bstream_read(stream, s->data, s->size)) {
				dealloc(work_mem);
				destroy_pack(p);
				bstream_destroy(stream);
				return FALSE;
			}
		}
	}
	dealloc(work_mem);
	bstream_destroy(stream);
	*pack = p;
	return TRUE;
}

boolean pack_terrain_file(
	const char * pathfmt,
	const struct pack * pack,
	...)
{
	struct bin_stream * stream;
	char path[512];
	const char * header = "MagPack Ver 0.1a";
	va_list ap;
	uint32_t i;
	va_start(ap, pack);
	make_pathv(path, sizeof(path), pathfmt, ap);
	strlcat(path, ".npack", sizeof(path));
	va_end(ap);
	bstream_for_write(NULL, (size_t)1u << 25, &stream);
	if (!bstream_write_str(stream, header, 50)) {
		bstream_destroy(stream);
		return FALSE;
	}
	for (i = 0; i < 256; i++) {
		const struct packed_file * s = &pack->files[i];
		uint32_t len = (uint32_t)strlen(s->name);
		if (!bstream_write_u8(stream, (uint8_t)len) ||
			!bstream_write_str(stream, s->name, len) ||
			!bstream_write_u32(stream, s->size)) {
			bstream_destroy(stream);
			return FALSE;
		}
	}
	/* Empty byte to indicate that we are 
	 * done writing headers. */
	if (!bstream_fill(stream, 0, 1)) {
		bstream_destroy(stream);
		return FALSE;
	}
	for (i = 0; i < 256; i++) {
		const struct packed_file * s = &pack->files[i];
		if (!bstream_write_u32(stream, s->size)) {
			bstream_destroy(stream);
			return FALSE;
		}
		if (!bstream_write(stream, s->data, s->size)) {
			bstream_destroy(stream);
			return FALSE;
		}
	}
	if (!make_file(path, stream->head, bstream_offset(stream))) {
		bstream_destroy(stream);
		return FALSE;
	}
	bstream_destroy(stream);
	return TRUE;
}

boolean decompress_packed_file(
	struct packed_file * file,
	void ** work_mem,
	size_t * work_size)
{
	uint32_t sz;
	void * wm = *work_mem;
	if (*work_size < file->decompressed_size) {
		*work_size = file->decompressed_size;
		wm = reallocate(wm, *work_size);
		*work_mem = wm;
	}
	sz = aP_depack_asm(
		(unsigned char *)file->data,
		(unsigned char *)wm);
	if (sz != file->decompressed_size)
		return FALSE;
	file->size = sz;
	file->data = reallocate(file->data, file->decompressed_size);
	memcpy(file->data, wm, file->decompressed_size);
	return TRUE;
}

static struct sector_load_data * alloc_load_data(
	struct ac_terrain_module * mod)
{
	struct sector_load_data * d;
	if (!mod->task_freelist) {
		d = alloc(sizeof(*d));
		memset(d, 0, sizeof(*d));
		return d;
	}
	d = mod->load_data_freelist;
	mod->load_data_freelist = d->next;
	d->segment_info = NULL;
	return d;
}

static void free_load_data(
	struct ac_terrain_module * mod,
	struct sector_load_data * data)
{
	data->next = mod->load_data_freelist;
	mod->load_data_freelist = data;
}

static boolean read_segment_flags(
	struct bin_stream * stream,
	uint32_t * flags)
{
	if (!bstream_read_u32(stream, flags)) {
		ERROR("Failed to read compact segment flags.");
		return FALSE;
	}
	return TRUE;
}

static boolean read_segment_depth(
	struct bin_stream * stream,
	uint32_t * depth)
{
	if (!bstream_read_u32(stream, depth)) {
		ERROR("Failed to read compact segment depth.");
		return FALSE;
	}
	if (*depth != 16) {
		ERROR("Invalid compact segment depth (%u).", *depth);
		return FALSE;
	}
	return TRUE;
}

static boolean segments_from_stream(
	struct sector_load_data * d,
	struct bin_stream * stream)
{
	struct ac_terrain_segment_info * sinfo = d->segment_info;
	uint32_t version;
	uint32_t depth;
	uint32_t count;
	uint32_t x;
	uint32_t z;
	if (!bstream_read_u32(stream, &version)) {
		ERROR("Failed to read compact segment file version.");
		return FALSE;
	}
	switch (version) {
	case ALEF_COMPACT_FILE_VERSION2:
		if (!read_segment_flags(stream, &sinfo->flags))
			return FALSE;
	case ALEF_COMPACT_FILE_VERSION:
		if (!read_segment_depth(stream, &depth))
			return FALSE;
		for (z = 0; z < 16; z++) {
			for (x = 0; x < 16; x++) {
				struct ac_terrain_segment * s = &sinfo->segments[x][z];
				uint16_t u;
				if (!bstream_advance(stream, 2)) {
					ERROR("Stream ended unexpectedly.");
					return FALSE;
				}
				if (!bstream_read(stream, &s->tile,
						sizeof(s->tile))) {
					ERROR("Failed to read segment tile info.");
					return FALSE;
				}
				if (!bstream_read_u16(stream, &u)) {
					ERROR("Failed to read segment region id.");
					return FALSE;
				}
				s->region_id = u & 0x00FF;
			}
		}
		break;
	case ALEF_COMPACT_FILE_VERSION3:
	case ALEF_COMPACT_FILE_VERSION4: {
		struct ac_terrain_segment buf[256];
		uint32_t x;
		if (!read_segment_flags(stream, &sinfo->flags))
			return FALSE;
		if (!read_segment_depth(stream, &depth))
			return FALSE;
		if (!bstream_read(stream, buf, sizeof(buf))) {
			ERROR("Failed to read compact segment data.");
			return FALSE;
		}
		/* Flip segments. */
		for (x = 0; x < 16; x++) {
			uint32_t z;
			for (z = 0; z < 16; z++)
				sinfo->segments[x][z] = buf[z * 16 + x];
		}
		if (bstream_remain(stream) < 4)
			break;
		if (!bstream_read_u32(stream, &count)) {
			ERROR("Failed to read line block count.");
			return FALSE;
		}
		if (!count)
			break;
		sinfo->line_blocks = alloc(
			count * sizeof(*sinfo->line_blocks));
		if (!bstream_read(stream, sinfo->line_blocks,
				count * sizeof(*sinfo->line_blocks))) {
			ERROR("Failed to read line block data.");
			dealloc(sinfo->line_blocks);
			return FALSE;
		}
		sinfo->line_block_count = count;
		break;
	}
	}
	if (version == ALEF_COMPACT_FILE_VERSION3) {
		for (x = 0; x < 16; x++) {
			for (z = 0; z < 16; z++) {
				sinfo->segments[x][z].tile.has_no_layer = FALSE;
				sinfo->segments[x][z].tile.reserved = 0;
			}
		}
	}
	return TRUE;
}

static boolean load_segments(struct sector_load_data * d)
{
	struct pack * pack;
	uint32_t file_idx;
	struct bin_stream * stream;
	if (!unpack_terrain_file("%s/world/a00%02u%02ux.ma1",
			FALSE, &pack, ap_config_get(d->mod->ap_config, "ClientDir"),
			d->x / 16, d->z / 16)) {
		/* This sector may be empty. */
		return FALSE;
	}
	file_idx = sector_map_offset(d->z) * 16 + 
		sector_map_offset(d->x);
	if (!decompress_packed_file(&pack->files[file_idx],
			&d->work_mem, &d->work_size)) {
		WARN("Failed to decompress sector.");
		destroy_pack(pack);
		return FALSE;
	}
	bstream_from_buffer(pack->files[file_idx].data,
		(size_t)pack->files[file_idx].size,
		FALSE, &stream);
	d->segment_info = alloc(sizeof(*d->segment_info));
	memset(d->segment_info, 0, sizeof(*d->segment_info));
	if (!segments_from_stream(d, stream)) {
		dealloc(d->segment_info);
		d->segment_info = NULL;
		bstream_destroy(stream);
		destroy_pack(pack);
		return FALSE;
	}
	bstream_destroy(stream);
	destroy_pack(pack);
	return TRUE;
}

static boolean sector_load_task(void * data)
{
	struct sector_load_data * d = data;
	struct pack * pack;
	uint32_t file_idx;
	struct bin_stream * stream;
	boolean exists = FALSE;
	d->geometry = NULL;
	load_segments(d);
	if (!unpack_terrain_file("%s/world/b00%02u%02ux.ma2",
			FALSE, &pack, ap_config_get(d->mod->ap_config, "ClientDir"),
			d->x / 16, d->z / 16)) {
		/* This sector may be empty. */
		return FALSE;
	}
	file_idx = sector_map_offset(d->z) * 16 + 
		sector_map_offset(d->x);
	if (!decompress_packed_file(&pack->files[file_idx],
			&d->work_mem, &d->work_size)) {
		WARN("Failed to decompress sector.");
		destroy_pack(pack);
		return FALSE;
	}
	bstream_from_buffer(pack->files[file_idx].data,
		(size_t)pack->files[file_idx].size,
		FALSE, &stream);
	if (!bstream_read_i32(stream, &exists)) {
		ERROR("Stream ended unexpectedly.");
		bstream_destroy(stream);
		destroy_pack(pack);
		return FALSE;
	}
	if (!exists) {
		bstream_destroy(stream);
		destroy_pack(pack);
		return FALSE;
	}
	if (!ac_renderware_find_chunk(stream, rwID_ATOMIC, NULL)) {
		ERROR("Failed to find rwID_ATOMIC.");
		bstream_destroy(stream);
		destroy_pack(pack);
		return FALSE;
	}
	ac_texture_set_dictionary(d->mod->ac_texture, AC_DAT_DIR_TEX_WORLD);
	d->geometry = ac_mesh_read_rp_atomic(d->mod->ac_mesh, stream, NULL, 0);
	bstream_destroy(stream);
	destroy_pack(pack);
	if (!d->geometry) {
		ERROR("Failed to read RpAtomic.");
		return FALSE;
	}
	return TRUE;
}

static void sector_load_post_task(
	struct task_descriptor * task,
	void * data,
	boolean result)
{
	struct sector_load_data * d = data;
	struct ac_terrain_module * mod = d->mod;
	struct ac_terrain_sector * s = &mod->sectors[d->x][d->z];
	struct ap_scr_index idx = { d->x, d->z };
	assert(mod->ongoing_load_task_count != 0);
	if (!--mod->ongoing_load_task_count)
		mod->task_state = TASK_STATE_PRE_DRAW_BUFFER;
	free_task(mod, task);
	if (d->segment_info) {
		s->segment_info = d->segment_info;
		s->flags |= AC_TERRAIN_SECTOR_SEGMENT_IS_LOADED;
	}
	if (d->geometry) {
		if (!ap_scr_find_index(mod->visible_sectors, &idx)) {
			ac_mesh_destroy_geometry(mod->ac_mesh, d->geometry);
			free_load_data(mod, d);
			return;
		}
		s->geometry = d->geometry;
		s->flags |= AC_TERRAIN_SECTOR_DETAIL_IS_LOADED;
	}
	free_load_data(mod, d);
}

static boolean save_sector_geometry(
	struct ac_terrain_module * mod,
	uint32_t x,
	uint32_t z,
	struct ac_mesh_geometry * g)
{
	struct bin_stream * stream;
	char path[512];
	bstream_for_write(NULL, (size_t)1u << 20, &stream);
	if (!bstream_write_i32(stream, 1)) {
		ERROR("Stream ended unexpectedly.");
		bstream_destroy(stream);
		return FALSE;
	}
	if (!ac_mesh_write_rp_atomic(stream, g)) {
		ERROR("Failed to write atomic.");
		bstream_destroy(stream);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/world/D%03u,%03u.dff",
			ap_config_get(mod->ap_config, "ClientDir"), x, z)) {
		ERROR("Failed to create path.");
		bstream_destroy(stream);
		return FALSE;
	}
	if (!make_file(path, stream->head, bstream_offset(stream))) {
		ERROR("Failed to create output file.");
		bstream_destroy(stream);
		return FALSE;
	}
	bstream_destroy(stream);
	return TRUE;
}

static boolean save_sector_segments(
	struct ac_terrain_module * mod,
	uint32_t x,
	uint32_t z,
	struct ac_terrain_segment_info * si)
{
	struct bin_stream * stream;
	char path[512];
	uint32_t sz;
	bstream_for_write(NULL, (size_t)1u << 20, &stream);
	if (!bstream_write_u32(stream, ALEF_COMPACT_FILE_VERSION4)) {
		ERROR("Failed to write file version.");
		bstream_destroy(stream);
		return FALSE;
	}
	if (!bstream_write_u32(stream, si->flags)) {
		ERROR("Failed to write segment info flags.");
		bstream_destroy(stream);
		return FALSE;
	}
	if (!bstream_write_u32(stream, 16)) {
		ERROR("Failed to write segment depth.");
		bstream_destroy(stream);
		return FALSE;
	}
	for (sz = 0; sz < 16; sz++) {
		uint32_t sx;
		for (sx = 0; sx < 16; sx++) {
			if (!bstream_write(stream, &si->segments[sx][sz],
					sizeof(si->segments[sx][sz]))) {
				ERROR("Failed to write segment data.");
				bstream_destroy(stream);
				return FALSE;
			}
		}
	}
	if (!bstream_write_u32(stream, si->line_block_count)) {
		ERROR("Failed to write line block count.");
		bstream_destroy(stream);
		return FALSE;
	}
	if (!bstream_write(stream, si->line_blocks,
			si->line_block_count * sizeof(*si->line_blocks))) {
		ERROR("Failed to write line block data.");
		bstream_destroy(stream);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/world/C%03u,%03u.amf",
			ap_config_get(mod->ap_config, "ClientDir"), x, z)) {
		ERROR("Failed to create path.");
		bstream_destroy(stream);
		return FALSE;
	}
	if (!make_file(path, stream->head, bstream_offset(stream))) {
		ERROR("Failed to create output file.");
		bstream_destroy(stream);
		return FALSE;
	}
	bstream_destroy(stream);
	return TRUE;
}

static boolean sector_save_task(void * data)
{
	struct sector_save_data * d = data;
	if (d->geometry) {
		d->geometry_save_result = save_sector_geometry(d->mod,
			d->x, d->z, d->geometry);
	}
	if (d->segment_info) {
		d->segment_save_result = save_sector_segments(d->mod,
			d->x, d->z, d->segment_info);
	}
	return TRUE;
}

static void sector_save_post_task(
	struct task_descriptor * task,
	void * data,
	boolean result)
{
	struct sector_save_data * d = data;
	struct ac_terrain_module * mod = d->mod;
	struct ac_terrain_sector * s;
	mod->completed_save_task_count++;
	s = &mod->sectors[d->x][d->z];
	if (!d->geometry) {
	}
	else if (d->geometry_save_result) {
		s->flags &= ~AC_TERRAIN_SECTOR_HAS_DETAIL_CHANGES;
		INFO("(%u/%u) Saved file D%03u%03u.dff.",
			mod->completed_save_task_count, mod->save_task_count,
			d->x, d->z);
	}
	else {
		ERROR("(%u/%u) Failed to save file D%03u%03u.dff.",
			mod->completed_save_task_count, mod->save_task_count,
			d->x, d->z);
	}
	if (!d->segment_info) {
	}
	else if (d->segment_save_result) {
		s->flags &= ~AC_TERRAIN_SECTOR_HAS_SEGMENT_CHANGES;
		INFO("(%u/%u) Saved file C%03u%03u.amf.",
			mod->completed_save_task_count, mod->save_task_count,
			d->x, d->z);
	}
	else {
		ERROR("(%u/%u) Failed to save file C%03u%03u.amf.",
			mod->completed_save_task_count, mod->save_task_count,
			d->x, d->z);
	}
	if (mod->completed_save_task_count == mod->save_task_count) {
		vec_free(d->head);
		/* TODO: Execute mappack to pack saved files. */
		mod->save_task_count = 0;
		mod->completed_save_task_count = 0;
	}
}

void create_sector_load_task(
	struct ac_terrain_module * mod,
	const struct ap_scr_index * index)
{
	struct task_descriptor * t = alloc_task(mod);
	struct sector_load_data * data = alloc_load_data(mod);
	data->mod = mod;
	data->x = index->x;
	data->z = index->z;
	t->data = data;
	t->work_cb = sector_load_task;
	t->post_cb = sector_load_post_task;
	task_add(t, TRUE);
	mod->ongoing_load_task_count++;
	mod->task_state = TASK_STATE_LOAD_SECTOR;
}

void create_sector_save_tasks(struct ac_terrain_module * mod)
{
	struct sector_save_data * data;
	uint32_t x;
	uint32_t i;
	if (mod->save_task_count)
		return;
	data = vec_new_reserved(sizeof(*data), 128);
	for (x = 0; x < AP_SECTOR_WORLD_INDEX_WIDTH; x++) {
		uint32_t z;
		for (z = 0; z < AP_SECTOR_WORLD_INDEX_HEIGHT; z++) {
			struct ac_terrain_sector * s = &mod->sectors[x][z];
			struct sector_save_data d = { 0 };
			boolean skip = TRUE;
			if ((s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED) &&
				(s->flags & AC_TERRAIN_SECTOR_HAS_DETAIL_CHANGES)) {
				d.geometry = s->geometry;
				skip = FALSE;
			}
			if ((s->flags & AC_TERRAIN_SECTOR_SEGMENT_IS_LOADED) &&
				(s->flags & AC_TERRAIN_SECTOR_HAS_SEGMENT_CHANGES)) {
				d.segment_info = s->segment_info;
				skip = FALSE;
			}
			if (!skip) {
				d.mod = mod;
				d.x = x;
				d.z = z;
				vec_push_back(&data, &d);
			}
		}
	}
	if (vec_is_empty(data)) {
		vec_free(data);
		return;
	}
	mod->save_task_count = vec_count(data);
	mod->completed_save_task_count = 0;
	for (i = 0; i < mod->save_task_count; i++) {
		struct task_descriptor * t = alloc_task(mod);
		data[i].head = data;
		t->data = &data[i];
		t->work_cb = sector_save_task;
		t->post_cb = sector_save_post_task;
		task_add(t, TRUE);
	}
}

static void include_in_nvtx(
	struct normal_vertex ** buf,
	const struct normal_vertex * n)
{
	struct normal_vertex * tmp = *buf;
	uint32_t i;
	uint32_t count = vec_count(tmp);
	for (i = 0; i < count; i++) {
		if (tmp[i].v == n->v)
			return;
	}
	n->v->normal[0] = 0.0f;
	n->v->normal[1] = 0.0f;
	n->v->normal[2] = 0.0f;
	vec_push_back(buf, n);
}

static void include_in_ntri(
	struct normal_triangle ** buf,
	const struct normal_triangle * n)
{
	struct normal_triangle * tmp = *buf;
	uint32_t i;
	uint32_t count = vec_count(tmp);
	for (i = 0; i < count; i++) {
		if (tmp[i].t == n->t)
			return;
	}
	vec_push_back(buf, n);
}

static void calc_normals(
	struct draw_construct_data * dcd,
	struct normal_triangle * nbuf)
{
	uint32_t i;
	uint32_t count = vec_count(nbuf);
	struct normal_vertex * nvtx =
		vec_new_reserved(sizeof(*nvtx), 128);
	for (i = 0; i < count; i++) {
		struct ac_mesh_triangle * t = nbuf[i].t;
		struct ac_mesh_geometry * g = nbuf[i].g;
		struct ac_mesh_vertex * vtx[3] = {
			&g->vertices[t->indices[0]],
			&g->vertices[t->indices[1]],
			&g->vertices[t->indices[2]] };
		uint32_t j;
		for (j = 0; j < vec_count(dcd->sector_list); j++) {
			struct ac_terrain_sector * s = dcd->sector_list[j];
			struct ac_mesh_geometry * g = s->geometry;
			uint32_t k;
			if (!(s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED))
				continue;
			for (k = 0; k < g->vertex_count; k++) {
				uint32_t l;
				for (l = 0; l < 3; l++) {
					if (ac_mesh_eq_pos(vtx[l], &g->vertices[k])) {
						struct normal_vertex n = { 
							&g->vertices[k], g, j };
						include_in_nvtx(&nvtx, &n);
						break;
					}
				}
			}
		}
	}
	count = vec_count(nvtx);
	for (i = 0; i < count ;i++) {
		struct ac_mesh_vertex * v = nvtx[i].v;
		uint32_t j;
		for (j = 0; j < vec_count(dcd->sector_list); j++) {
			struct ac_terrain_sector * s = dcd->sector_list[j];
			struct ac_mesh_geometry * g = s->geometry;
			uint32_t k;
			if (!(s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED))
				continue;
			for (k = 0; k < g->triangle_count; k++) {
				struct ac_mesh_triangle * t = &g->triangles[k];
				struct ac_mesh_vertex * vtx[3] = {
					&g->vertices[t->indices[0]],
					&g->vertices[t->indices[1]],
					&g->vertices[t->indices[2]] };
				uint32_t l;
				for (l = 0; l < 3; l++) {
					if (ac_mesh_eq_pos(vtx[l], v)) {
						vec3 n;
						ac_mesh_calculate_surface_normal(g, t, n);
						v->normal[0] += n[0];
						v->normal[1] += n[1];
						v->normal[2] += n[2];
						break;
					}
				}
			}
		}
	}
	for (i = 0; i < count ;i++) {
		struct ac_mesh_vertex * v = nvtx[i].v;
		vec3 n = { v->normal[0], v->normal[1], v->normal[2] };
		glm_vec3_normalize(n);
		v->normal[0] = n[0];
		v->normal[1] = n[1];
		v->normal[2] = n[2];
	}
	vec_free(nvtx);
}

static struct ac_terrain_segment * get_segment_by_pos(
	struct ac_terrain_sector * s,
	float x,
	float z)
{
	int sx;
	int sz;
	assert(s != NULL);
	sx = (int)((x - s->extent_start[0]) / AP_SECTOR_STEPSIZE);
	sz = (int)((z - s->extent_start[1]) / AP_SECTOR_STEPSIZE);
	assert(sx >= 0 && sx < 16);
	assert(sz >= 0 && sz < 16);
	return &s->segment_info->segments[sx][sz];
}

static uint32_t snap_to_step_x(float x)
{
	assert(x >= AP_SECTOR_WORLD_START_X);
	return (uint32_t)(((x - AP_SECTOR_WORLD_START_X) + 
		(AP_SECTOR_STEPSIZE / 2.0f)) / AP_SECTOR_STEPSIZE);
}

static uint32_t snap_to_step_z(float z)
{
	assert(z >= AP_SECTOR_WORLD_START_Z);
	return (uint32_t)(((z - AP_SECTOR_WORLD_START_Z) + 
		(AP_SECTOR_STEPSIZE / 2.0f)) / AP_SECTOR_STEPSIZE);
}

static struct ac_terrain_sector * from_triangle(
	struct ac_terrain_module * mod,
	const struct ac_mesh_vertex * vertices)
{
	vec3 c = { 0.0f, 0.0f, 0.0f };
	uint32_t i;
	for (i = 0; i < 3; i++)
		c[0] += vertices[i].position[0];
	for (i = 0; i < 3; i++)
		c[2] += vertices[i].position[2];
	c[0] /= 3.0f;
	c[2] /= 3.0f;
	return ac_terrain_get_sector_at(mod, c);
}

static struct ac_terrain_sector * from_sector_index(
	struct ac_terrain_module * mod,
	const struct ap_scr_index * index)
{
	return &mod->sectors[index->x][index->z];
}

static void calc_visible_sectors(
	struct ac_terrain_module * mod,
	const float * pos)
{
	float x;
	float z;
	vec_clear(mod->visible_sectors);
	for (x = pos[0] - mod->view_distance;
		x < pos[0] + mod->view_distance;
		x += AP_SECTOR_WIDTH) {
		for (z = pos[2] - mod->view_distance;
			z < pos[2] + mod->view_distance;
			z += AP_SECTOR_HEIGHT) {
			uint32_t sx;
			uint32_t sz;
			float p[3] = { x, pos[1], z };
			struct ap_scr_index idx = { 0 };
			if (!ap_scr_pos_to_index(p, &sx, &sz))
				continue;
			if (ap_scr_distance(pos, sx, sz) > mod->view_distance)
				continue;
			idx.x = sx;
			idx.z = sz;
			vec_push_back(&mod->visible_sectors, &idx);
		}
	}
	ap_scr_sort_indices(mod->visible_sectors,
		vec_count(mod->visible_sectors));
}

static boolean load_all(struct ac_terrain_module * mod)
{
	uint32_t i, x, z;
	for (x = 17; x < 34; x++) {
		for (z = 17; z < 34; z++) {
			struct pack * pack = NULL;
			if (!unpack_terrain_file("%s/world/b00%02u%02ux.ma2",
				TRUE, &pack, ap_config_get(mod->ap_config, "ClientDir"), x, z)) {
				WARN("Failed to unpack (%u,%u).", x, z);
				continue;
			}
			ac_texture_set_dictionary(mod->ac_texture, AC_DAT_DIR_TEX_WORLD);
			for (i = 0; i < 256; i++) {
				struct bin_stream * stream;
				boolean exists = FALSE;
				bstream_from_buffer(pack->files[i].data,
					pack->files[i].size, FALSE, &stream);
				if (!bstream_read_i32(stream, &exists)) {
					ERROR("Stream ended unexpectedly.");
					return FALSE;
				}
				if (!exists) {
					bstream_destroy(stream);
					continue;
				}
				if (!ac_renderware_find_chunk(stream, rwID_ATOMIC, NULL)) {
					ERROR("Failed to find rwID_ATOMIC.");
					return FALSE;
				}
				if (!ac_mesh_read_rp_atomic(mod->ac_mesh, stream, NULL, 0)) {
					ERROR("Failed to read RpAtomic.");
					return FALSE;
				}
				bstream_destroy(stream);
			}
			destroy_pack(pack);
		}
	}
	return TRUE;
}

static boolean create_shaders(struct ac_terrain_module * mod)
{
	bgfx_shader_handle_t vsh;
	bgfx_shader_handle_t fsh;
	const bgfx_memory_t * mem;
	uint8_t null_data[256];
	if (!ac_render_load_shader("ac_terrain_main.vs", &vsh)) {
		ERROR("Failed to load vertex shader.");
		return FALSE;
	}
	if (!ac_render_load_shader("ac_terrain_main.fs", &fsh)) {
		ERROR("Failed to load fragment shader.");
		return FALSE;
	}
	mod->program = bgfx_create_program(vsh, fsh, true);
	if (!BGFX_HANDLE_IS_VALID(mod->program)) {
		ERROR("Failed to create program.");
		return FALSE;
	}
#define CREATE_SAMPLER(name, handle) {\
		mod->handle = bgfx_create_uniform(name, \
			BGFX_UNIFORM_TYPE_SAMPLER, 1);\
		if (!BGFX_HANDLE_IS_VALID(mod->handle)) {\
			ERROR("Failed to create sampler ("name").");\
			return FALSE;\
		}\
	}
	CREATE_SAMPLER("s_texBase", sampler[0]);
	CREATE_SAMPLER("s_texAlpha0", sampler[1]);
	CREATE_SAMPLER("s_texColor0", sampler[2]);
	CREATE_SAMPLER("s_texAlpha1", sampler[3]);
	CREATE_SAMPLER("s_texColor1", sampler[4]);
#undef CREATE_SAMPLER
	memset(null_data, 0, sizeof(null_data));
	mem = bgfx_copy(null_data, sizeof(null_data));
	mod->null_tex = bgfx_create_texture_2d(8, 8, false, 1,
		BGFX_TEXTURE_FORMAT_BGRA8, 0, mem);
	if (!BGFX_HANDLE_IS_VALID(mod->null_tex)) {
		ERROR("Failed to create null texture.");
		return FALSE;
	}
	return TRUE;
}

static void unload_sector(
	struct ac_terrain_module * mod, 
	struct ac_terrain_sector * s)
{
	if (!(s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED)) {
		assert(0);
		ERROR("Attempting to unload a sector that is not loaded.");
		return;
	}
	if (s->flags & AC_TERRAIN_SECTOR_HAS_DETAIL_CHANGES)
		WARN("Unloading a sector that has pending detail changes.");
	if (s->flags & AC_TERRAIN_SECTOR_HAS_SEGMENT_CHANGES)
		WARN("Unloading a sector that has pending segment changes.");
	ac_mesh_destroy_geometry(mod->ac_mesh, s->geometry);
	s->geometry = NULL;
	s->flags &= ~AC_TERRAIN_SECTOR_DETAIL_IS_LOADED;
}

static void add_to_batch(
	struct draw_buffer * buf,
	struct draw_batch * b,
	const struct ac_mesh_geometry * g,
	uint32_t vertex_offset)
{
	uint32_t i;
	for (i = 0; i < g->split_count; i++) {
		const struct ac_mesh_split * s = &g->splits[i];
		uint32_t j;
		if (ac_mesh_cmp_material(&b->material,
				&g->materials[s->material_index]) != 0)
			continue;
		for (j = 0; j < s->index_count; j++) {
			uint32_t idx = b->first_index + b->index_count + j;
			buf->indices[idx] = vertex_offset + 
				g->indices[s->index_offset + j];
		}
		b->index_count += j;
	}
}

static boolean recreate_vertex_buffer(struct draw_buffer *  buf)
{
	bgfx_vertex_layout_t layout = ac_mesh_vertex_layout();
	const bgfx_memory_t * mem;
	if (BGFX_HANDLE_IS_VALID(buf->vertex_buffer))
		bgfx_destroy_vertex_buffer(buf->vertex_buffer);
	mem = bgfx_copy(buf->vertices,
		buf->vertex_count * sizeof(*buf->vertices));
	buf->vertex_buffer = bgfx_create_vertex_buffer(mem, &layout,
		BGFX_BUFFER_NONE);
	if (!BGFX_HANDLE_IS_VALID(buf->vertex_buffer)) {
		ERROR("Failed to create vertex buffer.");
		return FALSE;
	}
	return TRUE;
}

static boolean create_buffers(struct draw_buffer * buf)
{
	bgfx_vertex_layout_t layout = ac_mesh_vertex_layout();
	const bgfx_memory_t * mem;
	mem = bgfx_copy(buf->vertices,
		buf->vertex_count * sizeof(*buf->vertices));
	buf->vertex_buffer = bgfx_create_vertex_buffer(mem, &layout,
		BGFX_BUFFER_NONE);
	if (!BGFX_HANDLE_IS_VALID(buf->vertex_buffer)) {
		ERROR("Failed to create vertex buffer.");
		return FALSE;
	}
	mem = bgfx_copy(buf->indices,
		buf->index_count * sizeof(*buf->indices));
	buf->index_buffer = bgfx_create_index_buffer(mem, 
		BGFX_BUFFER_INDEX32);
	if (!BGFX_HANDLE_IS_VALID(buf->index_buffer)) {
		ERROR("Failed to create index buffer.");
		return FALSE;
	}
	return TRUE;
}

static void init_draw_buf(struct draw_buffer * buf)
{
	buf->vertex_buffer =
		(bgfx_vertex_buffer_handle_t)BGFX_INVALID_HANDLE;
	buf->index_buffer =
		(bgfx_index_buffer_handle_t)BGFX_INVALID_HANDLE;
	buf->batch = vec_new_reserved(sizeof(*buf->batch), 128);
}

static void clear_draw_buffer(
	struct ac_terrain_module * mod, 
	struct draw_buffer * buf)
{
	uint32_t i;
	/* Free resources. */
	if (BGFX_HANDLE_IS_VALID(buf->vertex_buffer)) {
		bgfx_destroy_vertex_buffer(buf->vertex_buffer);
		buf->vertex_buffer =
			(bgfx_vertex_buffer_handle_t)BGFX_INVALID_HANDLE;
	}
	if (BGFX_HANDLE_IS_VALID(buf->index_buffer)) {
		bgfx_destroy_index_buffer(buf->index_buffer);
		buf->index_buffer =
			(bgfx_index_buffer_handle_t)BGFX_INVALID_HANDLE;
	}
	dealloc(buf->vertices);
	buf->vertices = NULL;
	dealloc(buf->indices);
	buf->indices = NULL;
	for (i = 0; i < vec_count(buf->batch); i++)
		ac_mesh_release_material(mod->ac_mesh, &buf->batch[i].material);
	vec_clear(buf->batch);
}

static void destroy_draw_buf(
	struct ac_terrain_module * mod, 
	struct draw_buffer * buf)
{
	clear_draw_buffer(mod, buf);
	vec_free(buf->batch);
}

static boolean construct_draw_buffer(
	struct draw_construct_data * data)
{
	uint32_t i;
	uint32_t vo_idx = 0;
	struct draw_buffer * buf = &data->buf;
	/* Construct combined buffers. */
	buf->vertex_count = 0;
	for (i = 0; i < vec_count(data->sector_list); i++) {
		buf->vertex_count +=
			data->sector_list[i]->geometry->vertex_count;
	}
	buf->vertices = alloc(
		buf->vertex_count * sizeof(*buf->vertices));
	buf->vertex_count = 0;
	vec_clear(data->vertex_offsets);
	for (i = 0; i < vec_count(data->sector_list); i++) {
		struct ac_mesh_geometry * g =
			data->sector_list[i]->geometry;
		memcpy(&buf->vertices[buf->vertex_count],
			g->vertices,
			g->vertex_count * sizeof(*buf->vertices));
		vec_push_back(&data->vertex_offsets,
			&buf->vertex_count);
		buf->vertex_count += g->vertex_count;
	}
	vec_clear(buf->batch);
	buf->index_count = 0;
	for (i = 0; i < vec_count(data->sector_list); i++) {
		const struct ac_mesh_geometry * g =
			data->sector_list[i]->geometry;
		uint32_t j;
		for (j = 0; j < g->split_count; j++) {
			const struct ac_mesh_split * sp = &g->splits[j];
			uint32_t k;
			struct draw_batch * b = NULL;
			buf->index_count += sp->index_count;
			for (k = 0; k < vec_count(buf->batch); k++) {
				if (ac_mesh_cmp_material(&buf->batch[k].material,
					&g->materials[sp->material_index]) == 0) {
					b = &buf->batch[k];
					break;
				}
			}
			if (!b) {
				struct draw_batch nb;
				memset(&nb, 0, sizeof(nb));
				ac_mesh_copy_material(data->mod->ac_mesh, &nb.material,
					&g->materials[sp->material_index]);
				vec_push_back(&buf->batch, &nb);
			}
		}
	}
	buf->indices = alloc(buf->index_count * sizeof(*buf->indices));
	buf->index_count = 0;
	for (i = 0; i < vec_count(buf->batch); i++) {
		struct draw_batch * b = &buf->batch[i];
		uint32_t j;
		b->first_index = buf->index_count;
		vo_idx = 0;
		for (j = 0; j < vec_count(data->sector_list); j++) {
			add_to_batch(buf, b, data->sector_list[j]->geometry,
				data->vertex_offsets[vo_idx]);
			vo_idx++;
		}
		buf->index_count += b->index_count;
	}
	if (!create_buffers(buf)) {
		ERROR("Failed to create vertex/index buffers.");
		return FALSE;
	}
	return TRUE;
}

static boolean draw_construct_task(void * data)
{
	return construct_draw_buffer(data);
}

static void draw_construct_post_task(
	struct task_descriptor * task,
	void * data,
	boolean result)
{
	struct draw_construct_data * d = data;
	free_task(d->mod, task);
	d->mod->task_state = TASK_STATE_FLUSH;
}

static void modified_segments(struct ac_terrain_module * mod)
{
	uint32_t i;
	uint32_t count = 0;
	size_t cb_size;
	struct ac_terrain_cb_segment_modification * cb_data;
	for (i = 0; i < vec_count(mod->visible_sectors); i++) {
		struct ac_terrain_sector * s = from_sector_index(mod,
			&mod->visible_sectors[i]);
		if ((s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED) &&
			(s->flags & AC_TERRAIN_SECTOR_SEGMENT_IS_LOADED)) {
			count++;
		}
	}
	cb_size = sizeof(*cb_data) + 
		count * sizeof(struct ac_terrain_sector *);
	cb_data = alloc(cb_size);
	memset(cb_data, 0, cb_size);
	cb_data->visible_sectors = 
		(struct ac_terrain_sector **)&cb_data[1];
	cb_data->visible_sector_count = count;
	count = 0;
	for (i = 0; i < vec_count(mod->visible_sectors); i++) {
		struct ac_terrain_sector * s = from_sector_index(mod,
			&mod->visible_sectors[i]);
		if ((s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED) &&
			(s->flags & AC_TERRAIN_SECTOR_SEGMENT_IS_LOADED)) {
			cb_data->visible_sectors[count++] = s;
		}
	}
	ap_module_enum_callback(mod, AC_TERRAIN_CB_SEGMENT_MODIFICATION, cb_data);
	dealloc(cb_data);
}

static boolean onregister(
	struct ac_terrain_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_mesh, AC_MESH_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_texture, AC_TEXTURE_MODULE_NAME);
	return TRUE;
}

static boolean oninitialize(struct ac_terrain_module * mod)
{
	if (!create_shaders(mod)) {
		ERROR("Failed to create terrain shaders.");
		return FALSE;
	}
	return TRUE;
}

static void onshutdown(struct ac_terrain_module * mod)
{
	uint32_t i;
	struct sector_load_data * d;
	struct task_descriptor * t;
	uint32_t x;
	uint32_t z;
	/* Make sure that all sector load tasks are completed. */
	task_wait_all();
	for (x = 0; x < AP_SECTOR_WORLD_INDEX_WIDTH; x++) {
		for (z = 0; z < AP_SECTOR_WORLD_INDEX_HEIGHT; z++) {
			struct ac_terrain_sector * s = &mod->sectors[x][z];
			if (s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED)
				unload_sector(mod, s);
		}
	}
	d = mod->load_data_freelist;
	while (d) {
		struct sector_load_data * next = d->next;
		dealloc(d->work_mem);
		dealloc(d);
		d = next;
	}
	t = mod->task_freelist;
	while (t) {
		struct task_descriptor * next = t->next;
		dealloc(t);
		t = next;
	}
	vec_free(mod->visible_sectors);
	vec_free(mod->visible_sectors_tmp);
	vec_free(mod->pending_unload_sectors);
	vec_free(mod->draw_construct_data.vertex_offsets);
	bgfx_destroy_texture(mod->null_tex);
	for (i = 0; i < COUNT_OF(mod->sampler); i++) {
		if (BGFX_HANDLE_IS_VALID(mod->sampler[i]))
			bgfx_destroy_uniform(mod->sampler[i]);
	}
	bgfx_destroy_program(mod->program);
	destroy_draw_buf(mod, &mod->draw_buf);
	destroy_draw_buf(mod, &mod->draw_construct_data.buf);
}

struct ac_terrain_module * ac_terrain_create_module()
{
	struct ac_terrain_module * mod = ap_module_instance_new(AC_TERRAIN_MODULE_NAME,
		sizeof(*mod), onregister, oninitialize, NULL, onshutdown);
	uint32_t x;
	uint32_t z;
	uint32_t i;
	mod->view_distance = AP_SECTOR_WIDTH * 4;
	for (x = 0; x < AP_SECTOR_WORLD_INDEX_WIDTH; x++) {
		for (z = 0; z < AP_SECTOR_WORLD_INDEX_HEIGHT; z++) {
			mod->sectors[x][z].extent_start[0] =
				AP_SECTOR_WORLD_START_X + AP_SECTOR_WIDTH * x;
			mod->sectors[x][z].extent_start[1] =
				AP_SECTOR_WORLD_START_Z + AP_SECTOR_HEIGHT * z;
			mod->sectors[x][z].extent_end[0] =
				AP_SECTOR_WORLD_START_X +
				AP_SECTOR_WIDTH * (x + 1);
			mod->sectors[x][z].extent_end[1] =
				AP_SECTOR_WORLD_START_Z +
				AP_SECTOR_HEIGHT * (z + 1);
			mod->sectors[x][z].index_x = x;
			mod->sectors[x][z].index_z = z;
		}
	}
	mod->visible_sectors = vec_new_reserved(
		sizeof(struct ap_scr_index), 128);
	mod->visible_sectors_tmp = vec_new_reserved(
		sizeof(struct ap_scr_index), 128);
	mod->pending_unload_sectors = vec_new_reserved(
		sizeof(struct ap_scr_index), 128);
	init_draw_buf(&mod->draw_buf);
	init_draw_buf(&mod->draw_construct_data.buf);
	mod->draw_construct_data.mod = mod;
	mod->draw_construct_data.sector_list = vec_new_reserved(
		sizeof(*mod->draw_construct_data.sector_list), 128);
	mod->draw_construct_data.vertex_offsets = vec_new_reserved(
		sizeof(uint32_t), 128);
	mod->null_tex = (bgfx_texture_handle_t)BGFX_INVALID_HANDLE;
	for (i = 0; i < COUNT_OF(mod->sampler); i++) {
		mod->sampler[i] =
			(bgfx_uniform_handle_t)BGFX_INVALID_HANDLE;
	}
	return mod;
}

void ac_terrain_add_callback(
	struct ac_terrain_module * mod,
	enum ac_terrain_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

struct ac_terrain_sector * ac_terrain_get_sector(
	struct ac_terrain_module * mod, 
	uint32_t x, 
	uint32_t z)
{
	if (x >= AP_SECTOR_WORLD_INDEX_WIDTH ||
		z >= AP_SECTOR_WORLD_INDEX_HEIGHT)
		return NULL;
	return &mod->sectors[x][z];
}

struct ac_terrain_sector * ac_terrain_get_sector_at(
	struct ac_terrain_module * mod,
	const vec3 p)
{
	uint32_t sx;
	uint32_t sz;
	if (!ap_scr_pos_to_index(p, &sx, &sz))
		return NULL;
	return &mod->sectors[sx][sz];
}

void ac_terrain_set_view_distance(
	struct ac_terrain_module * mod,
	float view_distance)
{
	mod->view_distance = view_distance;
	/* TODO: Load/Unload sectors. */
}

float ac_terrain_get_view_distance(struct ac_terrain_module * mod)
{
	return mod->view_distance;
}

void ac_terrain_sync(
	struct ac_terrain_module * mod,
	const float * pos, 
	boolean force)
{
	struct pack * pack = NULL;
	uint32_t pack_x = UINT32_MAX;
	uint32_t pack_z = UINT32_MAX;
	uint32_t i;
	if (mod->task_state != TASK_STATE_IDLE)
		return;
	if (!force) {
		vec2 vp = { pos[0], pos[2] };
		float dd = glm_vec2_distance(mod->last_sync_pos, vp);
		if (dd < AP_SECTOR_WIDTH)
			return;
	}
	vec_copy(&mod->visible_sectors_tmp, mod->visible_sectors);
	calc_visible_sectors(mod, pos);
	for (i = 0; i < vec_count(mod->visible_sectors_tmp); i++) {
		struct ap_scr_index * idx = &mod->visible_sectors_tmp[i];
		struct ac_terrain_sector * s = &mod->sectors[idx->x][idx->z];
		if (!ap_scr_find_index(mod->visible_sectors, idx) &&
			(s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED) &&
			!(s->flags & AC_TERRAIN_SECTOR_HAS_DETAIL_CHANGES) &&
			!(s->flags & AC_TERRAIN_SECTOR_HAS_SEGMENT_CHANGES)) {
			vec_push_back(&mod->pending_unload_sectors, idx);
			mod->task_state = TASK_STATE_PRE_DRAW_BUFFER;
		}
	}
	for (i = 0; i < vec_count(mod->visible_sectors); i++) {
		const struct ap_scr_index * idx =
			&mod->visible_sectors[i];
		struct ac_terrain_sector * s = &mod->sectors[idx->x][idx->z];
		if (!(s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED))
			create_sector_load_task(mod, idx);
	}
	mod->last_sync_pos[0] = pos[0];
	mod->last_sync_pos[1] = pos[2];
}

void ac_terrain_update(struct ac_terrain_module * mod, float dt)
{
	if (mod->update_draw_buffer_in) {
		uint32_t ms = (uint32_t)(dt * 1000.0f);
		if (ms < mod->update_draw_buffer_in) {
			mod->update_draw_buffer_in -= ms;
		}
		else if (mod->task_state == TASK_STATE_IDLE) {
			mod->update_draw_buffer_in = 0;
			mod->task_state = TASK_STATE_PRE_DRAW_BUFFER;
		}
	}
	switch (mod->task_state) {
	case TASK_STATE_PRE_DRAW_BUFFER: {
		struct task_descriptor * t = alloc_task(mod);
		struct draw_construct_data * data = 
			&mod->draw_construct_data;
		uint32_t i;
		vec_clear(data->sector_list);
		for (i = 0; i < vec_count(mod->visible_sectors); i++) {
			struct ac_terrain_sector * s = from_sector_index(mod,
				&mod->visible_sectors[i]);
			if (s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED)
				vec_push_back((void **)&data->sector_list, &s);
		}
		t->data = data;
		t->work_cb = draw_construct_task;
		t->post_cb = draw_construct_post_task;
		task_add(t, TRUE);
		mod->task_state = TASK_STATE_DRAW_BUFFER;
		break;
	}
	case TASK_STATE_FLUSH: {
		struct draw_buffer * prev = &mod->draw_buf;
		struct draw_buffer * next = &mod->draw_construct_data.buf;
		struct draw_batch * tmp;
		uint32_t i;
		uint32_t count;
		size_t cb_size;
		struct ac_terrain_cb_post_load_sector * cb_data;
		/* Unload sectors that are no longer visible. */
		for (i = 0; i < vec_count(mod->pending_unload_sectors); i++) {
			struct ac_terrain_sector * s = from_sector_index(mod,
				&mod->pending_unload_sectors[i]);
			/* Unload only if sector hasn't become visible. */
			if (!ap_scr_find_index(mod->visible_sectors,
				&mod->pending_unload_sectors[i]) &&
				!(s->flags & AC_TERRAIN_SECTOR_HAS_DETAIL_CHANGES) &&
				!(s->flags & AC_TERRAIN_SECTOR_HAS_SEGMENT_CHANGES)) {
				unload_sector(mod, s);
			}
		}
		vec_clear(mod->pending_unload_sectors);
		clear_draw_buffer(mod, prev);
		tmp = prev->batch;
		memcpy(prev, next, sizeof(*prev));
		memset(next, 0, sizeof(*next));
		MAKE_VB_INVALID(next->vertex_buffer);
		MAKE_IB_INVALID(next->index_buffer);
		next->batch = tmp;
		mod->task_state = TASK_STATE_IDLE;
		count = 0;
		for (i = 0; i < vec_count(mod->visible_sectors); i++) {
			struct ac_terrain_sector * s = from_sector_index(mod,
				&mod->visible_sectors[i]);
			if (s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED)
				count++;
		}
		cb_size = sizeof(*cb_data) + 
			count * sizeof(struct ac_terrain_sector *);
		cb_data = alloc(cb_size);
		memset(cb_data, 0, cb_size);
		glm_vec2_copy(mod->last_sync_pos, cb_data->sync_pos);
		cb_data->view_distance = mod->view_distance;
		cb_data->sectors = (struct ac_terrain_sector **)&cb_data[1];
		cb_data->sector_count = count;
		count = 0;
		for (i = 0; i < vec_count(mod->visible_sectors); i++) {
			struct ac_terrain_sector * s = from_sector_index(mod,
				&mod->visible_sectors[i]);
			if (s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED)
				cb_data->sectors[count++] = s;
		}
		ap_module_enum_callback(mod, AC_TERRAIN_CB_POST_LOAD_SECTOR, cb_data);
		dealloc(cb_data);
		break;
	}
	}
}

void ac_terrain_render(struct ac_terrain_module * mod)
{
	uint32_t i;
	uint32_t count = 0;
	mat4 model;
	if (!BGFX_HANDLE_IS_VALID(mod->draw_buf.vertex_buffer) ||
		!BGFX_HANDLE_IS_VALID(mod->draw_buf.index_buffer))
		return;
	glm_mat4_identity(model);
	bgfx_set_transform(&model, 1);
	bgfx_set_vertex_buffer(0, mod->draw_buf.vertex_buffer, 0,
		 mod->draw_buf.vertex_count);
	for (i = 0; i < vec_count(mod->draw_buf.batch); i++) {
		const uint8_t discard = 
			BGFX_DISCARD_BINDINGS |
			BGFX_DISCARD_INDEX_BUFFER |
			BGFX_DISCARD_INSTANCE_DATA |
			BGFX_DISCARD_STATE;
		uint64_t state = BGFX_STATE_WRITE_MASK |
			BGFX_STATE_DEPTH_TEST_LESS |
			BGFX_STATE_CULL_CW;
		const struct draw_batch * batch = &mod->draw_buf.batch[i];
		const struct ac_mesh_material * mat = &batch->material;
		uint32_t j;
		bgfx_set_index_buffer(mod->draw_buf.index_buffer,
			batch->first_index, batch->index_count);
		for (j = 0; j < COUNT_OF(mod->sampler); j++) {
			if (BGFX_HANDLE_IS_VALID(mat->tex_handle[j])) {
				ac_texture_test(mod->ac_texture, mat->tex_handle[j]);
				bgfx_set_texture(j, mod->sampler[j], 
					mat->tex_handle[j], UINT32_MAX);
			}
			else {
				bgfx_set_texture(j, mod->sampler[j], 
					mod->null_tex, UINT32_MAX);
			}
		}
		bgfx_set_state(state, 0xffffffff);
		//bgfx_set_debug(BGFX_DEBUG_WIREFRAME);
		bgfx_submit(0, mod->program, 0, discard);
		count++;
	}
	//INFO("DrawCallCount = %u", count);
}

boolean ac_terrain_bind_buffers(struct ac_terrain_module * mod)
{
	if (!BGFX_HANDLE_IS_VALID(mod->draw_buf.vertex_buffer) ||
		!BGFX_HANDLE_IS_VALID(mod->draw_buf.index_buffer))
		return FALSE;
	bgfx_set_vertex_buffer(0, mod->draw_buf.vertex_buffer, 0,
		 mod->draw_buf.vertex_count);
	bgfx_set_index_buffer(mod->draw_buf.index_buffer, 0,
		mod->draw_buf.index_count);
	return TRUE;
}

void ac_terrain_set_tex(
	struct ac_terrain_module * mod, 
	uint8_t stage, 
	bgfx_texture_handle_t tex)
{
	if (stage >= COUNT_OF(mod->sampler))
		return;
	if (BGFX_HANDLE_IS_VALID(tex)) {
		bgfx_set_texture(stage, mod->sampler[stage],
			tex, UINT32_MAX);
	}
	else {
		bgfx_set_texture(stage, mod->sampler[stage],
			mod->null_tex, UINT32_MAX);
	}
}

boolean ac_terrain_raycast(
	struct ac_terrain_module * mod,
	const float * origin,
	const float * dir,
	float * hit_point)
{
	vec3 o = { origin[0], origin[1], origin[2] };
	vec3 d = { dir[0], dir[1], dir[2] };
	uint32_t i;
	boolean hit = FALSE;
	float closest = 0.f;
	for (i = 0; i < vec_count(mod->visible_sectors); i++) {
		const struct ap_scr_index * idx = &mod->visible_sectors[i];
		struct ac_terrain_sector * s = &mod->sectors[idx->x][idx->z];
		uint32_t j;
		float rd;
		if (!(s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED))
			continue;
		if (!bsphere_raycast(&s->geometry->bsphere, o, d, &rd) ||
			(hit && closest < rd))
			continue;
		for (j = 0; j < s->geometry->index_count / 3; j++) {
			const uint16_t * indices =
				&s->geometry->indices[j * 3];
			struct ac_mesh_vertex * v = s->geometry->vertices;
			float dist;
			if (glm_ray_triangle(o, d, v[indices[0]].position,
				v[indices[1]].position, v[indices[2]].position,
				&dist) && (!hit || dist < closest)) {
				hit = TRUE;
				closest = dist;
			}
		}
	}
	if (hit && hit_point) {
		vec3 t;
		glm_vec3_scale(d, closest, t);
		glm_vec3_add(o, t, hit_point);
	}
	return hit;
}

boolean ac_terrain_get_triangle(
	struct ac_terrain_module * mod,
	const vec3 pos,
	struct ac_mesh_vertex * vertices,
	struct ac_mesh_material * material)
{
	uint32_t x;
	uint32_t z;
	struct ac_terrain_sector * s;
	struct ac_mesh_geometry * g;
	uint32_t i;
	vec2 p2d = { pos[0], pos[2] };
	if (!ap_scr_pos_to_index(pos, &x, &z))
		return FALSE;
	s = &mod->sectors[x][z];
	if (!(s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED))
		return FALSE;
	g = s->geometry;
	for (i = 0; i < g->split_count; i++) {
		const struct ac_mesh_split * split =
			&s->geometry->splits[i];
		uint32_t j;
		for (j = 0; j < split->index_count / 3; j++) {
			uint16_t * ind =
				&g->indices[split->index_offset + j * 3];
			vec2 tri[3] = {
				{ g->vertices[ind[0]].position[0],
				  g->vertices[ind[0]].position[2], },
				{ g->vertices[ind[1]].position[0],
				  g->vertices[ind[1]].position[2], },
				{ g->vertices[ind[2]].position[0],
				  g->vertices[ind[2]].position[2], },
			};
			if (vec2_triangle_point(tri[0], tri[1], tri[2], p2d)) {
				uint32_t k;
				for (k = 0; k < 3; k++) {
					memcpy(&vertices[k], &g->vertices[ind[k]],
						sizeof(*vertices));
				}
				if (material) {
					ac_mesh_copy_material(mod->ac_mesh, material,
						&g->materials[split->material_index]);
				}
				return TRUE;
			}
		}
	}
	assert(0);
	return FALSE;
}

boolean ac_terrain_set_triangle(
	struct ac_terrain_module * mod,
	uint32_t triangle_count,
	const struct ac_mesh_vertex * vertices,
	const struct ac_mesh_material * materials)
{
	uint32_t i;
	uint32_t changed = 0;
	struct ac_terrain_sector * sectors[16] = { 0 };
	if (mod->task_state != TASK_STATE_IDLE)
		return FALSE;
	assert(triangle_count < COUNT_OF(sectors));
	for (i = 0; i < triangle_count; i++) {
		struct ac_terrain_sector * s =
			from_triangle(mod, &vertices[i * 3]);
		struct ac_mesh_geometry * g;
		uint32_t j;
		uint32_t x[3] = {
			snap_to_step_x(vertices[i * 3 + 0].position[0]),
			snap_to_step_x(vertices[i * 3 + 1].position[0]),
			snap_to_step_x(vertices[i * 3 + 2].position[0]) };
		uint32_t z[3] = {
			snap_to_step_z(vertices[i * 3 + 0].position[2]),
			snap_to_step_z(vertices[i * 3 + 1].position[2]),
			snap_to_step_z(vertices[i * 3 + 2].position[2]) };
		if (!s || !(s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED))
			return FALSE;
		g = s->geometry;
		for (j = 0; j < g->triangle_count; j++) {
			struct ac_mesh_vertex * v[3] = {
				&g->vertices[g->triangles[j].indices[0]],
				&g->vertices[g->triangles[j].indices[1]],
				&g->vertices[g->triangles[j].indices[2]] };
			boolean found = TRUE;
			uint32_t k;
			for (k = 0; k < 3; k++) {
				if (snap_to_step_x(v[k]->position[0]) != x[k] ||
					snap_to_step_z(v[k]->position[2]) != z[k]) {
					found = FALSE;
					break;
				}
			}
			if (found) {
				found = FALSE;
				for (k = 0; k < 3; k++) {
					memcpy(v[k], &vertices[i * 3 + k],
						sizeof(*vertices));
				}
				ac_mesh_set_material(mod->ac_mesh, g, &g->triangles[j],
					&materials[i]);
				for (k = 0; k < changed; k++) {
					if (sectors[k] == s) {
						found = TRUE;
						break;
					}
				}
				if (!found)
					sectors[changed++] = s;
				break;
			}
		}
	}
	for (i = 0; i < changed; i++) {
		sectors[i]->geometry = ac_mesh_rebuild_splits(mod->ac_mesh,
			sectors[i]->geometry);
		sectors[i]->flags |= AC_TERRAIN_SECTOR_HAS_DETAIL_CHANGES;
		if (!mod->update_draw_buffer_in)
			mod->update_draw_buffer_in = 50;
	}
	return (changed == triangle_count);
}

void ac_terrain_adjust_height(
	struct ac_terrain_module * mod,
	vec3 pos,
	float radius,
	float d,
	enum interp_type interp_type)
{
	vec2 posxz = { pos[0], pos[2] };
	uint32_t count = 0;
	struct draw_construct_data * dcd = &mod->draw_construct_data;
	uint32_t i;
	struct normal_set * nset;
	struct normal_set * nset_edit;
	struct ac_mesh_vertex ** nvtx;
	if (mod->task_state != TASK_STATE_IDLE)
		return;
	nset = vec_new_reserved(sizeof(*nset), 128);
	nset_edit = vec_new_reserved(sizeof(*nset_edit), 128);
	nvtx = vec_new_reserved(sizeof(*nvtx), 128);
	for (i = 0; i < vec_count(dcd->sector_list); i++) {
		struct ac_terrain_sector * s = dcd->sector_list[i];
		struct ac_mesh_geometry * g = s->geometry;
		uint32_t j;
		boolean update_geometry = FALSE;
		if (!(s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED))
			continue;
		for (j = 0; j < g->triangle_count; j++) {
			struct ac_mesh_triangle * t = &g->triangles[j];
			struct ac_mesh_vertex * vtx[3] = {
				&g->vertices[t->indices[0]],
				&g->vertices[t->indices[1]],
				&g->vertices[t->indices[2]] };
			uint32_t k;
			boolean added = FALSE;
			boolean added_edit = FALSE;
			for (k = 0; k < 3; k++) {
				struct ac_mesh_vertex * v = vtx[k];
				vec2 p = { v->position[0], v->position[2] };
				float distance = glm_vec2_distance(p, posxz);
				if (distance < radius) {
					struct normal_set set = { t, g, i };
					//v->position[1] +=
					//	((radius - distance) / radius) * d;
					v->position[1] +=
						interp(interp_type, d, 
							1.0f - (distance / radius));
					update_geometry = TRUE;
					s->flags |= AC_TERRAIN_SECTOR_HAS_DETAIL_CHANGES;
					if (!added_edit) {
						vec_push_back(&nset_edit, &set);
						added_edit = TRUE;
					}
				}
				if (!added &&
					distance < radius + AP_SECTOR_STEPSIZE * 3) {
					struct normal_set set = { t, g, i };
					vec_push_back(&nset, &set);
					added = TRUE;
				}
			}
		}
		if (update_geometry) {
			struct ac_mesh_vertex * vd = mod->draw_buf.vertices;
			const uint32_t * vo = dcd->vertex_offsets;
			memcpy(&vd[vo[i]], g->vertices,
				g->vertex_count * sizeof(*vd));
			bsphere_from_points(&g->bsphere,
				g->vertices->position,
				g->vertex_count,
				sizeof(*g->vertices));
		}
	}
	count = vec_count(nset);
	for (i = 0; i < count; i++) {
		struct normal_set * set = &nset[i];
		struct ac_mesh_vertex * v[3] = {
			&set->g->vertices[set->t->indices[0]],
			&set->g->vertices[set->t->indices[1]],
			&set->g->vertices[set->t->indices[2]] };
		uint32_t j;
		for (j = 0; j < 3; j++) {
			uint32_t k;
			uint32_t count = vec_count(nset_edit);
			for (k = 0; k < count; k++) {
				struct normal_set * s = &nset_edit[k];
				struct ac_mesh_vertex * vtx[3] = {
					&s->g->vertices[s->t->indices[0]],
					&s->g->vertices[s->t->indices[1]],
					&s->g->vertices[s->t->indices[2]] };
				if (ac_mesh_eq_pos(v[j], vtx[0]) ||
					ac_mesh_eq_pos(v[j], vtx[1]) ||
					ac_mesh_eq_pos(v[j], vtx[2])) {
					vec_push_back((void **)&nvtx, &v[j]);
					v[j]->normal[0] = 0.0f;
					v[j]->normal[1] = 0.0f;
					v[j]->normal[2] = 0.0f;
					break;
				}
			}
		}
	}
	count = vec_count(nvtx);
	for (i = 0; i < count; i++) {
		struct ac_mesh_vertex * v = nvtx[i];
		uint32_t c = vec_count(nset);
		uint32_t j;
		for (j = 0; j < c; j++) {
			struct normal_set * s = &nset[j];
			struct ac_mesh_vertex * vtx[3] = {
				&s->g->vertices[s->t->indices[0]],
				&s->g->vertices[s->t->indices[1]],
				&s->g->vertices[s->t->indices[2]] };
			if (ac_mesh_eq_pos(v, vtx[0]) ||
				ac_mesh_eq_pos(v, vtx[1]) ||
				ac_mesh_eq_pos(v, vtx[2])) {
				vec3 n;
				ac_mesh_calculate_surface_normal(s->g, s->t, n);
				v->normal[0] += n[0];
				v->normal[1] += n[1];
				v->normal[2] += n[2];
			}
		}
		{
			vec3 n = { v->normal[0],
				v->normal[1], v->normal[2] };
			glm_vec3_normalize(n);
			v->normal[0] = n[0];
			v->normal[1] = n[1];
			v->normal[2] = n[2];
		}
	}
	if (count) {
		for (i = 0; i < vec_count(dcd->sector_list); i++) {
			struct ac_terrain_sector * s = dcd->sector_list[i];
			struct ac_mesh_geometry * g = s->geometry;
			struct ac_mesh_vertex * vd;
			const uint32_t * vo;
			if (!(s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED))
				continue;
			vd = mod->draw_buf.vertices;
			vo = dcd->vertex_offsets;
			memcpy(&vd[vo[i]], g->vertices,
				g->vertex_count * sizeof(*vd));
		}
		recreate_vertex_buffer(&mod->draw_buf);
	}
	vec_free(nset);
	vec_free(nset_edit);
	vec_free(nvtx);
}

void ac_terrain_level(
	struct ac_terrain_module * mod,
	vec3 pos,
	float radius,
	float d,
	enum interp_type interp_type)
{
	vec2 posxz = { pos[0], pos[2] };
	float height = 0.0f;
	uint32_t count = 0;
	struct draw_construct_data * dcd = &mod->draw_construct_data;
	uint32_t i;
	struct normal_set * nset;
	struct normal_set * nset_edit;
	struct ac_mesh_vertex ** nvtx;
	if (mod->task_state != TASK_STATE_IDLE)
		return;
	for (i = 0; i < vec_count(dcd->sector_list); i++) {
		struct ac_terrain_sector * s = dcd->sector_list[i];
		uint32_t j;
		if (!(s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED))
			continue;
		for (j = 0; j < s->geometry->vertex_count; j++) {
			struct ac_mesh_vertex * v =
				&s->geometry->vertices[j];
			vec2 p = { v->position[0], v->position[2] };
			float distance = glm_vec2_distance(p, posxz);
			if (distance < radius) {
				height += v->position[1];
				count++;
			}
		}
	}
	if (!count)
		return;
	nset = vec_new_reserved(sizeof(*nset), count / 3);
	nset_edit = vec_new_reserved(sizeof(*nset_edit), count / 3);
	nvtx = vec_new_reserved(sizeof(*nvtx), count / 3);
	height /= (float)count;
	for (i = 0; i < vec_count(dcd->sector_list); i++) {
		struct ac_terrain_sector * s = dcd->sector_list[i];
		struct ac_mesh_geometry * g = s->geometry;
		uint32_t j;
		boolean update_geometry = FALSE;
		if (!(s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED))
			continue;
		for (j = 0; j < g->triangle_count; j++) {
			struct ac_mesh_triangle * t = &g->triangles[j];
			struct ac_mesh_vertex * vtx[3] = {
				&g->vertices[t->indices[0]],
				&g->vertices[t->indices[1]],
				&g->vertices[t->indices[2]] };
			uint32_t k;
			boolean added = FALSE;
			boolean added_edit = FALSE;
			for (k = 0; k < 3; k++) {
				struct ac_mesh_vertex * v = vtx[k];
				vec2 p = { v->position[0], v->position[2] };
				float distance = glm_vec2_distance(p, posxz);
				if (distance < radius) {
					struct normal_set set = { t, g, i };
					float dabs = interp(interp_type, d,
						1.0f - (distance / radius));
					if (fabsf(v->position[1] - height) <= dabs) {
						v->position[1] = height;
					}
					else {
						v->position[1] += glm_signf(
							height - v->position[1]) * dabs;
					}
					update_geometry = TRUE;
					s->flags |= AC_TERRAIN_SECTOR_HAS_DETAIL_CHANGES;
					if (!added_edit) {
						vec_push_back(&nset_edit, &set);
						added_edit = TRUE;
					}
				}
				if (!added &&
					distance < radius + AP_SECTOR_STEPSIZE * 3) {
					struct normal_set set = { t, g, i };
					vec_push_back(&nset, &set);
					added = TRUE;
				}
			}
		}
		if (update_geometry) {
			struct ac_mesh_vertex * vd = mod->draw_buf.vertices;
			const uint32_t * vo = dcd->vertex_offsets;
			memcpy(&vd[vo[i]], g->vertices,
				g->vertex_count * sizeof(*vd));
			bsphere_from_points(&g->bsphere,
				g->vertices->position,
				g->vertex_count,
				sizeof(*g->vertices));
		}
	}
	count = vec_count(nset);
	for (i = 0; i < count; i++) {
		struct normal_set * set = &nset[i];
		struct ac_mesh_vertex * v[3] = {
			&set->g->vertices[set->t->indices[0]],
			&set->g->vertices[set->t->indices[1]],
			&set->g->vertices[set->t->indices[2]] };
		uint32_t j;
		for (j = 0; j < 3; j++) {
			uint32_t k;
			uint32_t count = vec_count(nset_edit);
			for (k = 0; k < count; k++) {
				struct normal_set * s = &nset_edit[k];
				struct ac_mesh_vertex * vtx[3] = {
					&s->g->vertices[s->t->indices[0]],
					&s->g->vertices[s->t->indices[1]],
					&s->g->vertices[s->t->indices[2]] };
				if (ac_mesh_eq_pos(v[j], vtx[0]) ||
					ac_mesh_eq_pos(v[j], vtx[1]) ||
					ac_mesh_eq_pos(v[j], vtx[2])) {
					vec_push_back((void **)&nvtx, &v[j]);
					v[j]->normal[0] = 0.0f;
					v[j]->normal[1] = 0.0f;
					v[j]->normal[2] = 0.0f;
					break;
				}
			}
		}
	}
	count = vec_count(nvtx);
	for (i = 0; i < count; i++) {
		struct ac_mesh_vertex * v = nvtx[i];
		uint32_t c = vec_count(nset);
		uint32_t j;
		for (j = 0; j < c; j++) {
			struct normal_set * s = &nset[j];
			struct ac_mesh_vertex * vtx[3] = {
				&s->g->vertices[s->t->indices[0]],
				&s->g->vertices[s->t->indices[1]],
				&s->g->vertices[s->t->indices[2]] };
			if (ac_mesh_eq_pos(v, vtx[0]) ||
				ac_mesh_eq_pos(v, vtx[1]) ||
				ac_mesh_eq_pos(v, vtx[2])) {
				vec3 n;
				ac_mesh_calculate_surface_normal(s->g, s->t, n);
				v->normal[0] += n[0];
				v->normal[1] += n[1];
				v->normal[2] += n[2];
			}
		}
		{
			vec3 n = { v->normal[0],
				v->normal[1], v->normal[2] };
			glm_vec3_normalize(n);
			v->normal[0] = n[0];
			v->normal[1] = n[1];
			v->normal[2] = n[2];
		}
	}
	if (count) {
		for (i = 0; i < vec_count(dcd->sector_list); i++) {
			struct ac_terrain_sector * s = dcd->sector_list[i];
			struct ac_mesh_geometry * g = s->geometry;
			struct ac_mesh_vertex * vd;
			const uint32_t * vo;
			if (!(s->flags & AC_TERRAIN_SECTOR_DETAIL_IS_LOADED))
				continue;
			vd = mod->draw_buf.vertices;
			vo = dcd->vertex_offsets;
			memcpy(&vd[vo[i]], g->vertices,
				g->vertex_count * sizeof(*vd));
		}
		recreate_vertex_buffer(&mod->draw_buf);
	}
	vec_free(nset);
	vec_free(nset_edit);
	vec_free(nvtx);
}

void ac_terrain_set_region_id(
	struct ac_terrain_module * mod,
	vec3 pos,
	uint32_t size,
	uint32_t region_id)
{
	float x;
	uint32_t count = 0;
	if (!size--)
		return;
	x = pos[0] - size * AP_SECTOR_STEPSIZE;
	for (; x <= pos[0] + size * AP_SECTOR_STEPSIZE; x += AP_SECTOR_STEPSIZE) {
		float z;
		z = pos[2] - size * AP_SECTOR_STEPSIZE;
		for (; z <= pos[2] + size * AP_SECTOR_STEPSIZE; z += AP_SECTOR_STEPSIZE) {
			vec3 p = { x, pos[1], z };
			struct ac_terrain_sector * s = 
				ac_terrain_get_sector_at(mod, p);
			struct ac_terrain_segment * seg;
			if (!s || !s->segment_info)
				continue;
			seg = get_segment_by_pos(s, x, z);
			if (seg->region_id == region_id)
				continue;
			seg->region_id = region_id;
			s->flags |= AC_TERRAIN_SECTOR_HAS_SEGMENT_CHANGES;
			count++;
		}
	}
	if (count)
		modified_segments(mod);
}

void ac_terrain_set_tile_info(
	struct ac_terrain_module * mod,
	vec3 pos,
	uint32_t size,
	enum ac_terrain_tile_type tile_type,
	uint8_t geometry_block)
{
	float x;
	uint32_t count = 0;
	if (!size--)
		return;
	x = pos[0] - size * AP_SECTOR_STEPSIZE;
	for (; x <= pos[0] + size * AP_SECTOR_STEPSIZE; x += AP_SECTOR_STEPSIZE) {
		float z;
		z = pos[2] - size * AP_SECTOR_STEPSIZE;
		for (; z <= pos[2] + size * AP_SECTOR_STEPSIZE; z += AP_SECTOR_STEPSIZE) {
			vec3 p = { x, pos[1], z };
			struct ac_terrain_sector * s = 
				ac_terrain_get_sector_at(mod, p);
			struct ac_terrain_segment * seg;
			boolean mod = FALSE;
			if (!s || !s->segment_info)
				continue;
			seg = get_segment_by_pos(s, x, z);
			if (tile_type != AC_TERRAIN_TT_COUNT && 
				seg->tile.tile_type != tile_type) {
				seg->tile.tile_type = tile_type;
				mod = TRUE;
			}
			if (seg->tile.geometry_block != geometry_block) {
				seg->tile.geometry_block = geometry_block;
				mod = TRUE;
			}
			if (mod) {
				s->flags |= AC_TERRAIN_SECTOR_HAS_SEGMENT_CHANGES;
				count++;
			}
		}
	}
	if (count)
		modified_segments(mod);
}

void ac_terrain_commit_changes(struct ac_terrain_module * mod)
{
	create_sector_save_tasks(mod);
}
