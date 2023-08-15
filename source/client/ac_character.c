#include "client/ac_character.h"

#include "core/bin_stream.h"
#include "core/file_system.h"
#include "core/log.h"
#include "core/string.h"

#include "public/ap_character.h"

#include "utility/au_table.h"

#include "client/ac_dat.h"
#include "client/ac_mesh.h"
#include "client/ac_render.h"
#include "client/ac_renderware.h"
#include "client/ac_texture.h"

#include <assert.h>

#define INI_NAME_LABEL "LABEL"
#define INI_NAME_DFF "DFF"
#define INI_NAME_DEF_ARM_DFF "DEFAULT_ARMOUR_DFF"
#define INI_NAME_ANIMATION_NAME "ANIMATION_NAME"
#define INI_NAME_BLENDING_ANIMATION_NAME "B_ANIMATION_NAME"
#define INI_NAME_ANIMATION_FLAGS "ANIMATION_FLAGS"
#define INI_NAME_ANIMATION_ACTIVE_RATE "ANIMATION_ACTIVE_RATE"
#define INI_NAME_ANIMATION_CUST_TYPE "ANIMATION_CUST_TYPE"
#define INI_NAME_ANIMATION_POINT "ANIMATION_POINT"
#define INI_NAME_ANIMATION_CUST_FLAGS "ANIMATION_CUST_FLAGS"
#define INI_NAME_SUB_ANIMATION_NAME "SUB_ANIMATION_NAME"
#define INI_NAME_ANIM_TYPE2 "ANIMATION_TYPE2"
#define INI_NAME_LOD_LEVEL "LOD_LEVEL"
#define INI_NAME_LOD_DISTANCE "LOD_DISTANCE"
#define INI_NAME_LOD_HAS_BILLBOARD_NUM "LOD_HAS_BILLBOARD"
#define INI_NAME_LOD_BILLBOARD_INFO "LOD_BILLBOARD_INFO"
#define INI_NAME_HEIGHT "HEIGHT"
#define INI_NAME_DEPTH "DEPTH"
#define INI_NAME_RIDER_HEIGHT "RIDER_HEIGHT"
#define INI_NAME_PICK_DFF "PICK_DFF"
#define INI_NAME_PRE_LIGHT "PRE_LIGHT"
#define INI_NAME_OBJECT_TYPE "OBJECT_TYPE"
#define INI_NAME_BOUNDING_SPHERE "BOUNDING_SPHERE"
#define INI_NAME_SCALE "SCALE"
#define INI_NAME_PICKNODE "PICKNODE"
#define INI_NAME_PICKNODE_2 "PICKNODE_2"
#define INI_NAME_PICKNODE_3 "PICKNODE_3"
#define INI_NAME_PICKNODE_4 "PICKNODE_4"
#define INI_NAME_PICKNODE_5 "PICKNODE_5"
#define INI_NAME_PICKNODE_6 "PICKNODE_6"
#define INI_NAME_TAGGING "TAGGING"
#define INI_NAME_DNF_1 "DID_NOT_FINISH_KOREA"
#define INI_NAME_DNF_2 "DID_NOT_FINISH_CHINA"
#define INI_NAME_DNF_3 "DID_NOT_FINISH_WESTERN"
#define INI_NAME_DNF_4 "DID_NOT_FINISH_JAPAN"
#define INI_NAME_DNF_5 "DID_NOT_FINISH_TAIWAN"
#define INI_NAME_LOOKAT_NODE "LOOK_AT_NODE"
#define INI_NAME_USE_BENDING "USE_BENDING"
#define INI_NAME_BENDING_FACTOR "BENDING_FACTOR"
#define INI_NON_PICKING_TYPE "NON_PICKING"
#define INI_NAME_FACEATOMIC "FACEATOMIC"
#define INI_NAME_OCTREE_DATA "OCTREE_DATA"
#define INI_NAME_DEFAULT_FACE_NUM "DEFAULT_FACE_NUM"
#define INI_NAME_DEFAULT_FACE "DEFAULT_FACE"
#define INI_NAME_DEFAULT_HAIR_NUM "DEFAULT_HAIR_NUM"
#define INI_NAME_DEFAULT_HAIR "DEFAULT_HAIR"
#define INI_NAME_DEFAULT_FACE_RENDER_TYPE "DEFAULT_RENDER_TYPE_FACE"
#define INI_NAME_DEFAULT_HAIR_RENDER_TYPE "DEFAULT_RENDER_TYPE_HAIR"
#define INI_NAME_CUSTOMIZE_REVIEW "CUSTOMIZE_REVIEW"

size_t AC_CHARACTER_ATTACHMENT_OFFSET = SIZE_MAX;
size_t AC_CHARACTER_TEMPLATE_ATTACHMENT_OFFSET = SIZE_MAX;

struct ac_character_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ac_dat_module * ac_dat;
	struct ac_lod_module * ac_lod;
	struct ac_mesh_module * ac_mesh;
	struct ac_render_module * ac_render;
	struct ac_texture_module * ac_texture;
	bgfx_program_handle_t program;
	bgfx_uniform_handle_t sampler[5];
	bgfx_texture_handle_t null_tex;
};

boolean cbstreamreadtemplate(
	struct ac_character_module * mod,
	void * data, 
	struct ap_module_stream * stream)
{
	struct ac_character_template * temp = ac_character_get_template(data);
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = ap_module_stream_get_value_name(stream);
		const char * value = ap_module_stream_get_value(stream);
		enum ac_lod_stream_read_result readresult = ac_lod_stream_read(mod->ac_lod, 
			stream, &temp->lod);
		enum ac_render_stream_read_result crtreadresult;
		switch (readresult) {
		case AC_LOD_STREAM_READ_RESULT_ERROR:
			ERROR("Failed to read character lod stream.");
			return FALSE;
		case AC_LOD_STREAM_READ_RESULT_PASS:
			break;
		case AC_LOD_STREAM_READ_RESULT_READ:
			continue;
		}
		crtreadresult = ac_render_stream_read_crt(stream, &temp->clump_render_type);
		switch (crtreadresult) {
		case AC_RENDER_STREAM_READ_RESULT_ERROR:
			ERROR("Failed to read character render stream.");
			return FALSE;
		case AC_RENDER_STREAM_READ_RESULT_PASS:
			break;
		case AC_RENDER_STREAM_READ_RESULT_READ:
			continue;
		}
		if (strcmp(value_name, INI_NAME_DFF) == 0) {
			snprintf(temp->dff_name, sizeof(temp->dff_name), "A%07X.ecl",
				strtoul(value, NULL, 10));
		}
		else if (strcmp(value_name, INI_NAME_DEF_ARM_DFF) == 0) {
			strlcpy(temp->default_armour_dff_name, value, 
				sizeof(temp->default_armour_dff_name));
		}
		else if (strcmp(value_name, INI_NAME_HEIGHT) == 0) {
			temp->height = strtol(value, NULL, 10);
		}
		else if (strcmp(value_name, INI_NAME_SCALE) == 0) {
			temp->scale = strtof(value, NULL);
		}
	}
	return TRUE;
}

static boolean charctor(
	struct ac_character_module * mod,
	struct ap_character * c)
{
	return TRUE;
}

static void releasetemp(
	struct ac_character_module * mod,
	struct ap_character_template * temp)
{
	struct ac_character_template * attachment = ac_character_get_template(temp);
	assert(attachment->refcount != 0);
	if (!--attachment->refcount) {
		/** \todo Release resources. */
		if (attachment->clump) {
			ac_mesh_destroy_clump(mod->ac_mesh, attachment->clump);
			attachment->clump = NULL;
		}
	}
}

static boolean chardtor(
	struct ac_character_module * mod,
	struct ap_character * character)
{
	if (character->temp)
		releasetemp(mod, character->temp);
	return TRUE;
}

static boolean cbchartemplatector(
	struct ac_character_module * mod,
	struct ap_character_template * temp)
{
	struct ac_character_template * attachment = ac_character_get_template(temp);
	attachment->scale = 1.0f;
	return TRUE;
}

static boolean createbuffers(struct ac_mesh_clump * c)
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

static struct ac_mesh_clump * loadclump(
	struct ac_character_module * mod, 
	const char * dff)
{
	void * data;
	size_t size;
	struct bin_stream * stream;
	RwChunkHeaderInfo ch;
	if (!ac_dat_load_resource(mod->ac_dat, AC_DAT_DIR_CHARACTER, dff, 
			&data, &size)) {
		WARN("Failed to load dff: %s", dff);
		return NULL;
	}
	ac_texture_set_dictionary(mod->ac_texture, AC_DAT_DIR_TEX_CHARACTER);
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
			if (!createbuffers(c)) {
				/** \todo */
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

static boolean cbcharsettemplate(
	struct ac_character_module * mod,
	struct ap_character_cb_set_template * cb)
{
	struct ap_character * character = cb->character;
	if (cb->previous_temp)
		releasetemp(mod, cb->previous_temp);
	if (character->temp) {
		struct ac_character_template * temp = ac_character_get_template(character->temp);
		if (temp->refcount++ == 0 && !strisempty(temp->dff_name)) {
			temp->clump = loadclump(mod, temp->dff_name);
		}
	}
	return TRUE;
}

static boolean onregister(
	struct ac_character_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_dat, AC_DAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_lod, AC_LOD_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_mesh, AC_MESH_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_render, AC_RENDER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_texture, AC_TEXTURE_MODULE_NAME);
	AC_CHARACTER_ATTACHMENT_OFFSET = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct ac_character),
		mod, charctor, chardtor);
	AC_CHARACTER_TEMPLATE_ATTACHMENT_OFFSET = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_TEMPLATE, sizeof(struct ac_character_template),
		mod, cbchartemplatector, NULL);
	ap_character_add_stream_callback(mod->ap_character, AP_CHARACTER_MDI_TEMPLATE, 
		AC_CHARACTER_MODULE_NAME, mod, cbstreamreadtemplate, NULL);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_SET_TEMPLATE, mod, cbcharsettemplate);
	return TRUE;
}

static boolean createshaders(struct ac_character_module * mod)
{
	bgfx_shader_handle_t vsh;
	bgfx_shader_handle_t fsh;
	const bgfx_memory_t * mem;
	uint8_t null_data[256];
	if (!ac_render_load_shader("ac_character.vs", &vsh)) {
		ERROR("Failed to load vertex shader.");
		return FALSE;
	}
	if (!ac_render_load_shader("ac_character.fs", &fsh)) {
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

static boolean oninitialize(struct ac_character_module * mod)
{
	if (!createshaders(mod)) {
		ERROR("Failed to create shaders.");
		return FALSE;
	}
	return TRUE;
}

static void onclose(struct ac_character_module * mod)
{
}

static void onshutdown(struct ac_character_module * mod)
{
	uint32_t i;
	bgfx_destroy_program(mod->program);
	for (i = 0; i < COUNT_OF(mod->sampler); i++)
		bgfx_destroy_uniform(mod->sampler[i]);
	bgfx_destroy_texture(mod->null_tex);
}

struct ac_character_module * ac_character_create_module()
{
	struct ac_character_module * mod = ap_module_instance_new(AC_CHARACTER_MODULE_NAME, 
		sizeof(*mod), onregister, oninitialize, onclose, onshutdown);
	return mod;
}

boolean ac_character_read_templates(
	struct ac_character_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct ap_module_stream * stream;
	uint32_t count;
	uint32_t i;
	stream = ap_module_stream_create();
	if (!ap_module_stream_open(stream, file_path, 0, decrypt)) {
		ERROR("Failed to open stream (%s).", file_path);
		ap_module_stream_destroy(stream);
		return FALSE;
	}
	count = ap_module_stream_get_section_count(stream);
	for (i = 0; i < count; i++) {
		uint32_t tid = strtoul(ap_module_stream_read_section_name(stream, i), 
			NULL, 10);
		struct ap_character_template * temp = 
			ap_character_get_template(mod->ap_character, tid);
		if (!temp) {
			ERROR("Failed to retrieve character template (tid = %u).", tid);
			ap_module_stream_destroy(stream);
			return FALSE;
		}
		if (!ap_module_stream_enum_read(mod->ap_character, stream, 
				AP_CHARACTER_MDI_TEMPLATE, temp)) {
			ERROR("Failed to read character template (tid = %u).", tid);
			ap_module_stream_destroy(stream);
			return FALSE;
		}
	}
	ap_module_stream_destroy(stream);
	INFO("Loaded %u character client templates.", count);
	return TRUE;
}

void ac_character_render(
	struct ac_character_module * mod,
	struct ap_character * character)
{
	struct ac_character_template * attachment;
	struct ac_mesh_geometry * g = NULL;
	int view;
	if (!character->temp)
		return;
	attachment = ac_character_get_template(character->temp);
	if (!attachment->clump)
		return;
	view = ac_render_get_view(mod->ac_render);
	g = attachment->clump->glist;
	while (g) {
		const uint8_t discard = 
			BGFX_DISCARD_BINDINGS |
			BGFX_DISCARD_INDEX_BUFFER |
			BGFX_DISCARD_INSTANCE_DATA |
			BGFX_DISCARD_STATE;
		mat4 model;
		uint32_t j;
		vec3 pos = { character->pos.x, character->pos.y, character->pos.z };
		vec3 scale = { attachment->scale, attachment->scale, attachment->scale };
		if (!BGFX_HANDLE_IS_VALID(g->vertex_buffer)) {
			g = g->next;
			continue;
		}
		glm_mat4_identity(model);
		glm_translate(model, pos);
		glm_rotate_x(model, glm_rad(character->rotation_x), model);
		glm_rotate_y(model, glm_rad(character->rotation_y), model);
		glm_scale(model, scale);
		bgfx_set_transform(&model, 1);
		bgfx_set_vertex_buffer(0, g->vertex_buffer, 0, g->vertex_count);
		for (j = 0; j < g->split_count; j++) {
			struct ac_mesh_split * s = &g->splits[j];
			struct ac_mesh_material * mat = &g->materials[s->material_index];
			uint32_t k;
			uint64_t state = BGFX_STATE_WRITE_MASK |
				BGFX_STATE_DEPTH_TEST_LESS |
				BGFX_STATE_CULL_CW;
			bgfx_set_index_buffer(g->index_buffer, s->index_offset, s->index_count);
			for (k = 0; k < COUNT_OF(mod->sampler); k++) {
				if (BGFX_HANDLE_IS_VALID(mat->tex_handle[k])) {
					bgfx_set_texture(k, mod->sampler[k], 
						mat->tex_handle[k], UINT32_MAX);
				}
				else {
					bgfx_set_texture(k, mod->sampler[k], mod->null_tex, UINT32_MAX);
				}
			}
			bgfx_set_state(state, 0xffffffff);
			bgfx_submit(view, mod->program, 0, discard);
		}
		g = g->next;
	}
}
