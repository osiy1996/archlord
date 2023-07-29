#include <assert.h>
#include <string.h>
#include <al_public/ap_sector.h>
#include <al_runtime/ar_terrain.h>
#include "ar_terrain_internal.h"
#include <al_runtime/ar_config.h>
#include <al_runtime/ar_mesh.h>
#include <al_runtime/ar_renderware.h>
#include <al_runtime/ar_texture.h>
#include <core/bin_stream.h>
#include <core/core.h>
#include <core/file_system.h>
#include <core/malloc.h>
#include <core/log.h>
#include <core/vector.h>
#include <task/task.h>

#define ALEF_COMPACT_FILE_VERSION	0x1000
#define ALEF_COMPACT_FILE_VERSION2	0x1001
#define ALEF_COMPACT_FILE_VERSION3	0x1002
#define ALEF_COMPACT_FILE_VERSION4	0x1003

struct sector_save_data {
	uint32_t x;
	uint32_t z;
	struct ar_mesh_geometry * geometry;
	boolean geometry_save_result;
	struct ar_trn_segment_info * segment_info;
	boolean segment_save_result;
	struct sector_save_data * head;
};

static struct sector_load_data * alloc_load_data(
	struct ae_terrain_module * ctx)
{
	struct sector_load_data * d;
	if (!ctx->task_freelist) {
		d = alloc(sizeof(*d));
		memset(d, 0, sizeof(*d));
		return d;
	}
	d = ctx->load_data_freelist;
	ctx->load_data_freelist = d->next;
	d->segment_info = NULL;
	return d;
}

static void free_load_data(
	struct ae_terrain_module * ctx,
	struct sector_load_data * data)
{
	data->next = ctx->load_data_freelist;
	ctx->load_data_freelist = data;
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
	struct ar_trn_segment_info * sinfo = d->segment_info;
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
				struct ar_trn_segment * s = &sinfo->segments[x][z];
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
		struct ar_trn_segment buf[256];
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
			FALSE, &pack, ar_cfg_get_client_dir(),
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
			FALSE, &pack, ar_cfg_get_client_dir(),
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
	if (!ar_rw_find_chunk(stream, rwID_ATOMIC, NULL)) {
		ERROR("Failed to find rwID_ATOMIC.");
		bstream_destroy(stream);
		destroy_pack(pack);
		return FALSE;
	}
	ar_tex_set_dictionary(AR_DAT_DIR_TEX_WORLD);
	d->geometry = ar_mesh_read_rp_atomic(stream, NULL, 0);
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
	struct ae_terrain_module * ctx = get_ctx();
	struct sector_load_data * d = data;
	struct ar_trn_sector * s = &ctx->sectors[d->x][d->z];
	struct ap_scr_index idx = { d->x, d->z };
	assert(ctx->ongoing_load_task_count != 0);
	if (!--ctx->ongoing_load_task_count)
		ctx->task_state = TASK_STATE_PRE_DRAW_BUFFER;
	free_task(ctx, task);
	if (d->segment_info) {
		s->segment_info = d->segment_info;
		s->flags |= AR_TRN_SECTOR_SEGMENT_IS_LOADED;
	}
	if (d->geometry) {
		if (!ap_scr_find_index(ctx->visible_sectors, &idx)) {
			ar_mesh_destroy_geometry(d->geometry);
			free_load_data(ctx, d);
			return;
		}
		s->geometry = d->geometry;
		s->flags |= AR_TRN_SECTOR_DETAIL_IS_LOADED;
	}
	free_load_data(ctx, d);
}

static boolean save_sector_geometry(
	uint32_t x,
	uint32_t z,
	struct ar_mesh_geometry * g)
{
	struct bin_stream * stream;
	char path[512];
	bstream_for_write(NULL, (size_t)1u << 20, &stream);
	if (!bstream_write_i32(stream, 1)) {
		ERROR("Stream ended unexpectedly.");
		bstream_destroy(stream);
		return FALSE;
	}
	if (!ar_mesh_write_rp_atomic(stream, g)) {
		ERROR("Failed to write atomic.");
		bstream_destroy(stream);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/world/D%03u,%03u.dff",
			ar_cfg_get_client_dir(), x, z)) {
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
	uint32_t x,
	uint32_t z,
	struct ar_trn_segment_info * si)
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
			ar_cfg_get_client_dir(), x, z)) {
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
		d->geometry_save_result = save_sector_geometry(
			d->x, d->z, d->geometry);
	}
	if (d->segment_info) {
		d->segment_save_result = save_sector_segments(
			d->x, d->z, d->segment_info);
	}
	return TRUE;
}

static void sector_save_post_task(
	struct task_descriptor * task,
	void * data,
	boolean result)
{
	struct ae_terrain_module * ctx = get_ctx();
	struct sector_save_data * d = data;
	struct ar_trn_sector * s;
	ctx->completed_save_task_count++;
	s = &ctx->sectors[d->x][d->z];
	if (!d->geometry) {
	}
	else if (d->geometry_save_result) {
		s->flags &= ~AR_TRN_SECTOR_HAS_DETAIL_CHANGES;
		INFO("(%u/%u) Saved file D%03u%03u.dff.",
			ctx->completed_save_task_count, ctx->save_task_count,
			d->x, d->z);
	}
	else {
		ERROR("(%u/%u) Failed to save file D%03u%03u.dff.",
			ctx->completed_save_task_count, ctx->save_task_count,
			d->x, d->z);
	}
	if (!d->segment_info) {
	}
	else if (d->segment_save_result) {
		s->flags &= ~AR_TRN_SECTOR_HAS_SEGMENT_CHANGES;
		INFO("(%u/%u) Saved file C%03u%03u.amf.",
			ctx->completed_save_task_count, ctx->save_task_count,
			d->x, d->z);
	}
	else {
		ERROR("(%u/%u) Failed to save file C%03u%03u.amf.",
			ctx->completed_save_task_count, ctx->save_task_count,
			d->x, d->z);
	}
	if (ctx->completed_save_task_count == ctx->save_task_count) {
		vec_free(d->head);
		/* TODO: Execute mappack to pack saved files. */
		ctx->save_task_count = 0;
		ctx->completed_save_task_count = 0;
	}
}

void create_sector_load_task(
	struct ae_terrain_module * ctx,
	const struct ap_scr_index * index)
{
	struct task_descriptor * t = alloc_task(ctx);
	struct sector_load_data * data = alloc_load_data(ctx);
	data->x = index->x;
	data->z = index->z;
	t->data = data;
	t->work_cb = sector_load_task;
	t->post_cb = sector_load_post_task;
	task_add(t, TRUE);
	ctx->ongoing_load_task_count++;
	ctx->task_state = TASK_STATE_LOAD_SECTOR;
}

void create_sector_save_tasks(struct ae_terrain_module * ctx)
{
	struct sector_save_data * data;
	uint32_t x;
	uint32_t i;
	if (ctx->save_task_count)
		return;
	data = vec_new_reserved(sizeof(*data), 128);
	for (x = 0; x < AP_SECTOR_WORLD_INDEX_WIDTH; x++) {
		uint32_t z;
		for (z = 0; z < AP_SECTOR_WORLD_INDEX_HEIGHT; z++) {
			struct ar_trn_sector * s = &ctx->sectors[x][z];
			struct sector_save_data d = { 0 };
			boolean skip = TRUE;
			if ((s->flags & AR_TRN_SECTOR_DETAIL_IS_LOADED) &&
				(s->flags & AR_TRN_SECTOR_HAS_DETAIL_CHANGES)) {
				d.geometry = s->geometry;
				skip = FALSE;
			}
			if ((s->flags & AR_TRN_SECTOR_SEGMENT_IS_LOADED) &&
				(s->flags & AR_TRN_SECTOR_HAS_SEGMENT_CHANGES)) {
				d.segment_info = s->segment_info;
				skip = FALSE;
			}
			if (!skip) {
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
	ctx->save_task_count = vec_count(data);
	ctx->completed_save_task_count = 0;
	for (i = 0; i < ctx->save_task_count; i++) {
		struct task_descriptor * t = alloc_task(ctx);
		data[i].head = data;
		t->data = &data[i];
		t->work_cb = sector_save_task;
		t->post_cb = sector_save_post_task;
		task_add(t, TRUE);
	}
}
