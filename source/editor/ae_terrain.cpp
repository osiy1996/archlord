#include "editor/ae_terrain.h"

#include "core/bin_stream.h"
#include "core/core.h"
#include "core/malloc.h"
#include "core/log.h"
#include "core/string.h"
#include "core/vector.h"

#include "task/task.h"

#include "public/ap_config.h"
#include "public/ap_map.h"
#include "public/ap_sector.h"

#include "client/ac_camera.h"
#include "client/ac_imgui.h"
#include "client/ac_mesh.h"
#include "client/ac_renderware.h"
#include "client/ac_terrain.h"
#include "client/ac_texture.h"
#include "client/ac_render.h"

#include "editor/ae_editor_action.h"
#include "editor/ae_texture.h"

#include "vendor/imgui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "vendor/imgui/imgui_internal.h"

#include <assert.h>
#include <string.h>

#define HEIGHT_TOOL_SIZE_SCALE 200.0f
#define LEVEL_TOOL_SIZE_SCALE 200.0f

#define PAINT_SET_COMB(LIST, T1, T2, TC) \
	(LIST)[(((T1) & 0xF) << 4) | ((T2) & 0xF)] = (TC)
#define PAINT_GET_COMB(LIST, T1, T2) \
	(LIST)[(((T1) & 0xF) << 4) | ((T2) & 0xF)]

struct paint_tile_info {
	vec2 uv0;
	vec2 uv1;
	enum paint_tile_type tile;
};

enum edit_mode {
	EDIT_MODE_NONE,
	EDIT_MODE_PAINT,
	EDIT_MODE_HEIGHT,
	EDIT_MODE_LEVEL,
	EDIT_MODE_REGION,
	EDIT_MODE_TILE,
	EDIT_MODE_COUNT
};

enum paint_layer {
	PAINT_LAYER_BASE,
	PAINT_LAYER_0,
	PAINT_LAYER_1,
};

enum paint_tile_type {
	PAINT_TILE_NONE,
	PAINT_TILE_TOP_LEFT,
	PAINT_TILE_TOP,
	PAINT_TILE_TOP_RIGHT,
	PAINT_TILE_LEFT,
	PAINT_TILE_CENTER,
	PAINT_TILE_RIGHT,
	PAINT_TILE_BOTTOM_LEFT,
	PAINT_TILE_BOTTOM,
	PAINT_TILE_BOTTOM_RIGHT,
	PAINT_TILE_SLICE_TL,
	PAINT_TILE_SLICE_TR,
	PAINT_TILE_SLICE_BL,
	PAINT_TILE_SLICE_BR,
	PAINT_TILE_CON_TB,
	PAINT_TILE_CON_BT,
	PAINT_TILE_COUNT
};

enum segment_texture_type {
	SEGMENT_TEX_REGION,
	SEGMENT_TEX_TILE_TYPE,
	SEGMENT_TEX_GB_GROUND,
	SEGMENT_TEX_GB_SKY,
	SEGMENT_TEX_COUNT
};

struct paint_vertex {
	vec3 pos;
	vec2 texcoord;
};

struct paint_tool {
	bgfx_texture_handle_t icon;
	enum paint_layer layer;
	bgfx_program_handle_t program;
	bgfx_program_handle_t program_grid;
	struct ac_mesh_vertex * vertices;
	struct ac_mesh_material * materials;
	uint32_t max_triangle_count;
	uint32_t scale_tex;
	uint32_t tex_browser;
	bgfx_texture_handle_t tex;
	char tex_name[64];
	bgfx_texture_handle_t tex_alpha;
	char tex_alpha_name[64];
	bgfx_uniform_handle_t sampler;
	enum paint_tile_type tile_combinations[256];
	boolean preview;
};

struct height_tool {
	bgfx_texture_handle_t icon;
	bgfx_program_handle_t program;
	bgfx_uniform_handle_t uniform;
	boolean preview;
	vec2 center;
	float size;
	float strength;
	enum interp_type falloff;
};

struct level_tool {
	bgfx_texture_handle_t icon;
	bgfx_program_handle_t program;
	bgfx_uniform_handle_t uniform;
	boolean preview;
	vec2 center;
	float size;
	float strength;
	enum interp_type falloff;
};

struct segment_info {
	bgfx_program_handle_t program;
	bgfx_uniform_handle_t uniform;
	bgfx_uniform_handle_t sampler;
	bgfx_texture_handle_t tex;
	vec2 begin;
	float length;
	float opacity;
};

struct color_bucket {
	uint32_t colors[56];
	boolean in_use[56];
};

struct color_info {
	uint32_t color_index;
	uint32_t refcount;
};

struct region_tool {
	struct color_bucket bucket;
	struct color_info colors[AP_MAP_MAX_REGION_COUNT];
	bgfx_program_handle_t program;
	bgfx_uniform_handle_t uniform;
	bgfx_uniform_handle_t sampler;
	vec2 begin;
	float length;
	float opacity;
	uint32_t size;
	struct ap_map_region_template * selected;
};

struct tile_tool {
	struct color_bucket bucket;
	struct color_info colors[AC_TERRAIN_TT_COUNT];
	bgfx_program_handle_t program;
	bgfx_uniform_handle_t uniform;
	bgfx_uniform_handle_t sampler;
	vec2 begin;
	float length;
	enum segment_texture_type tex_type;
	float opacity;
	uint32_t size;
	bool block_ground;
	bool block_sky;
	boolean button_state;
	struct ac_terrain_tile_info tile;
};

struct ae_terrain_module {
	struct ap_module_instance instance;
	struct ap_map_module * ap_map;
	struct ac_mesh_module * ac_mesh;
	struct ac_render_module * ac_render;
	struct ac_terrain_module * ac_terrain;
	struct ac_texture_module * ac_texture;
	struct ae_editor_action_module * ae_editor_action;
	struct ae_texture_module * ae_texture;
	bool is_toolkit_open;
	enum edit_mode edit_mode;
	bgfx_texture_handle_t placeholder_tex;
	boolean button_states[EDIT_MODE_COUNT];
	struct paint_tool paint;
	struct height_tool height;
	struct level_tool level;
	bgfx_texture_handle_t segment_tex[SEGMENT_TEX_COUNT];
	struct region_tool region;
	struct tile_tool * tile;
	bgfx_texture_handle_t interp_tex[INTERP_COUNT];
	bgfx_texture_info_t interp_info[INTERP_COUNT];
};

static struct paint_tile_info PAINT_TILES[] = {
	{ { 0.75f, 0.0f }, { 1.0f, 0.25f }, PAINT_TILE_TOP_LEFT },
	{ { 0.75f, 0.25f }, { 1.0f, 0.50f }, PAINT_TILE_TOP },
	{ { 0.50f, 0.0f }, { 0.75f, 0.25f }, PAINT_TILE_TOP_RIGHT },
	{ { 0.25f, 0.25f }, { 0.50f, 0.50f }, PAINT_TILE_LEFT },
	{ { 0.75f, 0.75f }, { 1.0f, 1.0f }, PAINT_TILE_CENTER },
	{ { 0.50f, 0.25f }, { 0.75f, 0.50f }, PAINT_TILE_RIGHT },
	{ { 0.25f, 0.0f }, { 0.50f, 0.25f }, PAINT_TILE_BOTTOM_LEFT },
	{ { 0.0f, 0.25f }, { 0.25f, 0.50f }, PAINT_TILE_BOTTOM },
	{ { 0.0f, 0.0f }, { 0.25f, 0.25f },  PAINT_TILE_BOTTOM_RIGHT },
	{ { 0.75f, 0.50f }, { 1.0f, 0.75f },  PAINT_TILE_SLICE_TL },
	{ { 0.50f, 0.50f }, { 0.75f, 0.75f },  PAINT_TILE_SLICE_TR },
	{ { 0.25f, 0.50f }, { 0.50f, 0.75f },  PAINT_TILE_SLICE_BL },
	{ { 0.0f, 0.50f }, { 0.25f, 0.75f },  PAINT_TILE_SLICE_BR },
	{ { 0.0f, 0.75f }, { 0.25f, 1.0f },  PAINT_TILE_CON_TB },
	{ { 0.25f, 0.75f }, { 0.50f, 1.0f },  PAINT_TILE_CON_BT },
};

void init_color_bucket(struct color_bucket * cb)
{
	const uint32_t colors[] = {
		0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF, 0x000000,
		0x800000, 0x008000, 0x000080, 0x808000, 0x800080, 0x008080, 0x808080,
		0xC00000, 0x00C000, 0x0000C0, 0xC0C000, 0xC000C0, 0x00C0C0, 0xC0C0C0,
		0x400000, 0x004000, 0x000040, 0x404000, 0x400040, 0x004040, 0x404040,
		0x200000, 0x002000, 0x000020, 0x202000, 0x200020, 0x002020, 0x202020,
		0x600000, 0x006000, 0x000060, 0x606000, 0x600060, 0x006060, 0x606060,
		0xA00000, 0x00A000, 0x0000A0, 0xA0A000, 0xA000A0, 0x00A0A0, 0xA0A0A0,
		0xE00000, 0x00E000, 0x0000E0, 0xE0E000, 0xE000E0, 0x00E0E0, 0xE0E0E0, };
	assert(sizeof(colors) == sizeof(cb->colors));
	memcpy(cb->colors, colors, sizeof(colors));
	memset(cb->in_use, 0, sizeof(cb->in_use));
}

uint32_t pick_from_color_bucket(struct color_bucket * cb)
{
	uint32_t i;
	for (i = 0; i < COUNT_OF(cb->in_use); i++) {
		if (!cb->in_use[i]) {
			cb->in_use[i] = TRUE;
			return i;
		}
	}
	WARN("Color bucket is empty!");
	return UINT32_MAX;
}

void release_to_color_bucket(
	struct color_bucket * cb,
	uint32_t color_index)
{
	cb->in_use[color_index] = FALSE;
}

static const char * get_tile_type_tag(uint8_t type)
{
	switch (type) {
	case AC_TERRAIN_TT_SOIL:
		return "Soil";
	case AC_TERRAIN_TT_SWAMP:
		return "Swamp";
	case AC_TERRAIN_TT_GRASS:
		return "Grass";
	case AC_TERRAIN_TT_SAND:
		return "Sand";
	case AC_TERRAIN_TT_LEAF:
		return "Leaf";
	case AC_TERRAIN_TT_SNOW:
		return "Snow";
	case AC_TERRAIN_TT_WATER:
		return "Water";
	case AC_TERRAIN_TT_STONE:
		return "Stone";
	case AC_TERRAIN_TT_WOOD:
		return "Wood";
	case AC_TERRAIN_TT_METAL:
		return "Metal";
	case AC_TERRAIN_TT_BONE:
		return "Bone";
	case AC_TERRAIN_TT_MUD:
		return "Mud";
	case AC_TERRAIN_TT_SOILGRASS:
		return "Soil and Grass";
	case AC_TERRAIN_TT_SOLIDSOIL:
		return "Solid Soil";
	case AC_TERRAIN_TT_SPORE:
		return "Spore";
	case AC_TERRAIN_TT_MOSS:
		return "Moss";
	case AC_TERRAIN_TT_GRANITE:
		return "Granite";
	default:
		return "Invalid";
	}
}

static const char * get_seg_tex_type_tag(
	enum segment_texture_type type)
{
	switch (type) {
	case SEGMENT_TEX_TILE_TYPE:
		return "Tile Type";
	case SEGMENT_TEX_GB_GROUND:
		return "Ground Block";
	case SEGMENT_TEX_GB_SKY:
		return "Sky Block";
	default:
		return "Invalid";
	}
}

static inline void bake_segments(
	struct tile_tool * tool,
	enum segment_texture_type type,
	struct ac_terrain_segment segments[16][16],
	uint32_t sector_offset_x,
	uint32_t sector_offset_z,
	uint32_t tex_len,
	uint8_t * tex_data)
{
	uint32_t x;
	for (x = 0; x < 16; x++) {
		uint32_t z;
		for (z = 0; z < 16; z++) {
			struct ac_terrain_segment * s = &segments[x][z];
			uint32_t index = (sector_offset_z + z) * tex_len + 
				sector_offset_x + x;
			assert(index < tex_len * tex_len);
			switch (type) {
			case SEGMENT_TEX_TILE_TYPE: {
				struct color_info * ci = 
					&tool->colors[s->tile.tile_type];
				if (s->tile.tile_type >= AC_TERRAIN_TT_COUNT) {
					WARN("Invalid tile type (%u).",
						s->tile.tile_type);
					break;
				}
				if (ci->color_index == UINT32_MAX) {
					ci->color_index = pick_from_color_bucket(
						&tool->bucket);
					if (ci->color_index == UINT32_MAX) {
						/* Ran out of colors, leave this 
						 * segment as transparent. */
						break;
					}
				}
				((uint32_t *)tex_data)[index] = 0xFF000000 |
					tool->bucket.colors[ci->color_index];
				ci->refcount++;
				break;
			}
			case SEGMENT_TEX_GB_GROUND:
				if (s->tile.geometry_block & AC_TERRAIN_GB_GROUND)
					((uint32_t *)tex_data)[index] = 0xFFFF0000;
				else
					((uint32_t *)tex_data)[index] = 0x00000000;
				break;
			case SEGMENT_TEX_GB_SKY:
				if (s->tile.geometry_block & AC_TERRAIN_GB_SKY)
					((uint32_t *)tex_data)[index] = 0xFFFF0000;
				else
					((uint32_t *)tex_data)[index] = 0x00000000;
				break;
			}
		}
	}
}

boolean init_tile_tool(struct ae_terrain_module * mod)
{
	struct tile_tool * t = (struct tile_tool *)alloc(sizeof(*t));
	uint32_t i;
	bgfx_shader_handle_t vsh;
	bgfx_shader_handle_t fsh;
	memset(t, 0, sizeof(*t));
	t->opacity = 0.1f;
	t->size = 1;
	t->length = ac_terrain_get_view_distance(mod->ac_terrain) * 2.0f;
	t->tex_type = SEGMENT_TEX_TILE_TYPE;
	init_color_bucket(&t->bucket);
	for (i = 0; i < COUNT_OF(t->colors); i++)
		t->colors[i].color_index = UINT32_MAX;
	t->tile.tile_type = AC_TERRAIN_TT_COUNT;
	/* Create shaders and shader objects. */
	if (!ac_render_load_shader("ae_terrain_segment.vs", &vsh)) {
		ERROR("Failed to load vertex shader.");
		return FALSE;
	}
	if (!ac_render_load_shader("ae_terrain_segment.fs", &fsh)) {
		ERROR("Failed to load fragment shader.");
		return FALSE;
	}
	t->program= bgfx_create_program(vsh, fsh,
		true);
	if (!BGFX_HANDLE_IS_VALID(t->program)) {
		ERROR("Failed to create region edit tool program.");
		return FALSE;
	}
	t->uniform = bgfx_create_uniform(
		"u_segmentInfo", BGFX_UNIFORM_TYPE_VEC4, 2);
	if (!BGFX_HANDLE_IS_VALID(t->uniform)) {
		ERROR("Failed to create segment uniform.");
		return FALSE;
	}
	t->sampler = bgfx_create_uniform("s_texSegment",
		BGFX_UNIFORM_TYPE_SAMPLER, 1);
	if (!BGFX_HANDLE_IS_VALID(t->sampler)) {
		ERROR("Failed to create sampler (s_texSegment).");
		return FALSE;
	}
	mod->tile = t;
	return TRUE;
}

void destroy_tile_tool(struct ae_terrain_module * mod)
{
	struct tile_tool * t = mod->tile;
	bgfx_destroy_program(t->program);
	bgfx_destroy_uniform(t->uniform);
	bgfx_destroy_uniform(t->sampler);
	dealloc(t);
	mod->tile = NULL;
}

void rebuild_tile_tex(
	struct ae_terrain_module * mod,
	struct ac_terrain_sector ** sectors,
	uint32_t sector_count)
{
	struct tile_tool * tool = mod->tile;
	vec2 b = { 0 };
	vec2 e = { 0 };
	boolean found[4] = { FALSE, FALSE, FALSE, FALSE };
	uint32_t i;
	uint32_t tex_len;
	uint8_t * tex_data;
	enum segment_texture_type types[3] = {
		SEGMENT_TEX_TILE_TYPE,
		SEGMENT_TEX_GB_GROUND,
		SEGMENT_TEX_GB_SKY };
	/* Find the extent of which the visible sectors cover. */
	for (i = 0; i < sector_count; i++) {
		struct ac_terrain_sector * s = sectors[i];
		if (!found[0] || s->extent_start[0] < b[0]) {
			b[0] = s->extent_start[0];
			found[0] = TRUE;
		}
		if (!found[1] || s->extent_start[1] < b[1]) {
			b[1] = s->extent_start[1];
			found[1] = TRUE;
		}
		if (!found[2] || s->extent_end[0] > e[0]) {
			e[0] = s->extent_end[0];
			found[2] = TRUE;
		}
		if (!found[3] || s->extent_end[1] > e[1]) {
			e[1] = s->extent_end[1];
			found[3] = TRUE;
		}
	}
	if (!found[0] || !found[1] || !found[2] || !found[3]) {
		WARN("Failed to find global sector extent.");
		return;
	}
	assert(e[0] > b[0] && e[1] > b[1]);
	glm_vec2_copy(b, tool->begin);
	tool->length = MAX(e[0] - b[0], e[1] - b[1]);
	tex_len = (uint32_t)(tool->length / AP_SECTOR_STEPSIZE);
	assert(0 != tex_len);
	tex_data = (uint8_t *)alloc(3 * (size_t)tex_len * tex_len * 4);
	memset(tex_data, 0, 3 * (size_t)tex_len * tex_len * 4);
	/* Reset tile color reference counts. */
	for (i = 0; i < COUNT_OF(tool->colors); i++)
		tool->colors[i].refcount = 0;
	for (i = 0; i < sector_count; i++) {
		struct ac_terrain_sector * s = sectors[i];
		uint32_t offset_u = 
			(uint32_t)((s->extent_start[0] - b[0]) / AP_SECTOR_STEPSIZE);
		uint32_t offset_v = 
			(uint32_t)((s->extent_start[1] - b[1]) / AP_SECTOR_STEPSIZE);
		uint32_t j;
		if (!s->segment_info)
			continue;
		for (j = 0; j < COUNT_OF(types); j++) {
			bake_segments(tool, types[j],
				s->segment_info->segments, offset_u, offset_v, 
				tex_len, &tex_data[j * tex_len * tex_len * 4]);
		}
	}
	for (i = 0; i < COUNT_OF(tool->colors); i++) {
		struct color_info * ci = &tool->colors[i];
		if (ci->color_index != UINT32_MAX && !ci->refcount) {
			/* No segment belongs to this region, 
			 * release color. */
			release_to_color_bucket(&tool->bucket,
				ci->color_index);
			ci->color_index = UINT32_MAX;
		}
	}
	for (i = 0; i < 3; i++) {
		const bgfx_memory_t * mem = bgfx_copy(
			&tex_data[i * tex_len * tex_len * 4], 
			(size_t)tex_len * tex_len * 4);
		if (BGFX_HANDLE_IS_VALID(mod->segment_tex[types[i]]))
			bgfx_destroy_texture(mod->segment_tex[types[i]]);
		mod->segment_tex[types[i]] = bgfx_create_texture_2d(
			tex_len, tex_len, false, 1,
			BGFX_TEXTURE_FORMAT_RGBA8, BGFX_SAMPLER_POINT, mem);
		if (!BGFX_HANDLE_IS_VALID(mod->segment_tex[types[i]])) {
			WARN("Failed to create segment texture (%u).",
				types[i]);
		}
	}
	dealloc(tex_data);
}

static boolean cbcustomrendertile(
	struct ae_terrain_module * mod, 
	struct ac_terrain_cb_custom_render * cb)
{
	struct tile_tool * t = mod->tile;
	vec4 uniform[2] = {
		{ t->begin[0], t->begin[1], t->length, 0.0f },
		{ t->opacity, 0.0f, 0.0f, 0.0f } };
	cb->state = BGFX_STATE_WRITE_RGB |
		BGFX_STATE_DEPTH_TEST_LEQUAL |
		BGFX_STATE_CULL_CW |
		BGFX_STATE_BLEND_FUNC_SEPARATE(
			BGFX_STATE_BLEND_SRC_ALPHA,
			BGFX_STATE_BLEND_INV_SRC_ALPHA,
			BGFX_STATE_BLEND_ZERO,
			BGFX_STATE_BLEND_ONE);
	cb->program = t->program;
	bgfx_set_uniform(t->uniform, uniform, 2);
	bgfx_set_texture(0, t->sampler,
		mod->segment_tex[t->tex_type], UINT32_MAX);
	return TRUE;
}

static void render_tile(struct ae_terrain_module * mod)
{
	ac_terrain_custom_render(mod->ac_terrain, mod, 
		(ap_module_default_t)cbcustomrendertile);
}

void render_tile_toolbar(struct ae_terrain_module * mod)
{
	struct tile_tool * tool = mod->tile;
	char label[256] = "Retain Original";
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		ImVec2(4.f, 5.f));
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.f);
	if (ImGui::BeginCombo("Display",
			get_seg_tex_type_tag(tool->tex_type))) {
		enum segment_texture_type types[] = {
			SEGMENT_TEX_TILE_TYPE,
			SEGMENT_TEX_GB_GROUND,
			SEGMENT_TEX_GB_SKY };
		uint32_t i;
		for (i = 0; i < COUNT_OF(types); i++) {
			if (ImGui::Selectable(get_seg_tex_type_tag(types[i]),
					tool->tex_type == types[i])) {
				tool->tex_type = types[i];
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.f);
	ImGui::DragFloat("Opacity", &tool->opacity,
		0.001f, 0.0f, 1.0f, "%g", 0);
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(160.f);
	if (tool->tile.tile_type != AC_TERRAIN_TT_COUNT) {
		strlcpy(label, get_tile_type_tag(tool->tile.tile_type),
			sizeof(label));
	}
	if (ImGui::BeginCombo("Tile Type", label)) {
		uint32_t i;
		if (ImGui::Selectable("Retain Original"))
			tool->tile.tile_type = AC_TERRAIN_TT_COUNT;
		ImGui::Dummy(ImVec2(0.0f, 10.0f));
		ImGui::Text("Visible");
		ImGui::Separator();
		for (i = 0; i < COUNT_OF(tool->colors); i++) {
			if (tool->colors[i].refcount) {
				ImVec4 c = ImGui::ColorConvertU32ToFloat4(
					tool->bucket.colors[tool->colors[i].color_index]);
				c.w = 1.0f;
				snprintf(label, sizeof(label), "##CT%u", i);
				ImGui::ColorButton(label, c, 
					ImGuiColorEditFlags_NoTooltip);
				strlcpy(label, get_tile_type_tag(i),
					sizeof(label));
				ImGui::SameLine();
				if (ImGui::Selectable(label, 
						i == tool->tile.tile_type)) {
					tool->tile.tile_type = i;
				}
			}
		}
		ImGui::Dummy(ImVec2(0.0f, 10.0f));
		ImGui::Text("Non-visible");
		ImGui::Separator();
		for (i = 0; i < COUNT_OF(tool->colors); i++) {
			if (!tool->colors[i].refcount) {
				snprintf(label, sizeof(label), "##CT%u", i);
				strlcpy(label, get_tile_type_tag(i),
					sizeof(label));
				if (ImGui::Selectable(label, 
						i == tool->tile.tile_type)) {
					tool->tile.tile_type = i;
				}
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.f);
	if (ImGui::DragScalar("Size", ImGuiDataType_U32, 
			&tool->size, 0.15f) && tool->size < 1) {
		tool->size = 1;
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.f);
	ImGui::Checkbox("Block Ground", &tool->block_ground);
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.f);
	ImGui::Checkbox("Block Sky", &tool->block_sky);
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::PopStyleVar(1);
}

static void on_mdown_tile(struct ae_terrain_module * mod, vec3 hit_point)
{
	struct tile_tool * t = mod->tile;
	uint8_t geometry_block = 0;
	if (t->block_ground)
		geometry_block |= AC_TERRAIN_GB_GROUND;
	if (t->block_sky)
		geometry_block |= AC_TERRAIN_GB_SKY;
	ac_terrain_set_tile_info(mod->ac_terrain, hit_point, t->size,
		(enum ac_terrain_tile_type)t->tile.tile_type,
		geometry_block);
}

static bgfx_vertex_layout_t paint_vertex_layout()
{
	bgfx_vertex_layout_t layout;
	bgfx_vertex_layout_begin(&layout,
		bgfx_get_renderer_type());
	bgfx_vertex_layout_add(&layout,
		BGFX_ATTRIB_POSITION, 3, BGFX_ATTRIB_TYPE_FLOAT,
		false, false);
	bgfx_vertex_layout_add(&layout,
		BGFX_ATTRIB_TEXCOORD0, 2, BGFX_ATTRIB_TYPE_FLOAT,
		false, false);
	bgfx_vertex_layout_end(&layout);
	return layout;
}

static boolean create_shaders(struct ae_terrain_module * mod)
{
	bgfx_shader_handle_t vsh;
	bgfx_shader_handle_t fsh;
	if (!ac_render_load_shader("ae_terrain_paint.vs", &vsh)) {
		ERROR("Failed to load vertex shader.");
		return FALSE;
	}
	if (!ac_render_load_shader("ae_terrain_paint.fs", &fsh)) {
		ERROR("Failed to load fragment shader.");
		return FALSE;
	}
	mod->paint.program = bgfx_create_program(vsh, fsh,
		true);
	if (!BGFX_HANDLE_IS_VALID(mod->paint.program)) {
		ERROR("Failed to create program.");
		return FALSE;
	}
	mod->paint.sampler = bgfx_create_uniform("s_texAlpha",
		BGFX_UNIFORM_TYPE_SAMPLER, 1);
	if (!BGFX_HANDLE_IS_VALID(mod->paint.sampler)) {
		ERROR("Failed to create sampler (paint).");
		return FALSE;
	}
	if (!ac_render_load_shader("ae_terrain_paint_grid.vs", &vsh)) {
		ERROR("Failed to load vertex shader.");
		return FALSE;
	}
	if (!ac_render_load_shader("ae_terrain_paint_grid.fs", &fsh)) {
		ERROR("Failed to load fragment shader.");
		return FALSE;
	}
	mod->paint.program_grid = bgfx_create_program(vsh, fsh,
		true);
	if (!BGFX_HANDLE_IS_VALID(mod->paint.program)) {
		ERROR("Failed to create program.");
		return FALSE;
	}
	if (!ac_render_load_shader("ae_terrain_height_tool.vs", &vsh)) {
		ERROR("Failed to load vertex shader.");
		return FALSE;
	}
	if (!ac_render_load_shader("ae_terrain_height_tool.fs", &fsh)) {
		ERROR("Failed to load fragment shader.");
		return FALSE;
	}
	mod->height.program = bgfx_create_program(vsh, fsh,
		true);
	if (!BGFX_HANDLE_IS_VALID(mod->height.program)) {
		ERROR("Failed to create height tool program.");
		return FALSE;
	}
	mod->height.uniform = bgfx_create_uniform(
		"u_heightTool", BGFX_UNIFORM_TYPE_VEC4, 1);
	if (!BGFX_HANDLE_IS_VALID(mod->height.uniform)) {
		ERROR("Failed to create uniform.");
		return FALSE;
	}
	if (!ac_render_load_shader("ae_terrain_level_tool.vs", &vsh)) {
		ERROR("Failed to load vertex shader.");
		return FALSE;
	}
	if (!ac_render_load_shader("ae_terrain_level_tool.fs", &fsh)) {
		ERROR("Failed to load fragment shader.");
		return FALSE;
	}
	mod->level.program = bgfx_create_program(vsh, fsh,
		true);
	if (!BGFX_HANDLE_IS_VALID(mod->level.program)) {
		ERROR("Failed to create level tool program.");
		return FALSE;
	}
	mod->level.uniform = bgfx_create_uniform(
		"u_levelTool", BGFX_UNIFORM_TYPE_VEC4, 1);
	if (!BGFX_HANDLE_IS_VALID(mod->level.uniform)) {
		ERROR("Failed to create level tool uniform.");
		return FALSE;
	}
	if (!ac_render_load_shader("ae_terrain_segment.vs", &vsh)) {
		ERROR("Failed to load vertex shader.");
		return FALSE;
	}
	if (!ac_render_load_shader("ae_terrain_segment.fs", &fsh)) {
		ERROR("Failed to load fragment shader.");
		return FALSE;
	}
	mod->region.program = bgfx_create_program(vsh, fsh,
		true);
	if (!BGFX_HANDLE_IS_VALID(mod->region.program)) {
		ERROR("Failed to create region edit tool program.");
		return FALSE;
	}
	mod->region.uniform = bgfx_create_uniform(
		"u_segmentInfo", BGFX_UNIFORM_TYPE_VEC4, 2);
	if (!BGFX_HANDLE_IS_VALID(mod->region.uniform)) {
		ERROR("Failed to create segment uniform.");
		return FALSE;
	}
	mod->region.sampler = bgfx_create_uniform("s_texSegment",
		BGFX_UNIFORM_TYPE_SAMPLER, 1);
	if (!BGFX_HANDLE_IS_VALID(mod->region.sampler)) {
		ERROR("Failed to create sampler (s_texSegment).");
		return FALSE;
	}
	return TRUE;
}

static boolean raycast_terrain(
	struct ae_terrain_module * mod,
	struct ac_camera * cam,
	int mouse_x,
	int mouse_y,
	vec3 point)
{
	int w;
	int h;
	vec3 dir;
	SDL_GetWindowSize(ac_render_get_window(mod->ac_render), &w, &h);
	if (mouse_x < 0 || mouse_y < 0 || mouse_x > w || mouse_y > h)
		return FALSE;
	ac_camera_ray(cam, (mouse_x / (w * 0.5f)) - 1.f,
		1.f - (mouse_y / (h * 0.5f)), dir);
	return ac_terrain_raycast(mod->ac_terrain, cam->eye, dir, point);
}

static void calc_max_triangle_count(struct paint_tool * paint)
{
	bgfx_vertex_layout_t layout = ac_mesh_vertex_layout();
	uint32_t maxcount;
	maxcount = bgfx_get_avail_transient_vertex_buffer(
		UINT32_MAX, &layout);
	paint->max_triangle_count = maxcount / 3;
	maxcount = bgfx_get_avail_transient_index_buffer(UINT32_MAX,
		true) / 3;
	paint->max_triangle_count = MIN(maxcount,
		paint->max_triangle_count);
}

static void snap_to_triangle(
	vec3 dst,
	const vec3 src,
	float side)
{
	int ix = (int)src[0] / (int)AP_SECTOR_STEPSIZE;
	int iz = (int)src[2] / (int)AP_SECTOR_STEPSIZE;
	float cx = ((ix * 2 + 
		copysignf(1.f, src[0])) * AP_SECTOR_STEPSIZE * 0.5f);
	float cz = ((iz * 2 + 
		copysignf(1.f, src[2])) * AP_SECTOR_STEPSIZE * 0.5f);
	dst[0] = cx + side * (AP_SECTOR_STEPSIZE * 0.25f);
	dst[1] = src[1];
	dst[2] = cz + side * (AP_SECTOR_STEPSIZE * 0.25f);
}

static void snaptoquad(
	vec3 dst,
	const vec3 src)
{
	float ix = (int)(src[0] / AP_SECTOR_STEPSIZE) * AP_SECTOR_STEPSIZE;
	float iz = (int)(src[2] / AP_SECTOR_STEPSIZE) * AP_SECTOR_STEPSIZE;
	if (src[0] < 0.0f)
		ix -= AP_SECTOR_STEPSIZE;
	if (src[2] < 0.0f)
		iz -= AP_SECTOR_STEPSIZE;
	dst[0] = ix;
	dst[1] = src[1];
	dst[2] = iz;
}

static void paint_adjust_texcoord(struct paint_tool * paint)
{
	float texcoords[4][4][4] = { 0 };
	uint32_t u;
	uint32_t v;
	float start[2] = { 0 };
	uint32_t i;
	for (u = 0; u < 4; u++) {
		for (v = 0; v < 4; v++) {
			texcoords[u][v][0] = u * 0.25f;
			texcoords[u][v][1] = v * 0.25f;
			texcoords[u][v][2] = (u + 1) * 0.25f;
			texcoords[u][v][3] = (v + 1) * 0.25f;
		}
	}
	u = 0;
	v = 0;
	for (i = 0; i < vec_count(paint->vertices); i++) {
		const struct ac_mesh_vertex * v = &paint->vertices[i];
		if (!i || v->position[0] < start[0])
			start[0] = v->position[0];
		if (!i || v->position[2] < start[1])
			start[1] = v->position[2];
	}
	for (i = 0; i < vec_count(paint->vertices); i++) {
		struct ac_mesh_vertex * vtx = &paint->vertices[i];
		vtx->texcoord[0][0] =
			(vtx->position[0] == start[0]) ? 
				texcoords[u][v][0] : texcoords[u][v][2];
		vtx->texcoord[0][1] =
			(vtx->position[2] == start[1]) ? 
				texcoords[u][v][1] : texcoords[u][v][3];
	}
}

static enum paint_tile_type paint_get_tile_type(
	const struct ac_mesh_vertex * const * v)
{
	uint32_t i;
	float u0 = v[0][0].texcoord[0][0];
	float u1 = v[0][0].texcoord[0][0];
	float v0 = v[0][0].texcoord[0][1];
	float v1 = v[0][0].texcoord[0][1];
	for (i = 0; i < 2; i++) {
		uint32_t j;
		for (j = 0; j < 3; j++) {
			float un = v[i][j].texcoord[0][0]; 
			float vn = v[i][j].texcoord[0][1];
			if (un < u0)
				u0 = un;
			if (un > u1)
				u1 = un;
			if (vn < v0)
				v0 = vn;
			if (vn > v1)
				v1 = vn;
		}
	}
	for (i = 0; i < COUNT_OF(PAINT_TILES); i++) {
		const struct paint_tile_info * info = &PAINT_TILES[i];
		if (info->uv0[0] == u0 && info->uv0[1] == v0 &&
			info->uv1[0] == u1 && info->uv1[1] == v1) {
			return info->tile;
		}
	}
	return PAINT_TILE_NONE;
}

static struct paint_tile_info * paint_get_tile(
	enum paint_tile_type tile)
{
	uint32_t i;
	for (i = 0; i < COUNT_OF(PAINT_TILES); i++) {
		struct paint_tile_info * info = &PAINT_TILES[i];
		if (info->tile == tile)
			return info;
	}
	return NULL;
}

static boolean paint_get_selection(
	struct ae_terrain_module * mod,
	struct paint_tool * paint,
	const vec3 p,
	struct ac_mesh_vertex ** triangle1,
	struct ac_mesh_vertex ** triangle2)
{
	uint32_t count = vec_count(paint->vertices) / 3;
	struct ac_mesh_vertex vertices[6];
	struct ac_mesh_material materials[2];
	uint32_t i;
	boolean add[2] = { TRUE, TRUE };
	struct ac_mesh_vertex ** triangles[2] = { triangle1, triangle2 };
	if (!ac_terrain_get_quad(mod->ac_terrain, p, AP_SECTOR_STEPSIZE, 
			vertices, materials)) {
		ERROR("Failed to retrieve quad.");
		return FALSE;
	}
	for (i = 0; i < count; i++) {
		uint32_t j;
		for (j = 0; j < 2; j++) {
			if (ac_mesh_eq_pos(&paint->vertices[i * 3 + 0], 
					&vertices[j * 3 + 0]) &&
				ac_mesh_eq_pos(&paint->vertices[i * 3 + 1], 
					&vertices[j * 3 + 1]) &&
				ac_mesh_eq_pos(&paint->vertices[i * 3 + 2], 
					&vertices[j * 3 + 2])) {
				*(triangles[j]) = &paint->vertices[i * 3];
				add[j] = FALSE;
			}
		}
	}
	assert(add[0] == add[1]);
	if (add[0] != add[1]) {
		for (i = 0; i < COUNT_OF(materials); i++)
			ac_mesh_release_material(mod->ac_mesh, &materials[i]);
		return FALSE;
	}
	if (!add[0]) {
		for (i = 0; i < COUNT_OF(materials); i++)
			ac_mesh_release_material(mod->ac_mesh, &materials[i]);
		return TRUE;
	}
	if (count + 6 > paint->max_triangle_count) {
		for (i = 0; i < COUNT_OF(materials); i++)
			ac_mesh_release_material(mod->ac_mesh, &materials[i]);
		return NULL;
	}
	for (i = 0; i < 2; i++) {
		uint32_t j;
		for (j = 0; j < 3; j++)
			vec_push_back((void **)&paint->vertices, &vertices[i * 3 + j]);
		*(triangles[i]) = &paint->vertices[vec_count(paint->vertices) - 3];
	}
	for (i = 0; i < COUNT_OF(materials); i++)
		vec_push_back((void **)&paint->materials, &materials[i]);
	return TRUE;
}

/*
 * Manual mapping of all tile combinations. 
 *
 * It essentially describes combinations such as:
 *   top-left + top-right = top
 *   bottom + top = center
 *   etc. */
static void paint_setup_tile_combinations(struct paint_tool * p)
{
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_NONE, PAINT_TILE_NONE, 
		PAINT_TILE_NONE);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_NONE, PAINT_TILE_TOP_LEFT, 
		PAINT_TILE_TOP_LEFT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_NONE, PAINT_TILE_TOP, 
		PAINT_TILE_TOP);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_NONE, PAINT_TILE_TOP_RIGHT, 
		PAINT_TILE_TOP_RIGHT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_NONE, PAINT_TILE_LEFT, 
		PAINT_TILE_LEFT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_NONE, PAINT_TILE_CENTER, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_NONE, PAINT_TILE_RIGHT, 
		PAINT_TILE_RIGHT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_NONE, PAINT_TILE_BOTTOM_LEFT, 
		PAINT_TILE_BOTTOM_LEFT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_NONE, PAINT_TILE_BOTTOM, 
		PAINT_TILE_BOTTOM);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_NONE, PAINT_TILE_BOTTOM_RIGHT, 
		PAINT_TILE_BOTTOM_RIGHT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_NONE, PAINT_TILE_SLICE_TL, 
		PAINT_TILE_SLICE_TL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_NONE, PAINT_TILE_SLICE_TR, 
		PAINT_TILE_SLICE_TR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_NONE, PAINT_TILE_SLICE_BL, 
		PAINT_TILE_SLICE_BL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_NONE, PAINT_TILE_SLICE_BR, 
		PAINT_TILE_SLICE_BR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_NONE, PAINT_TILE_CON_TB, 
		PAINT_TILE_CON_TB);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_NONE, PAINT_TILE_CON_BT, 
		PAINT_TILE_CON_BT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_LEFT, PAINT_TILE_TOP_LEFT, 
		PAINT_TILE_TOP_LEFT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_LEFT, PAINT_TILE_TOP, 
		PAINT_TILE_TOP);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_LEFT, PAINT_TILE_TOP_RIGHT, 
		PAINT_TILE_TOP);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_LEFT, PAINT_TILE_LEFT, 
		PAINT_TILE_LEFT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_LEFT, PAINT_TILE_CENTER, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_LEFT, PAINT_TILE_RIGHT, 
		PAINT_TILE_SLICE_TR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_LEFT, PAINT_TILE_BOTTOM_LEFT, 
		PAINT_TILE_LEFT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_LEFT, PAINT_TILE_BOTTOM, 
		PAINT_TILE_SLICE_BL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_LEFT, PAINT_TILE_BOTTOM_RIGHT, 
		PAINT_TILE_CON_TB);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_LEFT, PAINT_TILE_SLICE_TL, 
		PAINT_TILE_SLICE_TL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_LEFT, PAINT_TILE_SLICE_TR, 
		PAINT_TILE_SLICE_TR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_LEFT, PAINT_TILE_SLICE_BL, 
		PAINT_TILE_SLICE_BL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_LEFT, PAINT_TILE_SLICE_BR, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_LEFT, PAINT_TILE_CON_TB, 
		PAINT_TILE_CON_TB);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_LEFT, PAINT_TILE_CON_BT, 
		PAINT_TILE_SLICE_TL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP, PAINT_TILE_TOP, 
		PAINT_TILE_TOP);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP, PAINT_TILE_TOP_RIGHT, 
		PAINT_TILE_TOP);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP, PAINT_TILE_LEFT, 
		PAINT_TILE_SLICE_TL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP, PAINT_TILE_CENTER, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP, PAINT_TILE_RIGHT, 
		PAINT_TILE_SLICE_TR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP, PAINT_TILE_BOTTOM_LEFT, 
		PAINT_TILE_SLICE_TL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP, PAINT_TILE_BOTTOM, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP, PAINT_TILE_BOTTOM_RIGHT, 
		PAINT_TILE_SLICE_TR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP, PAINT_TILE_SLICE_TL, 
		PAINT_TILE_SLICE_TL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP, PAINT_TILE_SLICE_TR, 
		PAINT_TILE_SLICE_TR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP, PAINT_TILE_SLICE_BL, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP, PAINT_TILE_SLICE_BR, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP, PAINT_TILE_CON_TB, 
		PAINT_TILE_SLICE_TR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP, PAINT_TILE_CON_BT, 
		PAINT_TILE_SLICE_TL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_RIGHT, PAINT_TILE_TOP_RIGHT, 
		PAINT_TILE_TOP_RIGHT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_RIGHT, PAINT_TILE_LEFT, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_RIGHT, PAINT_TILE_CENTER, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_RIGHT, PAINT_TILE_RIGHT, 
		PAINT_TILE_RIGHT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_RIGHT, PAINT_TILE_BOTTOM_LEFT, 
		PAINT_TILE_CON_BT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_RIGHT, PAINT_TILE_BOTTOM, 
		PAINT_TILE_SLICE_BR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_RIGHT, PAINT_TILE_BOTTOM_RIGHT, 
		PAINT_TILE_RIGHT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_RIGHT, PAINT_TILE_SLICE_TL, 
		PAINT_TILE_SLICE_TL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_RIGHT, PAINT_TILE_SLICE_TR, 
		PAINT_TILE_SLICE_TR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_RIGHT, PAINT_TILE_SLICE_BL, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_RIGHT, PAINT_TILE_SLICE_BR, 
		PAINT_TILE_SLICE_BR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_RIGHT, PAINT_TILE_CON_TB, 
		PAINT_TILE_SLICE_TR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_TOP_RIGHT, PAINT_TILE_CON_BT, 
		PAINT_TILE_CON_BT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_LEFT, PAINT_TILE_LEFT, 
		PAINT_TILE_LEFT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_LEFT, PAINT_TILE_CENTER, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_LEFT, PAINT_TILE_RIGHT, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_LEFT, PAINT_TILE_BOTTOM_LEFT, 
		PAINT_TILE_LEFT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_LEFT, PAINT_TILE_BOTTOM, 
		PAINT_TILE_SLICE_BL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_LEFT, PAINT_TILE_BOTTOM_RIGHT, 
		PAINT_TILE_SLICE_BL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_LEFT, PAINT_TILE_SLICE_TL, 
		PAINT_TILE_SLICE_TL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_LEFT, PAINT_TILE_SLICE_TR, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_LEFT, PAINT_TILE_SLICE_BL, 
		PAINT_TILE_SLICE_BL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_LEFT, PAINT_TILE_SLICE_BR, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_LEFT, PAINT_TILE_CON_TB, 
		PAINT_TILE_SLICE_BL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_LEFT, PAINT_TILE_CON_BT, 
		PAINT_TILE_SLICE_TL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_CENTER, PAINT_TILE_CENTER, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_CENTER, PAINT_TILE_RIGHT, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_CENTER, PAINT_TILE_BOTTOM_LEFT, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_CENTER, PAINT_TILE_BOTTOM, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_CENTER, PAINT_TILE_BOTTOM_RIGHT, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_CENTER, PAINT_TILE_SLICE_TL, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_CENTER, PAINT_TILE_SLICE_TR, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_CENTER, PAINT_TILE_SLICE_BL, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_CENTER, PAINT_TILE_SLICE_BR, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_CENTER, PAINT_TILE_CON_TB, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_CENTER, PAINT_TILE_CON_BT, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_RIGHT, PAINT_TILE_RIGHT, 
		PAINT_TILE_RIGHT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_RIGHT, PAINT_TILE_BOTTOM_LEFT, 
		PAINT_TILE_SLICE_BR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_RIGHT, PAINT_TILE_BOTTOM, 
		PAINT_TILE_SLICE_BR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_RIGHT, PAINT_TILE_BOTTOM_RIGHT, 
		PAINT_TILE_RIGHT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_RIGHT, PAINT_TILE_SLICE_TL, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_RIGHT, PAINT_TILE_SLICE_TR, 
		PAINT_TILE_SLICE_TR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_RIGHT, PAINT_TILE_SLICE_BL, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_RIGHT, PAINT_TILE_SLICE_BR, 
		PAINT_TILE_SLICE_BR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_RIGHT, PAINT_TILE_CON_TB, 
		PAINT_TILE_SLICE_TR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_RIGHT, PAINT_TILE_CON_BT, 
		PAINT_TILE_SLICE_BR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM_LEFT, PAINT_TILE_BOTTOM_LEFT, 
		PAINT_TILE_BOTTOM_LEFT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM_LEFT, PAINT_TILE_BOTTOM, 
		PAINT_TILE_BOTTOM);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM_LEFT, PAINT_TILE_BOTTOM_RIGHT, 
		PAINT_TILE_BOTTOM);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM_LEFT, PAINT_TILE_SLICE_TL, 
		PAINT_TILE_SLICE_TL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM_LEFT, PAINT_TILE_SLICE_TR, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM_LEFT, PAINT_TILE_SLICE_BL, 
		PAINT_TILE_SLICE_BL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM_LEFT, PAINT_TILE_SLICE_BR, 
		PAINT_TILE_SLICE_BR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM_LEFT, PAINT_TILE_CON_TB, 
		PAINT_TILE_SLICE_BL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM_LEFT, PAINT_TILE_CON_BT, 
		PAINT_TILE_CON_BT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM, PAINT_TILE_BOTTOM, 
		PAINT_TILE_BOTTOM);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM, PAINT_TILE_BOTTOM_RIGHT, 
		PAINT_TILE_BOTTOM);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM, PAINT_TILE_SLICE_TL, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM, PAINT_TILE_SLICE_TR, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM, PAINT_TILE_SLICE_BL, 
		PAINT_TILE_SLICE_BL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM, PAINT_TILE_SLICE_BR, 
		PAINT_TILE_SLICE_BR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM, PAINT_TILE_CON_TB, 
		PAINT_TILE_SLICE_BL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM, PAINT_TILE_CON_BT, 
		PAINT_TILE_SLICE_BR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM_RIGHT, PAINT_TILE_BOTTOM_RIGHT, 
		PAINT_TILE_BOTTOM_RIGHT);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM_RIGHT, PAINT_TILE_SLICE_TL, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM_RIGHT, PAINT_TILE_SLICE_TR, 
		PAINT_TILE_SLICE_TR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM_RIGHT, PAINT_TILE_SLICE_BL, 
		PAINT_TILE_SLICE_BL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM_RIGHT, PAINT_TILE_SLICE_BR, 
		PAINT_TILE_SLICE_BR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM_RIGHT, PAINT_TILE_CON_TB, 
		PAINT_TILE_CON_TB);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_BOTTOM_RIGHT, PAINT_TILE_CON_BT, 
		PAINT_TILE_SLICE_BR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_TL, PAINT_TILE_SLICE_TL, 
		PAINT_TILE_SLICE_TL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_TL, PAINT_TILE_SLICE_TR, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_TL, PAINT_TILE_SLICE_BL, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_TL, PAINT_TILE_SLICE_BR, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_TL, PAINT_TILE_CON_TB, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_TL, PAINT_TILE_CON_BT, 
		PAINT_TILE_SLICE_TL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_TR, PAINT_TILE_SLICE_TR, 
		PAINT_TILE_SLICE_TR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_TR, PAINT_TILE_SLICE_BL, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_TR, PAINT_TILE_SLICE_BR, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_TR, PAINT_TILE_CON_TB, 
		PAINT_TILE_SLICE_TR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_TR, PAINT_TILE_CON_BT, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_BL, PAINT_TILE_SLICE_BL, 
		PAINT_TILE_SLICE_BL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_BL, PAINT_TILE_SLICE_BR, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_BL, PAINT_TILE_CON_TB, 
		PAINT_TILE_SLICE_BL);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_BL, PAINT_TILE_CON_BT, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_BR, PAINT_TILE_SLICE_BR, 
		PAINT_TILE_SLICE_BR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_BR, PAINT_TILE_CON_TB, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_SLICE_BR, PAINT_TILE_CON_BT, 
		PAINT_TILE_SLICE_BR);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_CON_TB, PAINT_TILE_CON_TB, 
		PAINT_TILE_CON_TB);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_CON_TB, PAINT_TILE_CON_BT, 
		PAINT_TILE_CENTER);
	PAINT_SET_COMB(p->tile_combinations, 
		PAINT_TILE_CON_BT, PAINT_TILE_CON_BT, 
		PAINT_TILE_CON_BT);
}

static enum paint_tile_type paint_get_combined_tile(
	const struct paint_tool * paint,
	enum paint_tile_type prev,
	enum paint_tile_type applied)
{
	static boolean init;
	uint32_t h;
	uint32_t l;
	if (prev > applied) {
		h = prev;
		l = applied;
	}
	else {
		h = applied;
		l = prev;
	}
	return PAINT_GET_COMB(paint->tile_combinations, l, h);
}

static void paint_get_combined_uv(
	const struct paint_tool * paint,
	enum paint_tile_type prev,
	enum paint_tile_type applied,
	vec2 uv0,
	vec2 uv1)
{
	struct paint_tile_info * info = 
		paint_get_tile(paint_get_combined_tile(
			paint, prev, applied));
	glm_vec2_copy(info->uv0, uv0);
	glm_vec2_copy(info->uv1, uv1);
}

static void paint_set_quad(
	struct ae_terrain_module * mod,
	struct paint_tool * paint,
	vec3 start,
	float offset_x,
	float offset_z,
	enum paint_tile_type tile)
{
	uint32_t i;
	struct ac_mesh_vertex * vertices[2] = { 0 };
	vec2 uv0;
	vec2 uv1;
	vec3 p = { start[0] + offset_x, start[1], start[2] + offset_z };
	if (!paint_get_selection(mod, paint, p, &vertices[0], &vertices[1]))
		return;
	paint_get_combined_uv(paint, paint_get_tile_type(vertices), 
		tile, uv0, uv1);
	for (i = 0; i < COUNT_OF(vertices); i++) {
		struct ac_mesh_vertex * vtx = vertices[i];
		uint32_t j;
		vec2 extent_low = { 0.0f, 0.0f };
		vec2 extent_high = { 0.0f, 0.0f };
		vec2 extent = { 0.0f, 0.0f };
		if (!vtx)
			continue;
		extent_low[0] = vtx[0].position[0];
		extent_low[1] = vtx[0].position[2];
		extent_high[0] = vtx[0].position[0];
		extent_high[1] = vtx[0].position[2];
		for (j = 1; j < 3; j++) {
			struct ac_mesh_vertex * v = &vtx[j];
			if (v->position[0] < extent_low[0])
				extent_low[0] = v->position[0];
			if (v->position[2] < extent_low[1])
				extent_low[1] = v->position[2];
			if (v->position[0] > extent_high[0])
				extent_high[0] = v->position[0];
			if (v->position[2] > extent_high[1])
				extent_high[1] = v->position[2];
		}
		extent[0] = extent_high[0] - extent_low[0];
		extent[1] = extent_high[1] - extent_low[1];
		for (j = 0; j < 3; j++) {
			struct ac_mesh_vertex * v = &vtx[j];
			v->texcoord[0][0] = uv0[0] + 
				(uv1[0] - uv0[0]) * 
				(1.0f - ((extent_high[0] - v->position[0]) / 
					extent[0]));
			v->texcoord[0][1] = uv0[1] + 
				(uv1[1] - uv0[1]) * 
				(1.0f - ((extent_high[1] - v->position[2]) / 
					extent[1]));
		}
	}
}

static void paint_select(
	struct ae_terrain_module * mod,
	struct paint_tool * paint,
	const vec3 p)
{
	vec3 tri_point[2] = { 0 };
	vec3 start = { 0 };
	snap_to_triangle(tri_point[0], p, 1.0f);
	snap_to_triangle(tri_point[1], p, -1.0f);
	snaptoquad(start, p);
	paint_set_quad(mod, paint, start, -400.0f, -400.0f, 
		PAINT_TILE_TOP_LEFT);
	paint_set_quad(mod, paint, start, 0.0f, -400.0f, 
		PAINT_TILE_TOP);
	paint_set_quad(mod, paint, start, 400.0f, -400.0f, 
		PAINT_TILE_TOP_RIGHT);
	paint_set_quad(mod, paint, start, -400.0f, 0.0f, 
		PAINT_TILE_LEFT);
	paint_set_quad(mod, paint, start, 0.0f, 0.0f, 
		PAINT_TILE_CENTER);
	paint_set_quad(mod, paint, start, 400.0f, 0.0f, 
		PAINT_TILE_RIGHT);
	paint_set_quad(mod, paint, start, -400.0f, 400.0f, 
		PAINT_TILE_BOTTOM_LEFT);
	paint_set_quad(mod, paint, start, 0.0f, 400.0f, 
		PAINT_TILE_BOTTOM);
	paint_set_quad(mod, paint, start, 400.0f, 400.0f, 
		PAINT_TILE_BOTTOM_RIGHT);
}

static inline void bake_segments(
	struct region_tool * tool,
	struct ac_terrain_segment segments[16][16],
	uint32_t sector_offset_x,
	uint32_t sector_offset_z,
	uint32_t tex_len,
	uint8_t * tex_data)
{
	uint32_t x;
	for (x = 0; x < 16; x++) {
		uint32_t z;
		for (z = 0; z < 16; z++) {
			struct ac_terrain_segment * s = &segments[x][z];
			uint32_t index = (sector_offset_z + z) * tex_len + 
				sector_offset_x + x;
			struct color_info * ci = 
				&tool->colors[s->region_id];
			assert(index < tex_len * tex_len);
			if (s->region_id > AP_MAP_MAX_REGION_ID) {
				WARN("Segment has invalid terrain id (%u).",
					s->region_id);
				break;
			}
			if (ci->color_index == UINT32_MAX) {
				ci->color_index = pick_from_color_bucket(
					&tool->bucket);
				if (ci->color_index == UINT32_MAX) {
					/* Ran out of colors, leave this 
					 * segment as transparent. */
					break;
				}
			}
			((uint32_t *)tex_data)[index] = 0xFF000000 |
				tool->bucket.colors[ci->color_index];
			ci->refcount++;
		}
	}
}

static void rebuild_segment_textures(
	struct ae_terrain_module * mod,
	struct ac_terrain_sector ** sectors,
	uint32_t sector_count)
{
	struct region_tool * tool = &mod->region;
	vec2 b = { 0 };
	vec2 e = { 0 };
	boolean found[4] = { FALSE, FALSE, FALSE, FALSE };
	uint32_t i;
	uint32_t tex_len;
	uint8_t * tex_data;
	const bgfx_memory_t * mem;
	/* Find the extent of which the visible sectors cover. */
	for (i = 0; i < sector_count; i++) {
		struct ac_terrain_sector * s = sectors[i];
		if (!found[0] || s->extent_start[0] < b[0]) {
			b[0] = s->extent_start[0];
			found[0] = TRUE;
		}
		if (!found[1] || s->extent_start[1] < b[1]) {
			b[1] = s->extent_start[1];
			found[1] = TRUE;
		}
		if (!found[2] || s->extent_end[0] > e[0]) {
			e[0] = s->extent_end[0];
			found[2] = TRUE;
		}
		if (!found[3] || s->extent_end[1] > e[1]) {
			e[1] = s->extent_end[1];
			found[3] = TRUE;
		}
	}
	if (!found[0] || !found[1] || !found[2] || !found[3]) {
		WARN("Failed to find global sector extent.");
		return;
	}
	assert(e[0] > b[0] && e[1] > b[1]);
	glm_vec2_copy(b, tool->begin);
	tool->length = MAX(e[0] - b[0], e[1] - b[1]);
	tex_len = (uint32_t)(tool->length / AP_SECTOR_STEPSIZE);
	assert(0 != tex_len);
	tex_data = (uint8_t *)alloc(
		(size_t)tex_len * tex_len * 4);
	memset(tex_data, 0,
		(size_t)tex_len * tex_len * 4);
	/* Reset region color reference counts. */
	for (i = 0; i < COUNT_OF(mod->region.colors); i++)
		mod->region.colors[i].refcount = 0;
	for (i = 0; i < sector_count; i++) {
		struct ac_terrain_sector * s = sectors[i];
		uint32_t offset_u = 
			(uint32_t)((s->extent_start[0] - b[0]) / AP_SECTOR_STEPSIZE);
		uint32_t offset_v = 
			(uint32_t)((s->extent_start[1] - b[1]) / AP_SECTOR_STEPSIZE);
		if (!s->segment_info)
			continue;
		bake_segments(&mod->region, s->segment_info->segments, 
			offset_u, offset_v, 
			tex_len, tex_data);
	}
	for (i = 0; i < COUNT_OF(mod->region.colors); i++) {
		struct color_info * ci = &mod->region.colors[i];
		if (ci->color_index != UINT32_MAX && !ci->refcount) {
			/* No segment belongs to this region, 
			 * release color. */
			release_to_color_bucket(&mod->region.bucket,
				ci->color_index);
			ci->color_index = UINT32_MAX;
		}
	}
	mem = bgfx_copy(tex_data, (size_t)tex_len * tex_len * 4);
	if (BGFX_HANDLE_IS_VALID(mod->segment_tex[SEGMENT_TEX_REGION]))
		bgfx_destroy_texture(mod->segment_tex[SEGMENT_TEX_REGION]);
	mod->segment_tex[SEGMENT_TEX_REGION] = bgfx_create_texture_2d(
		tex_len, tex_len, false, 1,
		BGFX_TEXTURE_FORMAT_RGBA8, BGFX_SAMPLER_POINT, mem);
	if (!BGFX_HANDLE_IS_VALID(mod->segment_tex[SEGMENT_TEX_REGION])) {
		WARN("Failed to create segment texture.");
	}
	dealloc(tex_data);
}

static boolean cb_post_load_sector(
	struct ae_terrain_module * mod, 
	struct ac_terrain_cb_post_load_sector * data)
{
	rebuild_segment_textures(mod, data->sectors, data->sector_count);
	rebuild_tile_tex(mod, data->sectors, data->sector_count);
	return TRUE;
}

static boolean cb_segment_modification(
	struct ae_terrain_module * mod, 
	struct ac_terrain_cb_segment_modification * data)
{
	rebuild_segment_textures(mod, data->visible_sectors, data->visible_sector_count);
	rebuild_tile_tex(mod, data->visible_sectors, data->visible_sector_count);
	return TRUE;
}

static boolean cbcommitchanges(
	struct ae_terrain_module * mod, 
	void * data)
{
	ac_terrain_commit_changes(mod->ac_terrain);
	return TRUE;
}

static boolean onregister(
	struct ae_terrain_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_map, AP_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_mesh, AC_MESH_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_render, AC_RENDER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_terrain, AC_TERRAIN_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_texture, AC_TEXTURE_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ae_editor_action, AE_EDITOR_ACTION_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ae_texture, AE_TEXTURE_MODULE_NAME);
	ac_terrain_add_callback(mod->ac_terrain, AC_TERRAIN_CB_POST_LOAD_SECTOR, mod, (ap_module_default_t)cb_post_load_sector);
	ac_terrain_add_callback(mod->ac_terrain, AC_TERRAIN_CB_SEGMENT_MODIFICATION, mod, (ap_module_default_t)cb_segment_modification);
	mod->region.length = ac_terrain_get_view_distance(mod->ac_terrain) * 2.0f;
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_COMMIT_CHANGES, mod, (ap_module_default_t)cbcommitchanges);
	return TRUE;
}

boolean oninitialize(struct ae_terrain_module * mod)
{
	uint32_t i;
	mod->paint.icon = ac_texture_load(mod->ac_texture,
		"content/textures/tex-paint.png", NULL);
	if (!BGFX_HANDLE_IS_VALID(mod->paint.icon)) {
		ERROR("Failed to load paint icon.");
		return FALSE;
	}
	mod->placeholder_tex = ac_texture_load(mod->ac_texture, 
		"content/textures/na2.png", NULL);
	if (!BGFX_HANDLE_IS_VALID(mod->placeholder_tex)) {
		ERROR("Failed to load placeholder texture.");
		return FALSE;
	}
	if (!create_shaders(mod)) {
		ERROR("Failed to create shaders.");
		return FALSE;
	}
	calc_max_triangle_count(&mod->paint);
	strcpy(mod->paint.tex_alpha_name, "A0001C0T");
	mod->paint.tex_alpha = ac_texture_load_packed(mod->ac_texture,
		AC_DAT_DIR_TEX_WORLD, mod->paint.tex_alpha_name, TRUE, NULL);
	if (!BGFX_HANDLE_IS_VALID(mod->paint.tex_alpha)) {
		ERROR("Failed to load alpha texture.");
		return FALSE;
	}
	mod->height.icon = ac_texture_load(mod->ac_texture,
		"content/textures/height-tool2.png", NULL);
	if (!BGFX_HANDLE_IS_VALID(mod->height.icon)) {
		ERROR("Failed to load height tool icon.");
		return FALSE;
	}
	mod->level.icon = ac_texture_load(mod->ac_texture,
		"content/textures/level-tool2.png", NULL);
	if (!BGFX_HANDLE_IS_VALID(mod->level.icon)) {
		ERROR("Failed to load level tool icon.");
		return FALSE;
	}
	for (i = 0; i < COUNT_OF(mod->interp_tex); i++) {
		const char * path[] = {
			"content/textures/interp-constant.png",
			"content/textures/interp-linear.png",
			"content/textures/interp-sharp.png",
			"content/textures/interp-root.png",
			"content/textures/interp-sphere.png",
			"content/textures/interp-smooth.png" };
		mod->interp_tex[i] = ac_texture_load(mod->ac_texture, path[i], 
			&mod->interp_info[i]);
		if (!BGFX_HANDLE_IS_VALID(mod->interp_tex[i])) {
			ERROR("Failed to load fallof icons.");
			return FALSE;
		}
	}
	if (!init_tile_tool(mod)) {
		ERROR("Failed to initialize tile edit tool.");
		return FALSE;
	}
	return TRUE;
}

static void clearpaintselection(struct ae_terrain_module * mod)
{
	uint32_t i;
	uint32_t count = vec_count(mod->paint.materials);
	for (i = 0; i < count; i++)
		ac_mesh_release_material(mod->ac_mesh, &mod->paint.materials[i]);
	vec_clear(mod->paint.materials);
	vec_clear(mod->paint.vertices);
}

static void onclose(struct ae_terrain_module * mod)
{
	clearpaintselection(mod);
}

static void onshutdown(struct ae_terrain_module * mod)
{
	uint32_t i;
	for (i = 0; i < COUNT_OF(mod->interp_tex); i++)
		ac_texture_release(mod->ac_texture, mod->interp_tex[i]);
	ac_texture_release(mod->ac_texture, mod->placeholder_tex);
	ac_texture_release(mod->ac_texture, mod->paint.icon);
	ac_texture_release(mod->ac_texture, mod->paint.tex);
	ac_texture_release(mod->ac_texture, mod->paint.tex_alpha);
	vec_free(mod->paint.vertices);
	vec_free(mod->paint.materials);
	bgfx_destroy_program(mod->paint.program);
	bgfx_destroy_program(mod->paint.program_grid);
	bgfx_destroy_uniform(mod->paint.sampler);
	ac_texture_release(mod->ac_texture, mod->height.icon);
	bgfx_destroy_program(mod->height.program);
	bgfx_destroy_uniform(mod->height.uniform);
	ac_texture_release(mod->ac_texture, mod->level.icon);
	bgfx_destroy_program(mod->level.program);
	bgfx_destroy_uniform(mod->level.uniform);
	bgfx_destroy_program(mod->region.program);
	bgfx_destroy_uniform(mod->region.uniform);
	bgfx_destroy_uniform(mod->region.sampler);
	for (i = 0; i < COUNT_OF(mod->segment_tex); i++) {
		if (BGFX_HANDLE_IS_VALID(mod->segment_tex[i]))
			bgfx_destroy_texture(mod->segment_tex[i]);
	}
	destroy_tile_tool(mod);
}

struct ae_terrain_module * ae_terrain_create_module()
{
	struct ae_terrain_module  * mod = (struct ae_terrain_module *)ap_module_instance_new(
		AE_TERRAIN_MODULE_NAME, sizeof(*mod), 
		(ap_module_instance_register_t)onregister, 
		(ap_module_instance_initialize_t)oninitialize, 
		(ap_module_instance_close_t)onclose, 
		(ap_module_instance_shutdown_t)onshutdown);
	uint32_t i;
	mod->edit_mode = EDIT_MODE_NONE;
	BGFX_INVALIDATE_HANDLE(mod->paint.icon);
	mod->paint.tex_browser = UINT32_MAX;
	BGFX_INVALIDATE_HANDLE(mod->paint.tex);
	mod->paint.scale_tex = 1;
	mod->paint.vertices = (struct ac_mesh_vertex *)vec_new_reserved(
		sizeof(*mod->paint.vertices), 128);
	mod->paint.materials = (struct ac_mesh_material *)vec_new_reserved(
		sizeof(*mod->paint.materials), 128);
	paint_setup_tile_combinations(&mod->paint);
	mod->height.size = 10.0f;
	mod->height.strength = 1.0f;
	mod->height.falloff = INTERP_SMOOTH;
	mod->level.size = 10.0f;
	mod->level.strength = 1.0f;
	mod->level.falloff = INTERP_SMOOTH;
	mod->region.opacity = 0.1f;
	mod->region.size = 1;
	for (i = 0; i < COUNT_OF(mod->segment_tex); i++)
		BGFX_INVALIDATE_HANDLE(mod->segment_tex[i]);
	init_color_bucket(&mod->region.bucket);
	for (i = 0; i < COUNT_OF(mod->region.colors); i++)
		mod->region.colors[i].color_index = UINT32_MAX;
	return mod;
}

void ae_terrain_update(struct ae_terrain_module * mod, float dt)
{
	switch (mod->edit_mode) {
	case EDIT_MODE_HEIGHT:
		if (mod->height.preview &&
			ac_render_button_down(mod->ac_render, AC_RENDER_BUTTON_LEFT)) {
			vec3 pos = { mod->height.center[0], 0.0f,
				mod->height.center[1] };
			ac_terrain_adjust_height(mod->ac_terrain, pos, 
				mod->height.size * HEIGHT_TOOL_SIZE_SCALE, 
				dt * mod->height.strength * 1000.0f,
				mod->height.falloff);
		}
		break;
	case EDIT_MODE_LEVEL:
		if (mod->level.preview &&
			ac_render_button_down(mod->ac_render, AC_RENDER_BUTTON_LEFT)) {
			vec3 pos = { mod->level.center[0], 0.0f,
				mod->level.center[1] };
			ac_terrain_level(mod->ac_terrain, pos, 
				mod->level.size * LEVEL_TOOL_SIZE_SCALE, 
				dt * mod->level.strength * 100.0f,
				mod->level.falloff);
		}
		break;
	}
}

static boolean cbcustomrenderpaint(
	struct ae_terrain_module * mod, 
	struct ac_terrain_cb_custom_render * cb)
{
	cb->state = BGFX_STATE_WRITE_RGB |
		BGFX_STATE_DEPTH_TEST_LEQUAL |
		BGFX_STATE_CULL_CW |
		BGFX_STATE_BLEND_FUNC_SEPARATE(
			BGFX_STATE_BLEND_SRC_ALPHA,
			BGFX_STATE_BLEND_INV_SRC_ALPHA,
			BGFX_STATE_BLEND_ZERO,
			BGFX_STATE_BLEND_ONE);
	cb->program = mod->paint.program_grid;
	return TRUE;
}

static boolean cbcustomrenderheight(
	struct ae_terrain_module * mod, 
	struct ac_terrain_cb_custom_render * cb)
{
	vec4 uniform = {
		mod->height.center[0], mod->height.center[1],
		mod->height.size * HEIGHT_TOOL_SIZE_SCALE, 0.0f };
	if (!mod->height.preview)
		uniform[2] = 0.0f;
	cb->state = BGFX_STATE_WRITE_RGB |
		BGFX_STATE_DEPTH_TEST_LEQUAL |
		BGFX_STATE_CULL_CW |
		BGFX_STATE_BLEND_FUNC_SEPARATE(
			BGFX_STATE_BLEND_SRC_ALPHA,
			BGFX_STATE_BLEND_INV_SRC_ALPHA,
			BGFX_STATE_BLEND_ZERO,
			BGFX_STATE_BLEND_ONE);
	cb->program = mod->height.program;
	bgfx_set_uniform(mod->height.uniform, uniform, 1);
	return TRUE;
}

static boolean cbcustomrenderlevel(
	struct ae_terrain_module * mod, 
	struct ac_terrain_cb_custom_render * cb)
{
	vec4 uniform = {
		mod->level.center[0], mod->level.center[1],
		mod->level.size * LEVEL_TOOL_SIZE_SCALE, 0.0f };
	if (!mod->level.preview)
		uniform[2] = 0.0f;
	cb->state = BGFX_STATE_WRITE_RGB |
		BGFX_STATE_DEPTH_TEST_LEQUAL |
		BGFX_STATE_CULL_CW |
		BGFX_STATE_BLEND_FUNC_SEPARATE(
			BGFX_STATE_BLEND_SRC_ALPHA,
			BGFX_STATE_BLEND_INV_SRC_ALPHA,
			BGFX_STATE_BLEND_ZERO,
			BGFX_STATE_BLEND_ONE);
	cb->program = mod->level.program;
	bgfx_set_uniform(mod->level.uniform, uniform, 1);
	return TRUE;
}

static boolean cbcustomrenderregion(
	struct ae_terrain_module * mod, 
	struct ac_terrain_cb_custom_render * cb)
{
	vec4 uniform[2] = {
		{ mod->region.begin[0], mod->region.begin[1], mod->region.length, 0.0f },
		{ mod->region.opacity, 0.0f, 0.0f, 0.0f } };
	cb->state = BGFX_STATE_WRITE_RGB |
		BGFX_STATE_DEPTH_TEST_LEQUAL |
		BGFX_STATE_CULL_CW |
		BGFX_STATE_BLEND_FUNC_SEPARATE(
			BGFX_STATE_BLEND_SRC_ALPHA,
			BGFX_STATE_BLEND_INV_SRC_ALPHA,
			BGFX_STATE_BLEND_ZERO,
			BGFX_STATE_BLEND_ONE);
	cb->program = mod->region.program;
	bgfx_set_uniform(mod->region.uniform, uniform, 2);
	bgfx_set_texture(0, mod->region.sampler,
		mod->segment_tex[SEGMENT_TEX_REGION], UINT32_MAX);
	return TRUE;
}

void ae_terrain_render(struct ae_terrain_module * mod, struct ac_camera * cam)
{
	switch (mod->edit_mode) {
	case EDIT_MODE_PAINT: {
		mat4 model;
		uint64_t state = BGFX_STATE_WRITE_RGB |
			BGFX_STATE_DEPTH_TEST_LEQUAL |
			BGFX_STATE_CULL_CW |
			BGFX_STATE_BLEND_FUNC_SEPARATE(
				BGFX_STATE_BLEND_SRC_ALPHA,
				BGFX_STATE_BLEND_INV_SRC_ALPHA,
				BGFX_STATE_BLEND_ZERO,
				BGFX_STATE_BLEND_ONE);
		bgfx_transient_vertex_buffer_t tvb;
		bgfx_transient_index_buffer_t tib;
		bgfx_vertex_layout_t layout;
		uint32_t i;
		struct paint_tool * p = &mod->paint;
		uint32_t count;
		ac_terrain_custom_render(mod->ac_terrain, mod, 
			(ap_module_default_t)cbcustomrenderpaint);
		count = vec_count(p->vertices);
		if (!count)
			break;
		layout = ac_mesh_vertex_layout();
		if (!bgfx_alloc_transient_buffers(&tvb, &layout,
			count, &tib,
			count, true)) {
			ERROR("Failed to allocate transient buffers.");
			calc_max_triangle_count(p);
			clearpaintselection(mod);
			break;
		}
		memcpy(tvb.data, p->vertices,
			count * sizeof(*p->vertices));
		for (i = 0; i < count; i++)
			((uint32_t *)tib.data)[i] = i;
		glm_mat4_identity(model);
		bgfx_set_transform(&model, 1);
		bgfx_set_transient_vertex_buffer(0, &tvb, 0, count);
		bgfx_set_transient_index_buffer(&tib, 0, count);
		bgfx_set_state(state, 0xffffffff);
		bgfx_set_texture(0, p->sampler, p->tex_alpha, UINT32_MAX);
		bgfx_submit(0, mod->paint.program, 0, 0xff);
		break;
	}
	case EDIT_MODE_HEIGHT: {
		ac_terrain_custom_render(mod->ac_terrain, mod, 
			(ap_module_default_t)cbcustomrenderheight);
		break;
	}
	case EDIT_MODE_LEVEL: {
		ac_terrain_custom_render(mod->ac_terrain, mod, 
			(ap_module_default_t)cbcustomrenderlevel);
		break;
	}
	case EDIT_MODE_REGION: {
		ac_terrain_custom_render(mod->ac_terrain, mod, 
			(ap_module_default_t)cbcustomrenderregion);
		break;
	}
	case EDIT_MODE_TILE:
		render_tile(mod);
		break;
	}
}

void ae_terrain_on_mdown(
	struct ae_terrain_module * mod,
	struct ac_camera*  cam,
	int mouse_x,
	int mouse_y)
{
	vec3 point;
	if (!raycast_terrain(mod, cam, mouse_x, mouse_y, point))
		return;
	switch (mod->edit_mode) {
	case EDIT_MODE_PAINT:
		paint_select(mod, &mod->paint, point);
		break;
	case EDIT_MODE_REGION: {
		struct region_tool * t = &mod->region;
		if (t->selected)
			ac_terrain_set_region_id(mod->ac_terrain, point, t->size, t->selected->id);
		break;
	}
	case EDIT_MODE_TILE:
		on_mdown_tile(mod, point);
		break;
	}
}

boolean ae_terrain_on_mmove(
	struct ae_terrain_module * mod,
	struct ac_camera*  cam,
	int mouse_x,
	int mouse_y)
{
	vec3 point;
	boolean hit = raycast_terrain(mod, cam, 
		mouse_x, mouse_y, point);
	switch (mod->edit_mode) {
	case EDIT_MODE_PAINT:
		if (hit && ac_render_button_down(mod->ac_render, AC_RENDER_BUTTON_LEFT)) {
			paint_select(mod, &mod->paint, point);
			return TRUE;
		}
		break;
	case EDIT_MODE_HEIGHT:
		if (hit) {
			mod->height.preview = TRUE;
			mod->height.center[0] = point[0];
			mod->height.center[1] = point[2];
			return TRUE;
		}
		else {
			mod->height.preview = FALSE;
		}
		break;
	case EDIT_MODE_LEVEL:
		if (hit) {
			mod->level.preview = TRUE;
			mod->level.center[0] = point[0];
			mod->level.center[1] = point[2];
			return TRUE;
		}
		else {
			mod->level.preview = FALSE;
		}
		break;
	case EDIT_MODE_REGION: {
		struct region_tool * t = &mod->region;
		if (hit && 
			t->selected && 
			ac_render_button_down(mod->ac_render, AC_RENDER_BUTTON_LEFT)) {
			ac_terrain_set_region_id(mod->ac_terrain, point, t->size, t->selected->id);
			return TRUE;
		}
		break;
	}
	case EDIT_MODE_TILE:
		if (hit && ac_render_button_down(mod->ac_render, AC_RENDER_BUTTON_LEFT)) {
			on_mdown_tile(mod, point);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

boolean ae_terrain_on_mwheel(struct ae_terrain_module * mod, float delta)
{
	switch (mod->edit_mode) {
	case EDIT_MODE_PAINT:
		break;
	case EDIT_MODE_HEIGHT:
		if (ac_render_get_key_state(mod->ac_render)[SDL_SCANCODE_LSHIFT]) {
			if (delta < 0.f) {
				if (mod->height.size > 1.0f)
					mod->height.size -= 1.0f;
			}
			else {
				mod->height.size += 1.0f;
			}
			return TRUE;
		}
		break;
	case EDIT_MODE_LEVEL:
		if (ac_render_get_key_state(mod->ac_render)[SDL_SCANCODE_LSHIFT]) {
			if (delta < 0.f) {
				if (mod->level.size > 1.0f)
					mod->level.size -= 1.0f;
			}
			else {
				mod->level.size += 1.0f;
			}
			return TRUE;
		}
		break;
	}
	return FALSE;
}

boolean ae_terrain_on_key_down(struct ae_terrain_module * mod, uint32_t keycode)
{
	return FALSE;
}

static const char * paint_layer_tag(enum paint_layer l)
{
	switch (l) {
	case PAINT_LAYER_BASE:
		return "Base Layer";
	case PAINT_LAYER_0:
		return "Layer 1";
	case PAINT_LAYER_1:
		return "Layer 2";
	default:
		return "Invalid";
	}
}

static const char * falloff_tag(uint32_t f)
{
	switch (f) {
	case INTERP_CONSTANT:
		return "Constant";
	case INTERP_LINEAR:
		return "Linear";
	case INTERP_SHARP:
		return "Sharp";
	case INTERP_ROOT:
		return "Root";
	case INTERP_SPHERE:
		return "Sphere";
	case INTERP_SMOOTH:
		return "Smooth";
	default:
		return "Invalid";
	}
}

void reset_button_states(
	struct ae_terrain_module * mod,
	enum edit_mode exception)
{
	uint32_t i;
	for (i = 0; i < COUNT_OF(mod->button_states); i++) {
		if (i != exception)
			mod->button_states[i] = FALSE;
	}
}

bool tool_button(
	bgfx_texture_handle_t tex,
	const bgfx_texture_info_t * info,
	float box_size,
	const char * hint,
	boolean * btn_state,
	boolean is_disabled)
{
    ImGuiWindow * window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;
	ImVec2 size = ImVec2(0, 0);
	if (info && box_size != 0.0f) {
		float hscale = box_size / info->width;
		float vscale = box_size / info->height;
		float scale = MIN(hscale, vscale);
		ImVec2 cur = ImGui::GetCursorPos();
		size = ImVec2(info->width * scale, info->height * scale);
		cur = ImVec2(cur.x + ((box_size - size.x) / 2.0f),
			cur.y + ((box_size - size.y) / 2.0f));
		ImGui::SetCursorPos(cur);
	}
	else {
		size = ImVec2(box_size, box_size);
	}
	const ImVec2 padding = ImVec2(6.0f, 6.0f);
    size = ImGui::CalcItemSize(size, 40, 40);
    ImRect bb(window->DC.CursorPos - padding, 
		window->DC.CursorPos + size + padding);
    const ImU32 col = ImGui::GetColorU32(ImGuiCol_ButtonHovered,
		0.4f);
    if (!is_disabled && *btn_state) {
        window->DrawList->AddRectFilled(bb.Min, 
			bb.Max, col);
	}
	ImVec4 tint = is_disabled ? ImVec4(.8f, .8f, .8f, .8f) : 
		ImVec4(1.f, 1.f, 1.f, 1.f);
	ImGui::Image((ImTextureID)tex.idx, size,
		ImVec2(0, 0), ImVec2(1, 1), tint);
	if (hint && ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Text(hint);
		ImGui::EndTooltip();
	}
	boolean clicked = !is_disabled && ImGui::IsItemClicked();
	if (clicked && btn_state)
		*btn_state ^= 1;
	return clicked;
}

void falloff_controls(
	struct ae_terrain_module * mod,
	enum interp_type * falloff)
{
	uint32_t i;
	ImVec2 avail_box = ImGui::GetContentRegionAvail();
	float avail = MIN(avail_box.x, avail_box.y);
	for (i = 0; i < COUNT_OF(mod->interp_tex); i++) {
		boolean state = (*falloff == i);
		ImGui::SameLine();
		if (tool_button(mod->interp_tex[i], &mod->interp_info[i],
				avail, falloff_tag(i), &state, FALSE)) {
			*falloff = (enum interp_type)i;
		}
	}
	ImGui::SameLine();
	ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), 
		ImGui::GetCursorPosY() + 
		((avail_box.y - ImGui::CalcTextSize("Falloff").y) / 2.0f)));
	ImGui::Text("Falloff");
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
}

static void replacetexture(
	struct ae_terrain_module * mod,
	struct ac_mesh_material * material,
	uint32_t index,
	const char * tex_name,
	bgfx_texture_handle_t tex)
{
	if (BGFX_HANDLE_IS_VALID(material->tex_handle[index]))
		ac_texture_release(mod->ac_texture, material->tex_handle[index]);
	if (BGFX_HANDLE_IS_VALID(tex)) {
		ac_texture_copy(mod->ac_texture, tex);
		material->tex_handle[index] = tex;
		strlcpy(material->tex_name[index], tex_name, sizeof(material->tex_name[index]));
	}
	else {
		material->tex_handle[index] = BGFX_INVALID_HANDLE;
		memset(material->tex_name[index], 0, sizeof(material->tex_name[index]));
	}
}

void ae_terrain_toolbar(struct ae_terrain_module * mod)
{
	float toolkit_height = ImGui::GetContentRegionAvail().y;
	switch (mod->edit_mode) {
	case EDIT_MODE_PAINT: {
		uint16_t tex_id = BGFX_HANDLE_IS_VALID(mod->paint.tex) ?
			mod->paint.tex.idx : mod->placeholder_tex.idx;
		ImGui::SameLine();
		if (ImGui::ImageButton((ImTextureID)tex_id,
				ImVec2(toolkit_height - 4.f, toolkit_height - 4.f),
				ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), 2,
				ImVec4(0.f, 0.f, 0.f, 0.f),
				ImVec4(1.f, 1.f, 1.f, 1.f)) &&
			mod->paint.tex_browser == UINT32_MAX) {
			mod->paint.tex_browser = ae_texture_browse(mod->ae_texture,
				AC_DAT_DIR_TEX_WORLD, "Select Texture");
		}
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
			ac_texture_release(mod->ac_texture, mod->paint.tex);
			mod->paint.tex = BGFX_INVALID_HANDLE;
			memset(mod->paint.tex_name, 0,
				sizeof(mod->paint.tex_name));
		}
		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
			ImVec2(4.f, 5.f));
		ImGui::SetNextItemWidth(120.f);
		if (ImGui::BeginCombo("##PaintLayer",
				paint_layer_tag(mod->paint.layer), 0)) {
			const enum paint_layer layers[] = {
				PAINT_LAYER_BASE,
				PAINT_LAYER_0,
				PAINT_LAYER_1 };
			uint32_t i;
			for (i = 0; i < COUNT_OF(layers); i++) {
				if (ImGui::Selectable(paint_layer_tag(layers[i]),
						mod->paint.layer == layers[i],
						0, ImVec2( 0.f, 0.f )))
					mod->paint.layer = layers[i];
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.f);
		if (ImGui::DragScalar("Texture To Node Ratio",
				ImGuiDataType_U32, &mod->paint.scale_tex, 0.05f, 
				NULL, NULL, "%u", 0) &&
			mod->paint.scale_tex == 0) {
			mod->paint.scale_tex = 1;
		}
		ImGui::SameLine();
		if (ImGui::Button("Apply")) {
			uint32_t i;
			uint32_t count = vec_count(mod->paint.materials);
			switch (mod->paint.layer) {
			case PAINT_LAYER_BASE:
				for (i = 0; i < count; i++) {
					replacetexture(mod, &mod->paint.materials[i], 0, 
						mod->paint.tex_name, mod->paint.tex);
				}
				ac_terrain_set_base(mod->ac_terrain, vec_count(mod->paint.materials),
					mod->paint.scale_tex, mod->paint.vertices, mod->paint.materials);
				break;
			case PAINT_LAYER_0:
				for (i = 0; i < count; i++) {
					if (BGFX_HANDLE_IS_VALID(mod->paint.tex)) {
						replacetexture(mod, &mod->paint.materials[i], 1, 
							mod->paint.tex_alpha_name, mod->paint.tex_alpha);
						replacetexture(mod, &mod->paint.materials[i], 2, 
							mod->paint.tex_name, mod->paint.tex);
					}
					else {
						replacetexture(mod, &mod->paint.materials[i], 1, 
							NULL, BGFX_INVALID_HANDLE);
						replacetexture(mod, &mod->paint.materials[i], 2, 
							NULL, BGFX_INVALID_HANDLE);
					}
				}
				ac_terrain_set_layer(mod->ac_terrain, 0,
					vec_count(mod->paint.materials), mod->paint.scale_tex,
					mod->paint.vertices, mod->paint.materials);
				break;
			case PAINT_LAYER_1:
				for (i = 0; i < count; i++) {
					if (BGFX_HANDLE_IS_VALID(mod->paint.tex)) {
						replacetexture(mod, &mod->paint.materials[i], 3, 
							mod->paint.tex_alpha_name, mod->paint.tex_alpha);
						replacetexture(mod, &mod->paint.materials[i], 4, 
							mod->paint.tex_name, mod->paint.tex);
					}
					else {
						replacetexture(mod, &mod->paint.materials[i], 3, 
							NULL, BGFX_INVALID_HANDLE);
						replacetexture(mod, &mod->paint.materials[i], 4, 
							NULL, BGFX_INVALID_HANDLE);
					}
				}
				ac_terrain_set_layer(mod->ac_terrain, 1,
					vec_count(mod->paint.materials), mod->paint.scale_tex,
					mod->paint.vertices, mod->paint.materials);
				break;
			}
			clearpaintselection(mod);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
			clearpaintselection(mod);
		ImGui::PopStyleVar(1);
		break;
	}
	case EDIT_MODE_HEIGHT: {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
			ImVec2( 4.f, 5.f ));
		falloff_controls(mod, &mod->height.falloff);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.f);
		ImGui::DragFloat("Radius",
			&mod->height.size,
			0.01f, 1.0f, 1000.0f, "%g", 0);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.f);
		ImGui::DragFloat("Strength",
			&mod->height.strength,
			0.01f, -1000.0f, 1000.0f, "%g", 0);
		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::PopStyleVar(1);
		break;
	}
	case EDIT_MODE_LEVEL: {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
			ImVec2( 4.f, 5.f ));
		falloff_controls(mod, &mod->level.falloff);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.f);
		ImGui::DragFloat("Radius",
			&mod->height.size,
			0.01f, 1.0f, 1000.0f, "%g", 0);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.f);
		ImGui::DragFloat("Strength",
			&mod->level.strength,
			0.01f, 1.0f, 1000.0f, "%g", 0);
		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::PopStyleVar(1);
		break;
	}
	case EDIT_MODE_REGION: {
		struct region_tool * tool = &mod->region;
		char label[256] = "Not Selected";
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
			ImVec2( 4.f, 5.f ));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.f);
		ImGui::DragFloat("Opacity",
			&mod->region.opacity,
			0.001f, 0.0f, 1.0f, "%g", 0);
		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(160.f);
		if (tool->selected) {
			snprintf(label, sizeof(label), "[%u] %s",
				tool->selected->id, tool->selected->glossary);
		}
		if (ImGui::BeginCombo("Region", label)) {
			uint32_t i;
			ImGui::Text("Visible Regions");
			ImGui::Separator();
			for (i = 0; i < COUNT_OF(tool->colors); i++) {
				if (tool->colors[i].refcount) {
					ImVec4 c = ImGui::ColorConvertU32ToFloat4(
						tool->bucket.colors[tool->colors[i].color_index]);
					struct ap_map_region_template * t = 
						ap_map_get_region_template(mod->ap_map, i);
					c.w = 1.0f;
					snprintf(label, sizeof(label), "##CT%u", i);
					ImGui::ColorButton(label, c, 
						ImGuiColorEditFlags_NoTooltip);
					if (t) {
						snprintf(label, sizeof(label), "[%03u] %s",
							i, t->glossary);
					}
					else {
						snprintf(label, sizeof(label),
							"[%03u] Invalid Region", i);
					}
					ImGui::SameLine();
					if (ImGui::Selectable(label, t && t == tool->selected) &&
						t) {
						tool->selected = t;
					}
				}
			}
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			ImGui::Text("Non-visible Regions");
			ImGui::Separator();
			for (i = 0; i < COUNT_OF(tool->colors); i++) {
				if (!tool->colors[i].refcount) {
					struct ap_map_region_template * t = 
						ap_map_get_region_template(mod->ap_map, i);
					if (!t)
						continue;
					snprintf(label, sizeof(label), "##CT%u", i);
					if (t) {
						snprintf(label, sizeof(label), "[%03u] %s",
							i, t->glossary);
					}
					else {
						snprintf(label, sizeof(label),
							"[%03u] Invalid Region", i);
					}
					if (ImGui::Selectable(label, t && t == tool->selected) &&
						t) {
						tool->selected = t;
					}
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.f);
		if (ImGui::DragScalar("Size", ImGuiDataType_U32, 
				&tool->size, 0.15f) && tool->size < 1) {
			tool->size = 1;
		}
		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::PopStyleVar(1);
		break;
	}
	case EDIT_MODE_TILE:
		render_tile_toolbar(mod);
		break;
	}
}

void ae_terrain_toolbox(struct ae_terrain_module * mod)
{
	ImVec2 avail_box = ImGui::GetContentRegionAvail();
	float avail = MIN(avail_box.x, avail_box.y);
	bgfx_texture_handle_t icons[] = {
		BGFX_INVALID_HANDLE,
		mod->paint.icon,
		mod->height.icon,
		mod->level.icon,
		mod->level.icon,
		mod->level.icon };
	const char * tooltip[] = {
		"None",
		"Paint Paint Tool",
		"Height Tool",
		"Level Tool",
		"Region Edit Tool",
		"Tile Edit Tool" };
	boolean disabled[] = {
		FALSE,
		FALSE,
		FALSE,
		FALSE,
		FALSE,
		FALSE };
	uint32_t i;
	for (i = 0; i < COUNT_OF(icons); i++) {
		if (!BGFX_HANDLE_IS_VALID(icons[i]))
			continue;
		if (tool_button(icons[i], NULL, avail, tooltip[i],
				&mod->button_states[i], disabled[i])) {
			if (mod->button_states[i]) {
				mod->edit_mode = (enum edit_mode)i;
				reset_button_states(mod, (enum edit_mode)i);
			}
			else {
				mod->edit_mode = EDIT_MODE_NONE;
			}
		}
		ImGui::Separator();
	}
}

void ae_terrain_imgui(struct ae_terrain_module * mod)
{
	bgfx_texture_handle_t paint_tex = BGFX_INVALID_HANDLE;
	if (mod->paint.tex_browser != UINT32_MAX &&
		ae_texture_do_browser(mod->ae_texture, mod->paint.tex_browser, &paint_tex)) {
		/* Browser is closed, do not retain handle. */
		mod->paint.tex_browser = UINT32_MAX;
		ac_texture_release(mod->ac_texture, mod->paint.tex);
		mod->paint.tex = paint_tex;
		if (BGFX_HANDLE_IS_VALID(paint_tex)) {
			ac_texture_get_name(mod->ac_texture, paint_tex, TRUE,
				mod->paint.tex_name, sizeof(mod->paint.tex_name));
		}
		else {
			memset(mod->paint.tex_name, 0, sizeof(mod->paint.tex_name));
		}
	}
}

