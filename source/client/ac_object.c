#include "client/ac_object.h"

#include "core/bin_stream.h"
#include "core/file_system.h"
#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "task/task.h"

#include "public/ap_config.h"
#include "public/ap_object.h"
#include "public/ap_octree.h"
#include "public/ap_sector.h"

#include "client/ac_dat.h"
#include "client/ac_mesh.h"
#include "client/ac_render.h"
#include "client/ac_renderware.h"
#include "client/ac_terrain.h"
#include "client/ac_texture.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#define INI_NAME_DFF					"DFF"
#define INI_NAME_CATEGORY				"Category"
#define INI_NAME_RIDABLEMATERIALTYPE	"RidableMaterialType"
#define INI_NAME_OBJECT_TYPE			"ObjectType"
#define INI_NAME_LOD_LEVEL				"LOD_LEVEL"
#define INI_NAME_LOD_DISTANCE			"LOD_DISTANCE"
#define INI_NAME_LOD_HAS_BILLBOARD_NUM	"LOD_BILLBOARD_NUM"
#define INI_NAME_LOD_BILLBOARD_INFO		"LOD_BILLBOARD_INFO"
#define INI_NAME_COLLISION_DFF			"COLLISION_DFF"
#define INI_NAME_PICK_DFF				"PICK_DFF"
#define INI_NAME_PRE_LIGHT				"PRE_LIGHT"
#define INI_NAME_ANIMATION				"ANIMATION"
#define INI_NAME_ANIM_SPEED				"ANIM_SPEED"
#define INI_NAME_BSPHERE				"BSPHERE"
#define INI_NAME_GROUP_COUNT			"GroupCount"
#define GET_ANIM_NAME					"OBJECT_ANIM"
#define INI_NAME_OCTREEDATANUM			"OCTREE_DNUM"
#define INI_NAME_OCTREEDATA				"OCTREE_DATA"
#define INI_NAME_OCTREEDATA_LENGTH		11
#define INI_NAME_OCTREEDATA_MAXBBOX		"OCTREE_MAXBBOX"
#define INI_NAME_DNF_1					"DID_NOT_FINISH_KOREA"
#define INI_NAME_DNF_2					"DID_NOT_FINISH_CHINA"
#define INI_NAME_DNF_3					"DID_NOT_FINISH_WESTERN"
#define INI_NAME_DNF_4					"DID_NOT_FINISH_JAPAN"

size_t g_AcObjectTemplateOffset = SIZE_MAX;

struct ac_object_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	struct ap_object_module * ap_object;
	struct ac_dat_module * ac_dat;
	struct ac_lod_module * ac_lod;
	struct ac_mesh_module * ac_mesh;
	struct ac_render_module * ac_render;
	struct ac_texture_module * ac_texture;
	vec2 last_sync_pos;
	float view_distance;
	struct ap_scr_index * visible_sectors;
	struct ap_scr_index * visible_sectors_tmp;
	struct ac_object_sector sectors[AP_SECTOR_WORLD_INDEX_WIDTH][AP_SECTOR_WORLD_INDEX_HEIGHT];
	bgfx_texture_handle_t null_tex;
	bgfx_uniform_handle_t sampler[5];
	bgfx_program_handle_t program;
	size_t object_offset;
};

void demo(struct ac_object_module * mod, const char * dff)
{
	void * data;
	size_t size;
	struct bin_stream * stream;
	RwChunkHeaderInfo ch;
	if (!ac_dat_load_resource(mod->ac_dat, AC_DAT_DIR_OBJECT, dff, &data, &size)) {
		WARN("Failed to load dff: %s", dff);
		return;
	}
	ac_texture_set_dictionary(mod->ac_texture, AC_DAT_DIR_TEX_OBJECT);
	bstream_from_buffer(data, size, TRUE, &stream);
	while (ac_renderware_read_chunk(stream, &ch)) {
		switch (ch.type) {
		case rwID_CLUMP:
			if (!ac_mesh_read_rp_clump(mod->ac_mesh, stream)) {
				WARN("Failed to read clump (%s).", dff);
				bstream_destroy(stream);
				return;
			}
			break;
		default:
			if (!bstream_advance(stream, ch.length)) {
				WARN("Stream ended unexpectedly (%s).", dff);
				bstream_destroy(stream);
				return;
			}
		}
	}
	bstream_destroy(stream);
}

static struct ac_anim_data * add_anim(
	struct ac_object_template_group * d,
	const char * anim_name)
{
	struct ac_anim_data * ad = alloc(sizeof(*ad));
	memset(ad, 0, sizeof(*ad));
	strlcpy(ad->rt_anim_name, anim_name, sizeof(ad->rt_anim_name));
	if (!d->animation) {
		d->animation = alloc(sizeof(*d->animation));
		memset(d->animation, 0, sizeof(*d->animation));
	}
	if (!d->animation->tail) {
		d->animation->head = ad;
		d->animation->tail = ad;
	}
	else {
		d->animation->tail->next = ad;
		d->animation->tail = ad;
	}
	return ad;
}

static boolean cbobjecttemplatestreamread(
	struct ac_object_module * mod, 
	struct ap_object_template * t,
	struct ap_module_stream * stream)
{
	struct ac_object_template * tmp = ac_object_get_template(t);
	int occindex = 0;
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = ap_module_stream_get_value_name(stream);
		const char * value = ap_module_stream_get_value(stream);
		enum ac_lod_stream_read_result readresult = 
			ac_lod_stream_read(mod->ac_lod, stream, &tmp->lod);
		switch (readresult) {
		case AC_LOD_STREAM_READ_RESULT_ERROR:
			ERROR("Failed to read object lod stream.");
			return FALSE;
		case AC_LOD_STREAM_READ_RESULT_PASS:
			break;
		case AC_LOD_STREAM_READ_RESULT_READ:
			continue;
		}
		if (strcmp(value_name, INI_NAME_DFF) == 0) {
			char temp[256];
			uint32_t index;
			if (sscanf(value, "%u:%s", &index, temp) != 2) {
				ERROR("Failed to read object DFF name (%u).",
					t->tid);
				return FALSE;
			}
			if (!strisempty(temp)) {
				struct ac_object_template_group * d =
					ac_object_get_group(mod, tmp, index, TRUE);
				strlcpy(d->dff_name, temp, sizeof(d->dff_name));
			}
		}
		else if (strcmp(value_name, INI_NAME_ANIMATION) == 0) {
			char temp[256];
			uint32_t index;
			int32_t deprecated[3] = { 0 };
			if (sscanf(value, "%u:%d:%d:%d:%s", &index,
				&deprecated[0], &deprecated[1], &deprecated[2],
				temp) != 5) {
				ERROR("Failed to read object animation name (%u).",
					t->tid);
				return FALSE;
			}
			if (!strisempty(temp)) {
				struct ac_object_template_group * d =
					ac_object_get_group(mod, tmp, index, TRUE);
				add_anim(d, temp);
			}
		}
		else if (strcmp(value_name, INI_NAME_ANIM_SPEED) == 0) {
			uint32_t index;
			int32_t deprecated;
			float speed;
			struct ac_object_template_group * d;
			if (sscanf(value, "%u:%d:%f", &index, &deprecated, 
					&speed) != 3) {
				ERROR("Failed to read object animation speed (%u).",
					t->tid);
				return FALSE;
			}
			d = ac_object_get_group(mod, tmp, index, TRUE);
			d->anim_speed = speed;
		}
		else if (strcmp(value_name, INI_NAME_BSPHERE) == 0) {
			uint32_t index;
			RwSphere bs = { 0 };
			struct ac_object_template_group * d;
			if (sscanf(value, "%u:%f:%f:%f:%f", &index,
					&bs.center.x, &bs.center.y, &bs.center.z,
					&bs.radius) != 5) {
				ERROR("Failed to read object bsphere (%u).",
					t->tid);
				return FALSE;
			}
			d = ac_object_get_group(mod, tmp, index, TRUE);
			d->bsphere = bs;
		}
		else if (strncmp(value_name, 
			AC_RENDER_CRT_STREAM_CUSTOM_DATA1,
			strlen(AC_RENDER_CRT_STREAM_CUSTOM_DATA1)) == 0) {
			uint32_t index;
			struct ac_object_template_group * d;
			if (sscanf(value, "%u", &index) != 1) {
				ERROR("Failed to read object CRT index (%u).",
					t->tid);
				return FALSE;
			}
			d = ac_object_get_group(mod, tmp, index, TRUE);
			while (ap_module_stream_read_next_value(stream)) {
				const char * value_name = 
					ap_module_stream_get_value_name(stream);
				if (strncmp(value_name, 
					AC_RENDER_CRT_STREAM_CUSTOM_DATA2,
					strlen(AC_RENDER_CRT_STREAM_CUSTOM_DATA2)) == 0) {
					break;
				}
				if (!ac_render_stream_read_crt(stream, 
						&d->clump_render_type)) {
					ERROR("Failed to read CRT stream.");
					return FALSE;
				}
			}
		}
		else if (strcmp(value_name, INI_NAME_CATEGORY) == 0) {
			size_t i;
			size_t len;
			strlcpy(tmp->category, value, sizeof(tmp->category));
			len = strlen(tmp->category);
			for (i = 0; i < len; i++) {
				if (!isascii(tmp->category[i]))
					tmp->category[i] = '\0';
			}
		}
		else if (strcmp(value_name, INI_NAME_OBJECT_TYPE) == 0) {
			tmp->object_type = strtoul(value, NULL, 10);
		}
		else if (strcmp(value_name, INI_NAME_COLLISION_DFF) == 0) {
			strlcpy(tmp->collision_dff_name, value, 
				sizeof(tmp->collision_dff_name));
		}
		else if (strcmp(value_name, INI_NAME_PICK_DFF) == 0) {
			strlcpy(tmp->picking_dff_name, value, 
				sizeof(tmp->picking_dff_name));
		}
		else if (strcmp(value_name, INI_NAME_OCTREEDATANUM) == 0) {
			tmp->octree_data.occluder_box_count = (int8_t)strtol(value, NULL, 10);
			if (tmp->octree_data.occluder_box_count) {
				tmp->octree_data.top_verts = alloc(tmp->octree_data.occluder_box_count * 
					4 * sizeof(*tmp->octree_data.top_verts));
				memset(tmp->octree_data.top_verts, 0, tmp->octree_data.occluder_box_count * 
					4 * sizeof(*tmp->octree_data.top_verts));
			}
		}
		else if (strncmp(value_name, INI_NAME_OCTREEDATA, INI_NAME_OCTREEDATA_LENGTH) == 0) {
			int isoccluder;
			if (occindex >= tmp->octree_data.occluder_box_count) {
				ERROR("Too many octree data.");
				return FALSE;
			}
			if (sscanf(value, "%d:%f:%f:%f:%f:%f:%f:%f:%f:%f:%f:%f:%f",
					&isoccluder,
					&tmp->octree_data.top_verts[occindex * 4 + 0].x,
					&tmp->octree_data.top_verts[occindex * 4 + 0].y,
					&tmp->octree_data.top_verts[occindex * 4 + 0].z,
					&tmp->octree_data.top_verts[occindex * 4 + 1].x,
					&tmp->octree_data.top_verts[occindex * 4 + 1].y,
					&tmp->octree_data.top_verts[occindex * 4 + 1].z,
					&tmp->octree_data.top_verts[occindex * 4 + 2].x,
					&tmp->octree_data.top_verts[occindex * 4 + 2].y,
					&tmp->octree_data.top_verts[occindex * 4 + 2].z,
					&tmp->octree_data.top_verts[occindex * 4 + 3].x,
					&tmp->octree_data.top_verts[occindex * 4 + 3].y,
					&tmp->octree_data.top_verts[occindex * 4 + 3].z) != 13) {
				ERROR("Failed to read octree data.");
				return FALSE;
			}
			occindex++;
		}
		else if (strcmp(value_name, INI_NAME_OCTREEDATA_MAXBBOX) == 0) {
			if (sscanf(value, "%f:%f:%f:%f:%f:%f:%f:%f:%f:%f:%f:%f",
					&tmp->octree_data.top_verts_max[0].x,
					&tmp->octree_data.top_verts_max[0].y,
					&tmp->octree_data.top_verts_max[0].z,
					&tmp->octree_data.top_verts_max[1].x,
					&tmp->octree_data.top_verts_max[1].y,
					&tmp->octree_data.top_verts_max[1].z,
					&tmp->octree_data.top_verts_max[2].x,
					&tmp->octree_data.top_verts_max[2].y,
					&tmp->octree_data.top_verts_max[2].z,
					&tmp->octree_data.top_verts_max[3].x,
					&tmp->octree_data.top_verts_max[3].y,
					&tmp->octree_data.top_verts_max[3].z) != 12) {
				ERROR("Failed to read octree max. box.");
				return FALSE;
			}
		}
		else if (strcmp(value_name, INI_NAME_PRE_LIGHT) == 0) {
			int buf[4];
			if (sscanf(value, "%d %d %d %d", &buf[0], &buf[1], &buf[2], &buf[3]) != 4) {
				ERROR("Failed to read pre-light data.");
				return FALSE;
			}
			tmp->pre_light.red = (RwUInt8)buf[0];
			tmp->pre_light.green = (RwUInt8)buf[1];
			tmp->pre_light.blue = (RwUInt8)buf[2];
			tmp->pre_light.alpha = (RwUInt8)buf[3];
		}
		else if (strcmp(value_name, INI_NAME_RIDABLEMATERIALTYPE) == 0) {
			tmp->ridable_material_type = strtol(value, NULL, 10);
		}
		else if (strcmp(value_name, INI_NAME_DNF_1) == 0) {
			int didnotfinish = strtol(value, NULL, 10);
			if (didnotfinish)
				tmp->did_not_finish |= 1u << 0;
		}
		else if (strcmp(value_name, INI_NAME_DNF_2) == 0) {
			int didnotfinish = strtol(value, NULL, 10);
			if (didnotfinish)
				tmp->did_not_finish |= 1u << 1;
		}
		else if (strcmp(value_name, INI_NAME_DNF_3) == 0) {
			int didnotfinish = strtol(value, NULL, 10);
			if (didnotfinish)
				tmp->did_not_finish |= 1u << 2;
		}
		else if (strcmp(value_name, INI_NAME_DNF_4) == 0) {
			int didnotfinish = strtol(value, NULL, 10);
			if (didnotfinish)
				tmp->did_not_finish |= 1u << 3;
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

static boolean writegroups(
	struct ac_object_module * mod, 
	struct ap_module_stream * stream,
	struct ac_object_template * temp)
{
	boolean result = TRUE;
	char valuename[128];
	char value[256];
	struct ac_object_template_group * group = temp->group_list;
	int index = 0;
	while (group) {
		if (!strisempty(group->dff_name)) {
			snprintf(value, sizeof(value), "%d:%s", group->index, group->dff_name);
			result &= ap_module_stream_write_value(stream, INI_NAME_DFF, value);
		}
		if (group->animation && 
			group->animation->head && 
			!strisempty(group->animation->head->rt_anim_name)) {
			snprintf(value, sizeof(value), "%d:%d:%d:%d:%s", group->index, 
				0, 0, 0, group->animation->head->rt_anim_name);
			result &= ap_module_stream_write_value(stream, INI_NAME_ANIMATION, value);
			snprintf(value, sizeof(value), "%d:%d:%f", group->index, 0, group->anim_speed);
			result &= ap_module_stream_write_value(stream, INI_NAME_ANIM_SPEED, value);
		}
		au_ini_mgr_print_compact(value, "%d:%f:%f:%f:%f", group->index,
			group->bsphere.center.x, group->bsphere.center.y, 
			group->bsphere.center.z, group->bsphere.radius);
		result &= ap_module_stream_write_value(stream, INI_NAME_BSPHERE, value);
		if (group->clump_render_type.set_count > 0) {
			snprintf(valuename, sizeof(valuename), "%s%d", AC_RENDER_CRT_STREAM_CUSTOM_DATA1, index);
			result &= ap_module_stream_write_i32(stream, valuename, group->index);
			result &= ac_render_stream_write_clump_render_type(stream, &group->clump_render_type);
			snprintf(valuename, sizeof(valuename), "%s%d", AC_RENDER_CRT_STREAM_CUSTOM_DATA2, group->index);
			result &= ap_module_stream_write_i32(stream, valuename, index);
		}
		group = group->next;
		index++;
	}
	return result;
}

static boolean cbobjecttemplatestreamwrite(
	struct ac_object_module * mod, 
	struct ap_object_template * temp,
	struct ap_module_stream * stream)
{
	struct ac_object_template * attachment = ac_object_get_template(temp);
	boolean result = TRUE;
	char valuename[128];
	char buffer[1024];
	result &= ap_module_stream_write_value(stream, INI_NAME_CATEGORY, attachment->category);
	result &= ap_module_stream_write_i32(stream, INI_NAME_OBJECT_TYPE, attachment->object_type);
	if (!strisempty(attachment->collision_dff_name))
		result &= ap_module_stream_write_value(stream, INI_NAME_COLLISION_DFF, attachment->collision_dff_name);
	if (!strisempty(attachment->picking_dff_name))
		result &= ap_module_stream_write_value(stream, INI_NAME_PICK_DFF, attachment->picking_dff_name);
	result &= ap_module_stream_write_i32(stream, INI_NAME_RIDABLEMATERIALTYPE, attachment->ridable_material_type);
	if (attachment->object_type & AC_OBJECT_TYPE_OCCLUDER) {
		int i;
		result &= ap_module_stream_write_i32(stream, INI_NAME_OCTREEDATANUM, attachment->octree_data.occluder_box_count);
		for (i = 0; i < attachment->octree_data.occluder_box_count; i++) {
			snprintf(valuename, sizeof(valuename), "%s%d", INI_NAME_OCTREEDATA, i);
			snprintf(buffer, sizeof(buffer), "%d:%f:%f:%f:%f:%f:%f:%f:%f:%f:%f:%f:%f",
				1,
				attachment->octree_data.top_verts[i * 4].x,
				attachment->octree_data.top_verts[i * 4].y,
				attachment->octree_data.top_verts[i * 4].z,
				attachment->octree_data.top_verts[i * 4 + 1].x,
				attachment->octree_data.top_verts[i * 4 + 1].y,
				attachment->octree_data.top_verts[i * 4 + 1].z,
				attachment->octree_data.top_verts[i * 4 + 2].x,
				attachment->octree_data.top_verts[i * 4 + 2].y,
				attachment->octree_data.top_verts[i * 4 + 2].z,
				attachment->octree_data.top_verts[i * 4 + 3].x,
				attachment->octree_data.top_verts[i * 4 + 3].y,
				attachment->octree_data.top_verts[i * 4 + 3].z);
			result &= ap_module_stream_write_value(stream, valuename, buffer);
		}
	}
	au_ini_mgr_print_compact(buffer, "%f:%f:%f:%f:%f:%f:%f:%f:%f:%f:%f:%f",
		attachment->octree_data.top_verts_max[0].x,
		attachment->octree_data.top_verts_max[0].y,
		attachment->octree_data.top_verts_max[0].z,
		attachment->octree_data.top_verts_max[1].x,
		attachment->octree_data.top_verts_max[1].y,
		attachment->octree_data.top_verts_max[1].z,
		attachment->octree_data.top_verts_max[2].x,
		attachment->octree_data.top_verts_max[2].y,
		attachment->octree_data.top_verts_max[2].z,
		attachment->octree_data.top_verts_max[3].x,
		attachment->octree_data.top_verts_max[3].y,
		attachment->octree_data.top_verts_max[3].z);
	result &= ap_module_stream_write_value(stream, INI_NAME_OCTREEDATA_MAXBBOX, buffer);
	result &= writegroups(mod, stream, attachment);
	result &= ac_lod_stream_write(mod->ac_lod, stream, &attachment->lod);
	if (attachment->object_type & AC_OBJECT_TYPE_USE_PRE_LIGHT) {
		snprintf(buffer, sizeof(buffer), "%d %d %d %d",
			attachment->pre_light.red,
			attachment->pre_light.green,
			attachment->pre_light.blue,
			attachment->pre_light.alpha);
		result &= ap_module_stream_write_value(stream, INI_NAME_PRE_LIGHT, buffer);
	}
	result &= ap_module_stream_write_i32(stream, INI_NAME_DNF_1, 
		(attachment->did_not_finish & AP_GETSERVICEAREAFLAG(AP_SERVICE_AREA_KOREA)) ? 1 : 0);
	result &= ap_module_stream_write_i32(stream, INI_NAME_DNF_2, 
		(attachment->did_not_finish & AP_GETSERVICEAREAFLAG(AP_SERVICE_AREA_CHINA)) ? 1 : 0);
	result &= ap_module_stream_write_i32(stream, INI_NAME_DNF_4, 
		(attachment->did_not_finish & AP_GETSERVICEAREAFLAG(AP_SERVICE_AREA_JAPAN)) ? 1 : 0);
	return result;
}

boolean ac_object_object_read(
	struct ac_object_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_object * obj = data;
	struct ac_object * objc = ac_object_get_object(mod, obj);
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = ap_module_stream_get_value_name(stream);
		const char * value = ap_module_stream_get_value(stream);
		if (strcmp(value_name, INI_NAME_OBJECT_TYPE) == 0) {
			objc->object_type = strtol(value, NULL, 10);
		}
		else if (strcmp(value_name, INI_NAME_GROUP_COUNT) == 0) {
			assert(strtoul(value, NULL, 10) == 0);
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

boolean ac_object_object_write(
	struct ac_object_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_object * obj = data;
	struct ac_object * objc = ac_object_get_object(mod, obj);
	if (!ap_module_stream_write_i32(stream, INI_NAME_GROUP_COUNT, 0)) {
		ERROR("Failed to write object group count (oid = %u).",
			obj->object_id);
		return FALSE;
	}
	if (!ap_module_stream_write_i32(stream, INI_NAME_OBJECT_TYPE,
			objc->object_type)) {
		ERROR("Failed to write object type (oid = %u).",
			obj->object_id);
		return FALSE;
	}
	return TRUE;
}

static uint32_t get_unique_object_id(
	struct ac_object_module * mod,
	uint32_t sector_x,
	uint32_t sector_z)
{
	uint32_t i;
	uint32_t div_index;
	if (!ap_scr_div_index_from_sector_index(sector_x, sector_z, &div_index))
		return UINT32_MAX;
	div_index <<= 16;
	for (i = 1; i < 0xFFFF; i++) {
		uint32_t x;
		uint32_t id = div_index | i;
		boolean unique = TRUE;
		for (x = sector_x; unique && x < sector_x + AP_SECTOR_DEFAULT_DEPTH; x++) {
			uint32_t z;
			for (z = sector_z; unique && z < sector_z + AP_SECTOR_DEFAULT_DEPTH; z++) {
				uint32_t j;
				uint32_t count;
				struct ac_object_sector *s = &mod->sectors[x][z];
				if (!s->objects)
					continue;
				count = vec_count(s->objects);
				for (j = 0; j < count; j++) {
					if (s->objects[j]->object_id == id) {
						unique = FALSE;
						break;
					}
				}
			}
		}
		if (unique)
			return id;
	}
	return UINT32_MAX;
}

static uint32_t get_unique_object_id_from_list(
	uint32_t * list,
	uint32_t sector_x,
	uint32_t sector_z)
{
	uint32_t i;
	uint32_t div_index;
	if (!ap_scr_div_index_from_sector_index(sector_x, 
			sector_z, &div_index)) {
		return UINT32_MAX;
	}
	div_index <<= 16;
	for (i = 1; i < 0xFFFF; i++) {
		boolean unique = TRUE;
		uint32_t id = div_index | i;
		uint32_t j;
		uint32_t count = vec_count(list);
		for (j = 0; j < count; j++) {
			if (list[j] == id) {
				unique = FALSE;
				break;
			}
		}
		if (unique)
			return id;
	}
	return UINT32_MAX;
}

static uint32_t * read_object_ids(
	struct ac_object_module * mod,
	uint32_t sector_x, 
	uint32_t sector_z)
{
	char buf[1024];
	uint32_t div;
	file file;
	uint32_t * ids;
	if (!ap_scr_div_index_from_sector_index(sector_x, 
			sector_z, &div) ||
		!make_path(buf, sizeof(buf), "%s/ini/obj%05d.ini",
			ap_config_get(mod->ap_config, "ClientDir"), div)) {
		return NULL;
	}
	file = open_file(buf, FILE_ACCESS_READ);
	if (!file)
		return NULL;
	ids = vec_new_reserved(sizeof(*ids), 256);
	while (read_line(file, buf, sizeof(buf))) {
		if (!buf[0])
			continue;
		if (buf[0] == '[') {
			uint32_t id = strtol(buf + 1, NULL, 10);
			vec_push_back(&ids, &id);
		}
	}
	close_file(file);
	return ids;
}

static void create_transform(struct ap_object * obj, mat4 m)
{
	vec3 p = { obj->position.x, obj->position.y, obj->position.z };
	vec3 s = { obj->scale.x, obj->scale.y, obj->scale.z };
	glm_mat4_identity(m);
	glm_translate(m, p);
	glm_rotate_x(m, glm_rad(obj->rotation_x), m);
	glm_rotate_y(m, glm_rad(obj->rotation_y), m);
	glm_scale(m, s);
}

static void transform_bsphere(
	RwSphere * bsphere,
	mat4 m)
{
	vec3 c = { bsphere->center.x, bsphere->center.y, bsphere->center.z };
	vec3 s = { bsphere->radius, bsphere->radius, bsphere->radius };
	glm_mat4_mulv3(m, c, 1.0f, c);
	glm_mat4_mulv3(m, s, 0.0f, s);
	bsphere->center.x = c[0];
	bsphere->center.y = c[1];
	bsphere->center.z = c[2];
	bsphere->radius = MAX(s[0], MAX(s[1], s[2]));
}

static void transform_vertex(mat4 m, struct ac_mesh_vertex * v)
{
	vec3 c = { v->position[0], v->position[1], v->position[2] };
	glm_mat4_mulv3(m, c, 1.0f, c);
	v->position[0] = c[0];
	v->position[1] = c[1];
	v->position[2] = c[2];
}

static boolean raycast_triangle(
	mat4 m, 
	vec3 origin,
	vec3 dir,
	struct ac_mesh_vertex * v0,
	struct ac_mesh_vertex * v1,
	struct ac_mesh_vertex * v2,
	float * hit_distance)
{
	struct ac_mesh_vertex v[3] = { *v0, *v1, *v2 };
	uint32_t i;
	for (i = 0; i < 3; i++)
		transform_vertex(m, &v[i]);
	return glm_ray_triangle(origin, dir, 
		v[0].position,
		v[1].position, 
		v[2].position,
		hit_distance);
}

static void set_object_octree_node(
	struct ap_object * obj,
	struct ac_object * objc,
	struct ap_octree_node * node,
	uint32_t sector_x,
	uint32_t sector_z)
{
	mat4 m;
	struct ac_object_template_group * grp;
	boolean intersects = FALSE;
	struct ac_object_template * temp = ac_object_get_template(obj->temp);
	create_transform(obj, m);
	grp = temp->group_list;
	while (grp) {
		float scale = 
			MAX(obj->scale.x, MAX(obj->scale.y, obj->scale.z));
		vec3 bbox[2] = { 
			{ node->bsphere.center.x - node->hsize,
			  node->bsphere.center.y - node->hsize,
			  node->bsphere.center.z - node->hsize },
			{ node->bsphere.center.x + node->hsize,
			  node->bsphere.center.y + node->hsize,
			  node->bsphere.center.z + node->hsize } };
		vec4 bsphere = { 
			grp->bsphere.center.x, 
			grp->bsphere.center.y, 
			grp->bsphere.center.z, 
			grp->bsphere.radius * scale };
		assert(scale != 0.0f);
		glm_mat4_mulv3(m, bsphere, 1.0f, bsphere);
		if (glm_aabb_sphere(bbox, bsphere)) {
			intersects = TRUE;
			break;
		}
		grp = grp->next;
	}
	if (!intersects)
		return;
	if (node->has_child) {
		uint32_t i;
		for (i = 0; i < 8; i++) {
			set_object_octree_node(obj, objc, node->child[i], 
				sector_x, sector_z);
		}
	}
	else {
		struct ap_octree_id_list * link = alloc(sizeof(*link));
		memset(link, 0, sizeof(*link));
		link->six = sector_x;
		link->siz = sector_z;
		link->id = node->id;
		link->added_node = node;
		link->next = obj->octree_id_list;
		obj->octree_id_list = link;
		obj->octree_id++;
	}
}

static void clear_object_octree_id_list(struct ap_object * obj)
{
	struct ap_octree_id_list * link = obj->octree_id_list;
	while (link) {
		struct ap_octree_id_list * next = link->next;
		dealloc(link);
		link = next;
	}
	obj->octree_id_list = NULL;
	obj->octree_id = 0;
}

static void calc_visible_sectors(
	struct ac_object_module * mod,
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

static boolean create_shaders(struct ac_object_module * mod)
{
	bgfx_shader_handle_t vsh;
	bgfx_shader_handle_t fsh;
	const bgfx_memory_t * mem;
	uint8_t null_data[256];
	if (!ac_render_load_shader("ac_object.vs", &vsh)) {
		ERROR("Failed to load vertex shader.");
		return FALSE;
	}
	if (!ac_render_load_shader("ac_object.fs", &fsh)) {
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

static boolean create_clump_buffers(struct ac_mesh_clump * c)
{
	struct ac_mesh_geometry * g = c->glist;
	while (g) {
		bgfx_vertex_layout_t layout = ac_mesh_vertex_layout();
		const bgfx_memory_t * mem;
		mem = bgfx_copy(g->vertices,
			g->vertex_count * sizeof(*g->vertices));
		g->vertex_buffer = bgfx_create_vertex_buffer(mem, &layout,
			BGFX_BUFFER_NONE);
		if (!BGFX_HANDLE_IS_VALID(g->vertex_buffer)) {
			ERROR("Failed to create vertex buffer.");
			g = g->next;
			continue;
		}
		mem = bgfx_copy(g->indices,
			g->index_count * sizeof(*g->indices));
		g->index_buffer = bgfx_create_index_buffer(mem, 0);
		if (!BGFX_HANDLE_IS_VALID(g->index_buffer)) {
			ERROR("Failed to create index buffer.");
			bgfx_destroy_vertex_buffer(g->vertex_buffer);
			BGFX_INVALIDATE_HANDLE(g->vertex_buffer);
			g = g->next;
			continue;
		}
		g = g->next;
	}
	return TRUE;
}

static struct ac_mesh_clump * load_object_clump(
	struct ac_object_module * mod, 
	const char * dff)
{
	void * data;
	size_t size;
	struct bin_stream * stream;
	RwChunkHeaderInfo ch;
	if (!ac_dat_load_resource(mod->ac_dat, AC_DAT_DIR_OBJECT, dff, 
			&data, &size)) {
		WARN("Failed to load dff: %s", dff);
		return NULL;
	}
	ac_texture_set_dictionary(mod->ac_texture, AC_DAT_DIR_TEX_OBJECT);
	bstream_from_buffer(data, size, TRUE, &stream);
	while (ac_renderware_read_chunk(stream, &ch)) {
		switch (ch.type) {
		case rwID_CLUMP: {
			struct ac_mesh_clump * c = ac_mesh_read_rp_clump(mod->ac_mesh,
				stream);
			static uint32_t count = 0;
			if (!c) {
				WARN("Failed to read clump (%s).", dff);
				bstream_destroy(stream);
				return NULL;
			}
			if (!create_clump_buffers(c)) {
				/* TODO: */
			}
			return c;
		}

		default:
			if (!bstream_advance(stream, ch.length)) {
				WARN("Stream ended unexpectedly (%s).", dff);
				bstream_destroy(stream);
				return NULL;
			}
		}
	}
	bstream_destroy(stream);
	WARN("Stream does not contain rwID_CLUMP (%s).", dff);
	return NULL;
}

static boolean cbobjectinit(struct ac_object_module * mod, void * data)
{
	struct ap_object * obj = data;
	struct ac_object * objc = ac_object_get_object(mod, obj);
	struct ac_object_template * temp;
	assert(obj != NULL);
	assert(objc != NULL);
	temp = ac_object_get_template(obj->temp);
	ac_object_reference_template(mod, temp);
	return TRUE;
}

static boolean cbobjectcopy(struct ac_object_module * mod, void * data)
{
	struct ap_object_cb_copy_object_data * d = data;
	struct ac_object * src = ac_object_get_object(mod, d->src);
	struct ac_object * dst = ac_object_get_object(mod, d->dst);
	struct ac_object_template * temp;
	uint32_t id = get_unique_object_id(mod, src->sector->index_x, 
		src->sector->index_z);
	if (id != UINT32_MAX) {
		d->dst->object_id = id;
	}
	else {
		ERROR("Ran out of available object IDs.");
	}
	dst->is_group_child = src->is_group_child;
	dst->object_type = src->object_type;
	temp = ac_object_get_template(d->src->temp);
	assert(temp->refcount != 0);
	temp->refcount++;
	dst->status = src->status;
	assert(src->sector);
	dst->sector = src->sector;
	vec_push_back((void **)&dst->sector->objects, &d->dst);
	return TRUE;
}

static boolean cbobjectdtor(struct ac_object_module * mod, void * data)
{
	struct ap_object * obj = data;
	struct ac_object * objc = ac_object_get_object(mod, obj);
	struct ac_object_template * temp;
	assert(obj != NULL);
	assert(objc != NULL);
	temp = ac_object_get_template(obj->temp);
	ac_object_release_template(mod, temp);
	if (objc->sector) {
		uint32_t i;
		boolean erased = FALSE;
		uint32_t count;
		assert(objc->sector->objects != NULL);
		count = vec_count(objc->sector->objects);
		for (i = 0; i < count; i++) {
			struct ap_object * o = objc->sector->objects[i];
			if (o == obj) {
				vec_erase(objc->sector->objects,
					&objc->sector->objects[i]);
				erased = TRUE;
				break;
			}
		}
		assert(erased != FALSE);
	}
	return TRUE;
}

static boolean cbobjectmove(struct ac_object_module * mod, void * data)
{
	struct ap_object_cb_move_object_data * d = data;
	struct ac_object * objc = ac_object_get_object(mod, d->obj);
	struct ac_object_sector * s = ac_object_get_sector(mod, &d->obj->position);
	/* Regardless of whether object moves to a new sector, 
	 * the previous sector will be flagged dirty. */
	objc->sector->flags |= AC_OBJECT_SECTOR_HAS_CHANGES;
	if (s && s != objc->sector) {
		uint32_t i;
		uint32_t count;
		boolean erased = FALSE;
		count = vec_count(objc->sector->objects);
		for (i = 0; i < count; i++) {
			if (objc->sector->objects[i] == d->obj) {
				vec_erase(objc->sector->objects, 
					&objc->sector->objects[i]);
				erased = TRUE;
				break;
			}
		}
		assert(erased != FALSE);
		if (!erased)
			return TRUE;
		if (!ap_scr_is_same_division(s->index_x, objc->sector->index_x) ||
			!ap_scr_is_same_division(s->index_z, objc->sector->index_z)) {
			uint32_t id;
			d->obj->object_id = 0;
			id = get_unique_object_id(mod, s->index_x, s->index_z);
			if (id != UINT32_MAX) {
				d->obj->object_id = id;
			}
			else {
				ERROR("Ran out of available object IDs.");
			}
		}
		s->flags |= AC_OBJECT_SECTOR_HAS_CHANGES;
		if (!s->objects)
			s->objects = vec_new_reserved(sizeof(*s->objects), 8);
		vec_push_back((void **)&s->objects, (void *)&d->obj);
		objc->sector = s;
	}
	/*
	struct ac_object * objc = ac_object_get_object(d->obj);
	float minh;
	if (objc->temp && ac_object_get_min_height(objc->temp, &minh)) {
		float origin[3] = { d->obj->position.x, 
			d->obj->position.y + 10000.0f, d->obj->position.z };
		float dir[3] = { 0.0f, -1.0f, 0.0f };
		float hit[3];
		if (ar_trn_raycast(origin, dir, hit))
			d->obj->position.y = hit[1] - minh;
	}
	*/
	return TRUE;
}

static void render_clump(
	struct ac_object_module * mod,
	struct ap_object * obj,
	struct ac_mesh_clump * c,
	struct ac_object_render_data * rd)
{
	struct ac_mesh_geometry * g = c->glist;
	int view = ac_render_get_view(mod->ac_render);
	while (g) {
		const uint8_t discard = 
			BGFX_DISCARD_BINDINGS |
			BGFX_DISCARD_INDEX_BUFFER |
			BGFX_DISCARD_INSTANCE_DATA |
			BGFX_DISCARD_STATE;
		mat4 model;
		uint32_t j;
		vec3 pos = { obj->position.x, obj->position.y, obj->position.z };
		vec3 scale = { obj->scale.x, obj->scale.y, obj->scale.z };
		if (!BGFX_HANDLE_IS_VALID(g->vertex_buffer)) {
			g = g->next;
			continue;
		}
		glm_mat4_identity(model);
		glm_translate(model, pos);
		glm_rotate_x(model, glm_rad(obj->rotation_x), model);
		glm_rotate_y(model, glm_rad(obj->rotation_y), model);
		glm_scale(model, scale);
		bgfx_set_transform(&model, 1);
		bgfx_set_vertex_buffer(0, g->vertex_buffer, 0,
			 g->vertex_count);
		for (j = 0; j < g->split_count; j++) {
			struct ac_mesh_split * s = &g->splits[j];
			bgfx_set_index_buffer(g->index_buffer,
				s->index_offset, s->index_count);
			if (!rd->no_texture) {
				struct ac_mesh_material * mat = 
					&g->materials[s->material_index];
				uint32_t k;
				for (k = 0; k < COUNT_OF(mod->sampler); k++) {
					if (BGFX_HANDLE_IS_VALID(mat->tex_handle[k])) {
						ac_texture_test(mod->ac_texture, mat->tex_handle[k]);
						bgfx_set_texture(k, mod->sampler[k], 
							mat->tex_handle[k], UINT32_MAX);
					}
					else {
						bgfx_set_texture(k, mod->sampler[k], 
							mod->null_tex, UINT32_MAX);
					}
				}
			}
			bgfx_set_state(rd->state, 0xffffffff);
			bgfx_submit(view, rd->program, 0, discard);
		}
		g = g->next;
	}
}

static void render_obj(
	struct ac_object_module * mod, 
	struct ap_object * obj,
	struct ac_object_render_data * rd)
{
	struct ac_object * objc = ac_object_get_object(mod, obj);
	struct ac_object_template * temp = ac_object_get_template(obj->temp);
	struct ac_object_template_group * grp;
	grp = temp->group_list;
	while (grp) {
		if (grp->clump)
			render_clump(mod, obj, grp->clump, rd);
		grp = grp->next;
	}
}

static void unload_sector(
	struct ac_object_module * mod,
	struct ac_object_sector * s)
{
	assert(s->flags & AC_OBJECT_SECTOR_IS_LOADED);
	if (s->flags & AC_OBJECT_SECTOR_HAS_CHANGES) {
		WARN("Unloading a sector with pending changes.");
	}
	if (s->objects) {
		while (vec_count(s->objects))
			ap_object_destroy(mod->ap_object, s->objects[0]);
		vec_free(s->objects);
		s->objects = NULL;
	}
	s->flags &= ~AC_OBJECT_SECTOR_IS_LOADED;
}

static boolean cbobjecttemplatedtor(
	struct ac_object_module * mod,
	struct ap_object_template * temp)
{
	struct ac_object_template * t = ac_object_get_template(temp);
	struct ac_object_template_group * cur = t->group_list;
	while (cur) {
		struct ac_object_template_group * next = cur->next;
		/* TODO: Free animation */
		dealloc(cur);
		cur = next;
	}
	t->group_list = NULL;
	return TRUE;
}

static boolean onregister(
	struct ac_object_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_object, AP_OBJECT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_dat, AC_DAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_lod, AC_LOD_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_mesh, AC_MESH_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_render, AC_RENDER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_texture, AC_TEXTURE_MODULE_NAME);
	ap_object_add_callback(mod->ap_object, AP_OBJECT_CB_INIT_OBJECT, mod,  cbobjectinit);
	ap_object_add_callback(mod->ap_object, AP_OBJECT_CB_COPY_OBJECT, mod, cbobjectcopy);
	ap_object_add_callback(mod->ap_object, AP_OBJECT_CB_MOVE_OBJECT, mod, cbobjectmove);
	mod->object_offset = ap_object_attach_data(mod->ap_object, AP_OBJECT_MDI_OBJECT,
		sizeof(struct ac_object), mod, NULL, cbobjectdtor);
	g_AcObjectTemplateOffset = ap_object_attach_data(mod->ap_object, 
		AP_OBJECT_MDI_OBJECT_TEMPLATE, sizeof(struct ac_object_template),
		mod, NULL, cbobjecttemplatedtor);
	ap_object_add_stream_callback(mod->ap_object, AP_OBJECT_MDI_OBJECT,
		AC_OBJECT_MODULE_NAME, mod, ac_object_object_read, ac_object_object_write);
	ap_object_add_stream_callback(mod->ap_object, AP_OBJECT_MDI_OBJECT_TEMPLATE,
		AC_OBJECT_MODULE_NAME, mod, 
		cbobjecttemplatestreamread, cbobjecttemplatestreamwrite);
	return TRUE;
}

static boolean oninitialize(struct ac_object_module * mod)
{
	struct ac_dat_resource * res;
	uint32_t count;
	uint32_t i;
	if (!create_shaders(mod)) {
		ERROR("Failed to create object shaders.");
		return FALSE;
	}
	if (!ac_dat_batch_begin(mod->ac_dat, AC_DAT_DIR_WORLD_OCTREE)) {
		ERROR("Failed to load octree data.");
		return FALSE;
	}
	res = ac_dat_get_resources(mod->ac_dat, AC_DAT_DIR_WORLD_OCTREE, &count);
	for (i = 0; i < count; i++) {
		const struct ac_dat_resource * r = &res[i];
		uint32_t div;
		uint32_t sx;
		uint32_t sz;
		uint32_t x;
		struct ap_octree_batch * batch;
		void * data;
		size_t size;
		if (!ac_dat_load_resource_from_batch(mod->ac_dat, AC_DAT_DIR_WORLD_OCTREE, 
				r, &data, &size)) {
			ERROR("Failed to load dat resource (%s).", r->name);
			ac_dat_batch_end(mod->ac_dat, AC_DAT_DIR_WORLD_OCTREE);
			return FALSE;
		}
		if (strlen(r->name) != 10 ||
			sscanf(r->name + 2, "%d.dat", &div) != 1) {
			ERROR("Invalid octree file (%s).", r->name);
			ac_dat_batch_end(mod->ac_dat, AC_DAT_DIR_WORLD_OCTREE);
			return FALSE;
		}
		if (!ap_scr_from_division_index(div, &sx, &sz)) {
			ERROR("Invalid octree file (%s).", r->name);
			ac_dat_batch_end(mod->ac_dat, AC_DAT_DIR_WORLD_OCTREE);
			return FALSE;
		}
		batch = ap_octree_create_read_batch(data, size,
			sx / AP_SECTOR_DEFAULT_DEPTH,
			sz / AP_SECTOR_DEFAULT_DEPTH);
		if (!batch) {
			ERROR("Failed to create octree batch.");
			dealloc(data);
			continue;
		}
		for (x = sx; x < sx + AP_SECTOR_DEFAULT_DEPTH; x++){
			uint32_t z;
			for (z = sz; z < sz + AP_SECTOR_DEFAULT_DEPTH; z++) {
				mod->sectors[x][z].octree_roots = 
					ap_octree_load_from_batch(batch, x, z);
			}
		}
		ap_octree_destroy_batch(batch);
		dealloc(data);
	}
	ac_dat_batch_end(mod->ac_dat, AC_DAT_DIR_WORLD_OCTREE);
	return TRUE;
}

static void onclose(struct ac_object_module * mod)
{
	uint32_t i;
	uint32_t x;
	uint32_t z;
	/* Make sure that all sector load tasks are completed. */
	task_wait_all();
	for (x = 0; x < AP_SECTOR_WORLD_INDEX_WIDTH; x++) {
		for (z = 0; z < AP_SECTOR_WORLD_INDEX_HEIGHT; z++) {
			struct ac_object_sector * s = &mod->sectors[x][z];
			if (s->flags & AC_OBJECT_SECTOR_IS_LOADED)
				unload_sector(mod, s);
			if (s->octree_roots) {
				ap_octree_destroy_tree(s->octree_roots);
				s->octree_roots = NULL;
			}
		}
	}
	bgfx_destroy_texture(mod->null_tex);
	for (i = 0; i < COUNT_OF(mod->sampler); i++) {
		if (BGFX_HANDLE_IS_VALID(mod->sampler[i]))
			bgfx_destroy_uniform(mod->sampler[i]);
	}
	bgfx_destroy_program(mod->program);
}

static void onshutdown(struct ac_object_module * mod)
{
	vec_free(mod->visible_sectors);
	vec_free(mod->visible_sectors_tmp);
}

struct ac_object_module * ac_object_create_module()
{
	struct ac_object_module * mod = ap_module_instance_new(AC_OBJECT_MODULE_NAME,
		sizeof(*mod), onregister, oninitialize, onclose, onshutdown);
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
	mod->null_tex = (bgfx_texture_handle_t)BGFX_INVALID_HANDLE;
	for (i = 0; i < COUNT_OF(mod->sampler); i++) {
		mod->sampler[i] =
			(bgfx_uniform_handle_t)BGFX_INVALID_HANDLE;
	}
	return mod;
}

struct ac_object_template_group * ac_object_get_group(
	struct ac_object_module * mod,
	struct ac_object_template * t,
	uint32_t index,
	boolean add)
{
	struct ac_object_template_group * cur = t->group_list;
	struct ac_object_template_group * last = NULL;
	while (cur) {
		if (cur->index == index)
			return cur;
		last = cur;
		cur = cur->next;
	}
	if (add) {
		struct ac_object_template_group * link = alloc(sizeof(*link));
		memset(link, 0, sizeof(*link));
		link->index = index;
		link->anim_speed = 1.0f;
		if (last)
			last->next = link;
		else
			t->group_list = link;
		return link;
	}
	return NULL;
}

void ac_object_reference_template(
	struct ac_object_module * mod, 
	struct ac_object_template * temp)
{
	temp->refcount++;
	if (temp->status == AC_OBJECT_STATUS_INIT) {
		struct ac_object_template_group * grp = temp->group_list;
		while (grp) {
			grp->clump = load_object_clump(mod, grp->dff_name);
			grp = grp->next;
		}
		temp->status = AC_OBJECT_STATUS_LOAD_TEMPLATE;
	}
}

void ac_object_release_template(
	struct ac_object_module * mod, 
	struct ac_object_template * temp)
{
	assert(temp->refcount != 0);
	if (--temp->refcount == 0) {
		struct ac_object_template_group * grp = temp->group_list;
		while (grp) {
			if (grp->clump) {
				ac_mesh_destroy_clump(mod->ac_mesh, grp->clump);
				grp->clump = NULL;
			}
			grp = grp->next;
		}
		temp->status = AC_OBJECT_STATUS_INIT;
	}
}

void ac_object_get_bounding_sphere(
	struct ac_object_module * mod, 
	struct ac_object_template * temp,
	vec4 sphere)
{
	struct ac_object_template_group * group = temp->group_list;
	float * high = NULL;
	glm_vec4_scale(sphere, 0.0f, sphere);
	while (group) {
		struct ac_mesh_geometry * geometry;
		if (!group->clump) {
			group = group->next;
			continue;
		}
		geometry = group->clump->glist;
		while (geometry) {
			if (!high || geometry->bsphere.radius > *high)
				high = &geometry->bsphere.radius;
			vec4 s = { geometry->bsphere.center.x, geometry->bsphere.center.y, 
				geometry->bsphere.center.z, geometry->bsphere.radius };
			glm_sphere_merge(s, sphere, sphere);
			geometry = geometry->next;
		}
		group = group->next;
	}
}

struct ac_object * ac_object_get_object(
	struct ac_object_module * mod, 
	struct ap_object * obj)
{
	return ap_module_get_attached_data(obj, mod->object_offset);
}

struct ap_object ** ac_object_get_objects(
	struct ac_object_module * mod, 
	uint32_t x, 
	uint32_t z)
{
	if (x >= AP_SECTOR_WORLD_INDEX_WIDTH ||
		z >= AP_SECTOR_WORLD_INDEX_HEIGHT) {
		WARN("Invalid sector index (x = %u, z = %u).", x, z);
		return NULL;
	}
	return mod->sectors[x][z].objects;
}

struct ac_object_sector * ac_object_get_sector(
	struct ac_object_module * mod, 
	const struct au_pos * pos)
{
	uint32_t x;
	uint32_t z;
	if (!ap_scr_pos_to_index(&pos->x, &x, &z))
		return NULL;
	return &mod->sectors[x][z];
}

struct ac_object_sector * ac_object_get_sector_by_index(
	struct ac_object_module * mod, 
	uint32_t x,
	uint32_t z)
{
	assert(x < AP_SECTOR_WORLD_INDEX_WIDTH);
	assert(z < AP_SECTOR_WORLD_INDEX_HEIGHT);
	return &mod->sectors[x][z];
}

void ac_object_query_visible_sectors(
	struct ac_object_module * mod, 
	struct ac_object_sector *** sectors)
{
	uint32_t i;
	uint32_t count = vec_count(mod->visible_sectors);
	for (i = 0; i < count; i++) {
		const struct ap_scr_index * idx = &mod->visible_sectors[i];
		struct ac_object_sector * s = &mod->sectors[idx->x][idx->z];
		vec_push_back((void **)sectors, &s);
	}
}

void ac_object_query_visible_objects(
	struct ac_object_module * mod, 
	struct ap_object *** objects)
{
	uint32_t i;
	uint32_t count = vec_count(mod->visible_sectors);
	for (i = 0; i < count; i++) {
		const struct ap_scr_index * idx = &mod->visible_sectors[i];
		struct ac_object_sector * s = &mod->sectors[idx->x][idx->z];
		uint32_t j;
		uint32_t c;
		if (!s->objects)
			continue;
		c = vec_count(s->objects);
		for (j = 0; j < c; j++)
			vec_push_back((void **)objects, &s->objects[j]);
	}
}

void ac_object_sync(
	struct ac_object_module * mod, 
	const vec3 pos, 
	boolean force)
{
	struct pack * pack = NULL;
	uint32_t pack_x = UINT32_MAX;
	uint32_t pack_z = UINT32_MAX;
	uint32_t i;
	char objectdir[512];
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
		struct ac_object_sector * s = &mod->sectors[idx->x][idx->z];
		if ((s->flags & AC_OBJECT_SECTOR_IS_LOADED) &&
			!(s->flags & AC_OBJECT_SECTOR_HAS_CHANGES) &&
			!ap_scr_find_index(mod->visible_sectors, idx)) {
			unload_sector(mod, s);
		}
	}
	make_path(objectdir, sizeof(objectdir), "%s/ini", 
		ap_config_get(mod->ap_config, "ClientDir"));
	for (i = 0; i < vec_count(mod->visible_sectors); i++) {
		const struct ap_scr_index * idx =
			&mod->visible_sectors[i];
		struct ac_object_sector * s = &mod->sectors[idx->x][idx->z];
		uint32_t count = 0;
		if (s->flags & AC_OBJECT_SECTOR_IS_LOADED)
			continue;
		if (!s->objects)
			s->objects = vec_new_reserved(sizeof(*s->objects), 8);
		if (ap_object_load_sector(mod->ap_object, objectdir, &s->objects, 
				idx->x, idx->z)) {
			uint32_t i;
			count = vec_count(s->objects);
			for (i = 0; i < count; i++)
				ac_object_get_object(mod, s->objects[i])->sector = s;
		}
		s->flags |= AC_OBJECT_SECTOR_IS_LOADED;
	}
	mod->last_sync_pos[0] = pos[0];
	mod->last_sync_pos[1] = pos[2];
}

void ac_object_update(struct ac_object_module * mod, float dt)
{
}

void ac_object_render(struct ac_object_module * mod)
{
	uint32_t i;
	for (i = 0; i < vec_count(mod->visible_sectors); i++) {
		const struct ap_scr_index * idx =
			&mod->visible_sectors[i];
		struct ac_object_sector * s = &mod->sectors[idx->x][idx->z];
		if ((s->flags & AC_OBJECT_SECTOR_IS_LOADED) && s->objects) {
			uint32_t j;
			uint32_t count = vec_count(s->objects);
			for (j = 0; j < count; j++) {
				struct ac_object_render_data rd = { 0 };
				rd.state = 
					BGFX_STATE_WRITE_MASK |
					BGFX_STATE_DEPTH_TEST_LESS |
					BGFX_STATE_CULL_CW;
				rd.program = mod->program;
				render_obj(mod, s->objects[j], &rd);
			}
		}
	}
}

struct ap_object * ac_object_pick(
	struct ac_object_module * mod, 
	vec3 origin, 
	vec3 dir)
{
	uint32_t i;
	boolean hit = FALSE;
	float closest = 0.f;
	struct ap_object * hit_obj = NULL;
	for (i = 0; i < vec_count(mod->visible_sectors); i++) {
		const struct ap_scr_index * idx = &mod->visible_sectors[i];
		struct ac_object_sector * s = &mod->sectors[idx->x][idx->z];
		uint32_t j;
		float rd;
		uint32_t count;
		if (!s->objects)
			continue;
		count = vec_count(s->objects);
		for (j = 0; j < count; j++) {
			struct ac_object * obj = ac_object_get_object(mod, s->objects[j]);
			struct ac_object_template_group * grp = 
				ac_object_get_template(s->objects[j]->temp)->group_list;;
			mat4 m;
			create_transform(s->objects[j], m);
			while (grp) {
				if (grp->clump) {
					struct ac_mesh_geometry * g = grp->clump->glist;
					while (g) {
						RwSphere bs = g->bsphere;
						uint32_t k;
						/* Transform and hit-test 
						 * bounding-sphere. */
						transform_bsphere(&bs, m);
						if (!bsphere_raycast(&bs, 
								origin, dir, &rd) ||
							(hit && closest < rd)) {
							g = g->next;
							continue;
						}
						for (k = 0; k < g->index_count / 3; k++) {
							const uint16_t * indices =
								&g->indices[k * 3];
							struct ac_mesh_vertex * v =
								g->vertices;
							float dist;
							if (raycast_triangle(m, 
									origin, dir,
									&v[indices[0]],
									&v[indices[1]],
									&v[indices[2]],
									&dist) && 
								(!hit || dist < closest)) {
								hit = TRUE;
								closest = dist;
								hit_obj = s->objects[j];
							}
						}
						g = g->next;
					}
				}
				grp = grp->next;
			}
		}
	}
	return hit_obj;
}

void ac_object_render_object(
	struct ac_object_module * mod,
	struct ap_object * obj, 
	struct ac_object_render_data * render_data)
{
	if (!BGFX_HANDLE_IS_VALID(render_data->program))
		render_data->program = mod->program;
	render_obj(mod, obj, render_data);
}

void ac_object_render_object_template(
	struct ac_object_module * mod, 
	struct ap_object_template * temp, 
	struct ac_object_render_data * render_data,
	const struct au_pos * position)
{
	struct ac_object_template * attachment = ac_object_get_template(temp);
	struct ac_object_template_group * grp;
	if (!BGFX_HANDLE_IS_VALID(render_data->program))
		render_data->program = mod->program;
	grp = attachment->group_list;
	while (grp) {
		if (grp->clump) {
			struct ac_mesh_geometry * g = grp->clump->glist;
			int view = ac_render_get_view(mod->ac_render);
			while (g) {
				const uint8_t discard = 
					BGFX_DISCARD_BINDINGS |
					BGFX_DISCARD_INDEX_BUFFER |
					BGFX_DISCARD_INSTANCE_DATA |
					BGFX_DISCARD_STATE;
				mat4 model;
				uint32_t j;
				vec3 pos = { 0.0f, 0.0, 0.0f };
				vec3 scale = { 1.0f, 1.0f, 1.0f };
				if (!BGFX_HANDLE_IS_VALID(g->vertex_buffer)) {
					g = g->next;
					continue;
				}
				glm_mat4_identity(model);
				glm_translate(model, pos);
				glm_scale(model, scale);
				bgfx_set_transform(&model, 1);
				bgfx_set_vertex_buffer(0, g->vertex_buffer, 0, g->vertex_count);
				for (j = 0; j < g->split_count; j++) {
					struct ac_mesh_split * s = &g->splits[j];
					bgfx_set_index_buffer(g->index_buffer,
						s->index_offset, s->index_count);
					if (!render_data->no_texture) {
						struct ac_mesh_material * mat = 
							&g->materials[s->material_index];
						uint32_t k;
						for (k = 0; k < COUNT_OF(mod->sampler); k++) {
							if (BGFX_HANDLE_IS_VALID(mat->tex_handle[k])) {
								ac_texture_test(mod->ac_texture, mat->tex_handle[k]);
								bgfx_set_texture(k, mod->sampler[k], 
									mat->tex_handle[k], UINT32_MAX);
							}
							else {
								bgfx_set_texture(k, mod->sampler[k], 
									mod->null_tex, UINT32_MAX);
							}
						}
					}
					bgfx_set_state(render_data->state, 0xffffffff);
					bgfx_submit(view, render_data->program, 0, discard);
				}
				g = g->next;
			}
		}
		grp = grp->next;
	}
}

boolean ac_object_get_min_height(
	struct ac_object_module * mod,
	const struct ac_object_template * temp,
	float * height)
{
	struct ac_object_template_group * cur;
	boolean first = TRUE;
	float minh = 0.0f;
	if (temp->status != AC_OBJECT_STATUS_LOAD_TEMPLATE)
		return FALSE;
	cur = temp->group_list;
	while (cur) {
		struct ac_mesh_geometry * cg;
		if (!cur->clump) {
			cur = cur->next;
			continue;
		}
		cg = cur->clump->glist;
		while (cg) {
			uint32_t i;
			for (i = 0; i < cg->vertex_count; i++) {
				if (first || cg->vertices[i].position[1] < minh) {
					first = FALSE;
					minh = cg->vertices[i].position[1];
				}
			}
			cg = cg->next;
		}
		cur = cur->next;
	}
	if (!first) {
		*height = minh;
		return TRUE;
	}
	return FALSE;
}

void ac_object_export_sector(
	struct ac_object_module * mod,
	struct ac_object_sector * sector,
	const char * export_directory)
{
	if (sector->objects) {
		uint32_t count = vec_count(sector->objects);
		uint32_t i;
		uint32_t * ids = read_object_ids(mod, sector->index_x, sector->index_z);
		boolean write = TRUE;
		for (i = 0; i < count; i++) {
			struct ap_object * obj = sector->objects[i];
			uint32_t x;
			clear_object_octree_id_list(obj);
			for (x = sector->index_x - 4; x < sector->index_x + 4; x++) {
				uint32_t z;
				for (z = sector->index_z - 4; z < sector->index_z + 4; z++) {
					struct ap_octree_root_list * r;
					if (!ap_scr_is_index_valid(x, z))
						continue;
					if (!mod->sectors[x][z].octree_roots)
						continue;
					r = mod->sectors[x][z].octree_roots->roots;
					while (r) {
						set_object_octree_node(obj,
							ac_object_get_object(mod, obj),
							r->node, x, z);
						r = r->next;
					}
				}
			}
			if (!obj->object_id) {
				assert(0);
				ERROR("Object id cannot be 0.");
				write = FALSE;
				break;
			}
		}
		vec_free(ids);
		if (write) {
			ap_object_write_sector(mod->ap_object, export_directory, 
				sector->index_x, sector->index_z, sector->objects);
			/* TODO */
		}
	}
	INFO("Commited object sector (%u,%u) changes to file (%s/obj%05u.ini).",
		sector->index_x, sector->index_z, export_directory, 
		(sector->index_x / 16) * 100 + (sector->index_z / 16));
}
