#include "client/ac_effect.h"

#include "core/bin_stream.h"
#include "core/log.h"
#include "core/malloc.h"
#include "core/vector.h"

#include "public/ap_config.h"

#include "client/ac_texture.h"

#include <assert.h>
#include <stdlib.h>

struct ac_effect_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	struct ac_texture_module * ac_texture;
	struct ap_admin effect_set_admin;
};

static const size_t EFFECT_SIZE[AC_EFFECT_COUNT] = {
	sizeof(struct ac_effect_board),
	sizeof(struct ac_effect_particle_system),
	sizeof(struct ac_effect_particle_system_simple_black_hole),
	sizeof(struct ac_effect_tail),
	sizeof(struct ac_effect_object),
	sizeof(struct ac_effect_light),
	sizeof(struct ac_effect_sound),
	sizeof(struct ac_effect_moving_frame),
	sizeof(struct ac_effect_terrain_board),
	sizeof(struct ac_effect_post_fx),
	sizeof(struct ac_effect_camera) };

static const size_t ANIMATION_SIZE[AC_EFFECT_ANIMATION_COUNT] = {
	sizeof(struct ac_effect_animation_color),
	sizeof(struct ac_effect_animation_uv),
	sizeof(struct ac_effect_animation_missile),
	sizeof(struct ac_effect_animation_linear),
	sizeof(struct ac_effect_animation_revolution),
	sizeof(struct ac_effect_animation_rotation),
	sizeof(struct ac_effect_animation_spline),
	sizeof(struct ac_effect_animation_rt_anim),
	sizeof(struct ac_effect_animation_scale),
	sizeof(struct ac_effect_animation_particle_pos_scale),
	sizeof(struct ac_effect_animation_post_fx),
	sizeof(struct ac_effect_animation_camera) };

static const RwV3d AXISWX = { -1.0f, 0.0f, 0.0f };
static const RwV3d AXISWY = { 0.0f, -1.0f, 0.0f };
static const RwV3d AXISWZ = { 0.0f, 0.0f, -1.0f };

static boolean onregister(
	struct ac_effect_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_texture, AC_TEXTURE_MODULE_NAME);
	return TRUE;
}

static boolean readeffectsets(struct ac_effect_module * mod)
{
	char path[1024];
	struct bin_stream * stream;
	struct bin_stream * varstream;
	uint32_t count = 0;
	uint32_t i;
	if (!make_path(path, sizeof(path), "%s/effect/ini/new/efffile_set.eff", 
			ap_config_get(mod->ap_config, "ClientDir"))) {
		ERROR("Failed to create effect set path.");
		return FALSE;
	}
	if (!bstream_from_file(path, &stream)) {
		ERROR("Failed to create effect set stream.");
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/effect/ini/new/efffile_var.eff", 
			ap_config_get(mod->ap_config, "ClientDir"))) {
		ERROR("Failed to create effect var path.");
		bstream_destroy(stream);
		return FALSE;
	}
	if (!bstream_from_file(path, &varstream)) {
		ERROR("Failed to create effect var stream.");
		bstream_destroy(stream);
		return FALSE;
	}
	if (!bstream_read_u32(stream, &count)) {
		ERROR("Failed to read effect set count.");
		bstream_destroy(varstream);
		bstream_destroy(stream);
		return FALSE;
	}
	for (i = 0; i < count; i++) {
		struct ac_effect_set * set = ac_effect_new_set(mod);
		void ** object;
		boolean result = TRUE;
		uint32_t j;
		result &= bstream_read_u32(stream, &set->effect_set_id);
		result &= bstream_read(stream, set->title, sizeof(set->title));
		result &= bstream_read_u32(stream, &set->effect_set_life);
		result &= bstream_read(stream, &set->loop_option, sizeof(set->loop_option));
		result &= bstream_read_u32(stream, &set->effect_set_flags);
		result &= bstream_read(stream, &set->missile_info, sizeof(set->missile_info));
		result &= bstream_read(stream, &set->bsphere, sizeof(set->bsphere));
		result &= bstream_read(stream, &set->bbox, sizeof(set->bbox));
		result &= bstream_read(stream, &set->var_size_info, sizeof(set->var_size_info));
		result &= bstream_read_u32(stream, &set->file_offset);
		result &= bstream_read_u32(stream, &set->file_size);
		result &= bstream_seek(varstream, set->file_offset);
		for (j = 0; j < set->var_size_info.texture_count; j++) {
			struct ac_effect_texture * tex = vec_add_empty(&set->textures);
			BGFX_INVALIDATE_HANDLE(tex->texture);
			result &= bstream_read_u32(varstream, &tex->index);
			result &= bstream_read(varstream, tex->texture_name, sizeof(tex->texture_name));
			result &= bstream_read(varstream, tex->mask_name, sizeof(tex->mask_name));
		}
		for (j = 0; j < set->var_size_info.base_dependancy_count; j++) {
			struct ac_effect_base_dependancy * dep = vec_add_empty(&set->dependancies);
			result &= bstream_read_i32(varstream, &dep->parent_index);
			result &= bstream_read_i32(varstream, &dep->child_index);
			result &= bstream_read_u32(varstream, &dep->flags);
		}
		for (j = 0; j < set->var_size_info.base_count; j++) {
			enum ac_effect_type type = AC_EFFECT_BOARD;
			struct ac_effect_base * effect;
			result &= bstream_read(varstream, &type, sizeof(type));
			effect = ac_effect_new_effect(mod, type);
			if (!effect || !effect->StreamRead(mod, effect, varstream)) {
				ac_effect_free_effect(mod, effect);
				result = FALSE;
				break;
			}
			vec_push_back((void **)&set->effects, &effect);
		}
		if (!result) {
			ERROR("Stream ended unexpectedly.");
			ac_effect_release_set(mod, set);
			bstream_destroy(varstream);
			bstream_destroy(stream);
			return FALSE;
		}
		object = ap_admin_add_object_by_id(&mod->effect_set_admin, set->effect_set_id);
		if (object) {
			*object = set;
		}
		else {
			WARN("Failed to insert effect set (id = %u).", set->effect_set_id);
			ac_effect_release_set(mod, set);
		}
	}
	bstream_destroy(varstream);
	bstream_destroy(stream);
	return TRUE;
}

static boolean oninitialize(struct ac_effect_module * mod)
{
	if (!readeffectsets(mod)) {
		ERROR("Failed to read effect sets.");
		return FALSE;
	}
	return TRUE;
}

static void onshutdown(struct ac_effect_module * mod)
{
}

struct ac_effect_module * ac_effect_create_module()
{
	struct ac_effect_module * mod = ap_module_instance_new(AC_EFFECT_MODULE_NAME,
		sizeof(*mod), onregister, oninitialize, NULL, onshutdown);
	ap_admin_init(&mod->effect_set_admin, sizeof(struct ac_effect_set *), 128);
	return mod;
}

struct ac_effect_set * ac_effect_new_set(struct ac_effect_module * mod)
{
	struct ac_effect_set * set = alloc(sizeof(*set));
	memset(set, 0, sizeof(*set));
	set->effects = vec_new_reserved(sizeof(*set->effects), 4);
	set->textures = vec_new_reserved(sizeof(*set->textures), 4);
	set->dependancies = vec_new_reserved(sizeof(*set->dependancies), 4);
	set->refcount = 1;
	return set;
}

void ac_effect_release_set(struct ac_effect_module * mod, struct ac_effect_set * set)
{
	assert(set->refcount != 0);
	if (!--set->refcount) {
		uint32_t i;
		vec_free(set->effects);
		for (i = 0; i < vec_count(set->textures); i++) {
			if (BGFX_HANDLE_IS_VALID(set->textures[i].texture))
				ac_texture_release(mod->ac_texture, set->textures[i].texture);
		}
		vec_free(set->textures);
		vec_free(set->dependancies);
	}
}

static boolean streamreadtimetable(
	struct ac_effect_time_value ** table,
	struct bin_stream * stream)
{
	boolean result = TRUE;
	uint32_t count = 0;
	struct ac_effect_time_value * t = *table;
	struct ac_effect_time_value * t2;
	result &= bstream_read_u32(stream, &count);
	t2 = vec_reserve(t, sizeof(*t), count);
	result &= bstream_read(stream, t2, count * sizeof(*t2));
	if (t != t2)
		*table = t2;
	return result;
}

static boolean streamreadtimetablecolor(
	struct ac_effect_time_value_color ** table,
	struct bin_stream * stream)
{
	boolean result = TRUE;
	uint32_t count = 0;
	struct ac_effect_time_value_color * t = *table;
	struct ac_effect_time_value_color * t2;
	result &= bstream_read_u32(stream, &count);
	t2 = vec_reserve(t, sizeof(*t), count);
	result &= bstream_read(stream, t2, count * sizeof(*t2));
	if (t != t2)
		*table = t2;
	return result;
}

static boolean streamreadtimetableuv(
	struct ac_effect_time_value_uv ** table,
	struct bin_stream * stream)
{
	boolean result = TRUE;
	uint32_t count = 0;
	struct ac_effect_time_value_uv * t = *table;
	struct ac_effect_time_value_uv * t2;
	result &= bstream_read_u32(stream, &count);
	t2 = vec_reserve(t, sizeof(*t), count);
	result &= bstream_read(stream, t2, count * sizeof(*t2));
	if (t != t2)
		*table = t2;
	return result;
}

static boolean streamreadtimetablevec3(
	struct ac_effect_time_value_vec3 ** table,
	struct bin_stream * stream)
{
	boolean result = TRUE;
	uint32_t count = 0;
	struct ac_effect_time_value_vec3 * t = *table;
	struct ac_effect_time_value_vec3 * t2;
	result &= bstream_read_u32(stream, &count);
	t2 = vec_reserve(t, sizeof(*t), count);
	result &= bstream_read(stream, t2, count * sizeof(*t2));
	if (t != t2)
		*table = t2;
	return result;
}

static boolean streamreadtimetablerevolution(
	struct ac_effect_time_value_revolution ** table,
	struct bin_stream * stream)
{
	boolean result = TRUE;
	uint32_t count = 0;
	struct ac_effect_time_value_revolution * t = *table;
	struct ac_effect_time_value_revolution * t2;
	result &= bstream_read_u32(stream, &count);
	t2 = vec_reserve(t, sizeof(*t), count);
	result &= bstream_read(stream, t2, count * sizeof(*t2));
	if (t != t2)
		*table = t2;
	return result;
}

static boolean streamreadanimation(
	struct ac_effect_module * mod,
	struct ac_effect_animation * anim, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	result &= bstream_read_u32(stream, &anim->flags);
	result &= bstream_read_u32(stream, &anim->life_time);
	result &= bstream_read(stream, &anim->loop_option, sizeof(anim->loop_option));
	return result;
}

static boolean streamreadanimationcolor(
	struct ac_effect_module * mod,
	struct ac_effect_animation * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_animation_color * anim = (struct ac_effect_animation_color *)base;
	assert(base->type == AC_EFFECT_ANIMATION_COLOR);
	result &= streamreadanimation(mod, base, stream);
	result &= streamreadtimetablecolor(&anim->time_table, stream);
	return result;
}

static boolean streamreadanimationuv(
	struct ac_effect_module * mod,
	struct ac_effect_animation * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_animation_uv * anim = (struct ac_effect_animation_uv *)base;
	assert(base->type == AC_EFFECT_ANIMATION_TUTV);
	result &= streamreadanimation(mod, base, stream);
	result &= streamreadtimetableuv(&anim->time_table, stream);
	return result;
}

static boolean streamreadanimationmissile(
	struct ac_effect_module * mod,
	struct ac_effect_animation * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_animation_missile * anim = (struct ac_effect_animation_missile *)base;
	assert(base->type == AC_EFFECT_ANIMATION_MISSILE);
	result &= streamreadanimation(mod, base, stream);
	result &= bstream_read_f32(stream, &anim->speed0);
	result &= bstream_read_f32(stream, &anim->acceleration);
	return result;
}

static boolean streamreadanimationlinear(
	struct ac_effect_module * mod,
	struct ac_effect_animation * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_animation_linear * anim = (struct ac_effect_animation_linear *)base;
	assert(base->type == AC_EFFECT_ANIMATION_LINEAR);
	result &= streamreadanimation(mod, base, stream);
	result &= streamreadtimetablevec3(&anim->time_table, stream);
	return result;
}

static boolean streamreadanimationrevolution(
	struct ac_effect_module * mod,
	struct ac_effect_animation * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_animation_revolution * anim = (struct ac_effect_animation_revolution *)base;
	assert(base->type == AC_EFFECT_ANIMATION_REVOLUTION);
	result &= streamreadanimation(mod, base, stream);
	result &= bstream_read(stream, &anim->rotation_axis, sizeof(anim->rotation_axis));
	result &= bstream_read(stream, &anim->rotation_side, sizeof(anim->rotation_side));
	result &= streamreadtimetablerevolution(&anim->time_table, stream);
	return result;
}

static boolean streamreadanimationrotation(
	struct ac_effect_module * mod,
	struct ac_effect_animation * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_animation_rotation * anim = (struct ac_effect_animation_rotation *)base;
	assert(base->type == AC_EFFECT_ANIMATION_ROTATION);
	result &= streamreadanimation(mod, base, stream);
	result &= bstream_read(stream, &anim->rotation_axis, sizeof(anim->rotation_axis));
	result &= streamreadtimetable(&anim->time_table, stream);
	return result;
}

static boolean streamreadanimationspline(
	struct ac_effect_module * mod,
	struct ac_effect_animation * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_animation_spline * anim = (struct ac_effect_animation_spline *)base;
	assert(base->type == AC_EFFECT_ANIMATION_RPSPLINE);
	result &= streamreadanimation(mod, base, stream);
	if (anim->base.life_time)
		anim->inverse_life = 1.0f / anim->base.life_time;
	else
		anim->inverse_life = 0.0f;
	result &= bstream_read(stream, anim->spline_file_name, sizeof(anim->spline_file_name));
	return result;
}

static boolean streamreadanimationrtanim(
	struct ac_effect_module * mod,
	struct ac_effect_animation * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_animation_rt_anim * anim = (struct ac_effect_animation_rt_anim *)base;
	assert(base->type == AC_EFFECT_ANIMATION_RTANIM);
	result &= streamreadanimation(mod, base, stream);
	result &= bstream_read(stream, anim->anim_file_name, sizeof(anim->anim_file_name));
	return result;
}

static boolean streamreadanimationscale(
	struct ac_effect_module * mod,
	struct ac_effect_animation * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_animation_scale * anim = (struct ac_effect_animation_scale *)base;
	assert(base->type == AC_EFFECT_ANIMATION_SCALE);
	result &= streamreadanimation(mod, base, stream);
	result &= streamreadtimetablevec3(&anim->time_table, stream);
	return result;
}

static boolean streamreadanimationparticleposscale(
	struct ac_effect_module * mod,
	struct ac_effect_animation * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_animation_particle_pos_scale * anim = (struct ac_effect_animation_particle_pos_scale *)base;
	assert(base->type == AC_EFFECT_ANIMATION_PARTICLEPOSSCALE);
	result &= streamreadanimation(mod, base, stream);
	result &= streamreadtimetable(&anim->time_table, stream);
	return result;
}

static boolean streamreadbase(
	struct ac_effect_module * mod,
	struct ac_effect_base * effect, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	result &= bstream_read(stream, effect->base_title, sizeof(effect->base_title));
	result &= bstream_read_u32(stream, &effect->delay);
	result &= bstream_read_u32(stream, &effect->life);
	result &= bstream_read(stream, &effect->loop_option, sizeof(effect->loop_option));
	result &= bstream_read_u32(stream, &effect->flags);
	result &= bstream_read_u32(stream, &effect->animation_count);
	return result;
}

static boolean streamreadbaseanimations(
	struct ac_effect_module * mod,
	struct ac_effect_base * effect, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	uint32_t i;
	for (i = 0; i < effect->animation_count; i++) {
		enum ac_effect_animation_type type = AC_EFFECT_ANIMATION_COLOR;
		struct ac_effect_animation * anim;
		result &= bstream_read(stream, &type, sizeof(type));
		anim = ac_effect_new_animation(mod, type);
		result &= anim->StreamRead(mod, anim, stream);
		vec_push_back((void **)&effect->animations, &anim);
	}
	return result;
}

static void setinitialposition(
	struct ac_effect_render_base * render, 
	const RwV3d * pos)
{
	mat4 m;
	vec3 translate = { pos->x, pos->y, pos->z };
	glm_mat4_identity(m);
	glm_translate(m, translate);
	memcpy(render->translate, m, sizeof(render->translate));
}

static void setinitialrevolution(
	struct ac_effect_render_base * render, 
	const struct ac_effect_revolution * rev)
{
	mat4 m;
	vec3 x = { 1.0f, 0.0f, 0.0f };
	vec3 y = { 0.0f, 1.0f, 0.0f };
	vec3 z = { 0.0f, 0.0f, 1.0f };
	glm_mat4_identity(m);
	render->initial_revolution = *rev;
	if (render->base.type == AC_EFFECT_OBJECT) {
		glm_rotate(m, glm_rad(rev->pitch), x);
		glm_rotate(m, glm_rad(rev->yaw), y);
		glm_rotate(m, glm_rad(rev->roll), z);
	}
	else {
		glm_rotate(m, glm_rad(rev->roll), z);
		glm_rotate(m, glm_rad(rev->yaw), y);
		glm_rotate(m, glm_rad(rev->pitch), x);
	}
	memcpy(render->rotate, m, sizeof(render->rotate));
}

static boolean streamreadrenderbase(
	struct ac_effect_module * mod,
	struct ac_effect_render_base * render, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	result &= bstream_read(stream, &render->blend_type, sizeof(render->blend_type));
	result &= bstream_read_i32(stream, &render->texture_info.index);
	result &= bstream_read(stream, &render->initial_position, sizeof(render->initial_position));
	result &= bstream_read(stream, &render->initial_revolution, sizeof(render->initial_revolution));
	setinitialposition(render, &render->initial_position);
	setinitialrevolution(render, &render->initial_revolution);
	return result;
}

static boolean streamreadboard(
	struct ac_effect_module * mod,
	struct ac_effect_base * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_board * effect = (struct ac_effect_board *)base;
	result &= streamreadbase(mod, &effect->base.base, stream);
	result &= streamreadrenderbase(mod, &effect->base, stream);
	result &= streamreadbaseanimations(mod, &effect->base.base, stream);
	return result;
}

static boolean streamreadparticlesystem(
	struct ac_effect_module * mod,
	struct ac_effect_base * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_particle_system * effect = (struct ac_effect_particle_system *)base;
	result &= streamreadbase(mod, &effect->base.base, stream);
	result &= streamreadrenderbase(mod, &effect->base, stream);
	result &= bstream_read_i32(stream, &effect->capacity);
	result &= bstream_read_u32(stream, &effect->shoot_delay);
	result &= bstream_read(stream, effect->clump_file_name, sizeof(effect->clump_file_name));
	result &= bstream_read(stream, &effect->emitter, sizeof(effect->emitter));
	result &= bstream_read(stream, &effect->particle_prop, sizeof(effect->particle_prop));
	result &= streamreadbaseanimations(mod, &effect->base.base, stream);
	return result;
}

static boolean streamreadparticlesystemsimpleblackhole(
	struct ac_effect_module * mod,
	struct ac_effect_base * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_particle_system_simple_black_hole * effect = (struct ac_effect_particle_system_simple_black_hole *)base;
	result &= streamreadbase(mod, &effect->base.base, stream);
	result &= streamreadrenderbase(mod, &effect->base, stream);
	result &= bstream_read_i32(stream, &effect->capacity);
	result &= bstream_read_i32(stream, &effect->one_shoot_count);
	result &= bstream_read_i32(stream, &effect->one_shoot_count_offset);
	result &= bstream_read_u32(stream, &effect->time_gap);
	result &= bstream_read_u32(stream, &effect->particle_life);
	result &= bstream_read_f32(stream, &effect->initial_speed);
	result &= bstream_read_f32(stream, &effect->initial_speed_offset);
	result &= bstream_read_f32(stream, &effect->roll_min);
	result &= bstream_read_f32(stream, &effect->roll_max);
	result &= bstream_read_f32(stream, &effect->roll_step);
	result &= bstream_read_f32(stream, &effect->radius);
	if (effect->time_gap == 0)
		effect->time_gap = 1;
	effect->roll_step = (effect->roll_max - effect->roll_min) / effect->capacity;
	result &= streamreadbaseanimations(mod, &effect->base.base, stream);
	return result;
}

static boolean streamreadtail(
	struct ac_effect_module * mod,
	struct ac_effect_base * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_tail * effect = (struct ac_effect_tail *)base;
	result &= streamreadbase(mod, &effect->base.base, stream);
	result &= streamreadrenderbase(mod, &effect->base, stream);
	result &= bstream_read_i32(stream, &effect->max_count);
	result &= bstream_read_u32(stream, &effect->time_gap);
	result &= bstream_read_u32(stream, &effect->point_life);
	if (effect->point_life != 0)
		effect->inverse_point_life = 1.0f / effect->point_life;
	else
		effect->inverse_point_life = 0.0f;
	result &= bstream_read_f32(stream, &effect->height1);
	result &= bstream_read_f32(stream, &effect->height2);
	result &= streamreadbaseanimations(mod, &effect->base.base, stream);
	return result;
}

static boolean streamreadobject(
	struct ac_effect_module * mod,
	struct ac_effect_base * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_object * effect = (struct ac_effect_object *)base;
	result &= streamreadbase(mod, &effect->base.base, stream);
	result &= streamreadrenderbase(mod, &effect->base, stream);
	result &= bstream_read(stream, effect->clump_file_name, sizeof(effect->clump_file_name));
	if (base->flags & AC_EFFECT_FLAG_OBJ_DUMMY)
		memset(effect->clump_file_name, 0, sizeof(effect->clump_file_name));
	result &= bstream_read(stream, &effect->prelit_color, sizeof(effect->prelit_color));
	result &= streamreadbaseanimations(mod, &effect->base.base, stream);
	return result;
}

static boolean streamreadlight(
	struct ac_effect_module * mod,
	struct ac_effect_base * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_light * effect = (struct ac_effect_light *)base;
	result &= streamreadbase(mod, &effect->base, stream);
	result &= bstream_read(stream, &effect->light_type, sizeof(effect->light_type));
	result &= bstream_read(stream, &effect->center, sizeof(effect->center));
	result &= bstream_read(stream, &effect->revolution, sizeof(effect->revolution));
	result &= bstream_read_f32(stream, &effect->con_angle);
	result &= bstream_read(stream, &effect->surface_prop, sizeof(effect->surface_prop));
	result &= bstream_read(stream, &effect->color, sizeof(effect->color));
	result &= streamreadbaseanimations(mod, &effect->base, stream);
	return result;
}

static boolean streamreadsound(
	struct ac_effect_module * mod,
	struct ac_effect_base * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_sound * effect = (struct ac_effect_sound *)base;
	result &= streamreadbase(mod, &effect->base, stream);
	result &= bstream_read(stream, &effect->sound_type, sizeof(effect->sound_type));
	result &= bstream_read(stream, effect->sound_file_name, sizeof(effect->sound_file_name));
	result &= bstream_read(stream, effect->mono_file_name, sizeof(effect->mono_file_name));
	result &= bstream_read_u32(stream, &effect->loop_count);
	result &= bstream_read_f32(stream, &effect->volume);
	result &= streamreadbaseanimations(mod, &effect->base, stream);
	return result;
}

static boolean streamreadmovingfame(
	struct ac_effect_module * mod,
	struct ac_effect_base * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_moving_frame * effect = (struct ac_effect_moving_frame *)base;
	result &= streamreadbase(mod, &effect->base, stream);
	result &= bstream_read(stream, &effect->type, sizeof(effect->type));
	result &= bstream_read(stream, &effect->axis, sizeof(effect->axis));
	result &= bstream_read(stream, &effect->whose, sizeof(effect->whose));
	result &= bstream_read_f32(stream, &effect->amplitude);
	result &= bstream_read_u32(stream, &effect->duration);
	result &= bstream_read_f32(stream, &effect->total_cycle);
	if (effect->duration != 0) {
		effect->cof = effect->amplitude / (effect->duration * effect->duration);
		effect->speed = effect->total_cycle * 360.0f / effect->duration;
	}
	else {
		effect->cof = 0.0f;
		effect->speed = 0.1f;
	}
	result &= streamreadbaseanimations(mod, &effect->base, stream);
	return result;
}

static boolean streamreadterrainboard(
	struct ac_effect_module * mod,
	struct ac_effect_base * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_terrain_board * effect = (struct ac_effect_terrain_board *)base;
	result &= streamreadbase(mod, &effect->base.base, stream);
	result &= streamreadrenderbase(mod, &effect->base, stream);
	result &= streamreadbaseanimations(mod, &effect->base.base, stream);
	return result;
}

static boolean streamreadpostfx(
	struct ac_effect_module * mod,
	struct ac_effect_base * base, 
	struct bin_stream * stream)
{
	boolean result = TRUE;
	struct ac_effect_post_fx * effect = (struct ac_effect_post_fx *)base;
	result &= streamreadbase(mod, &effect->base, stream);
	result &= bstream_read(stream, &effect->fx_type, sizeof(effect->fx_type));
	result &= bstream_read(stream, &effect->center, sizeof(effect->center));
	result &= bstream_read_u32(stream, &effect->flags);
	result &= streamreadbaseanimations(mod, &effect->base, stream);
	return result;
}

static void constructbase(
	struct ac_effect_base * base, 
	enum ac_effect_type type,
	size_t size)
{
	memset(base, 0, size);
	base->type = type;
	base->animations = vec_new_reserved(sizeof(*base->animations), 2);
}

struct ac_effect_base * ac_effect_new_effect(
	struct ac_effect_module * mod, 
	enum ac_effect_type type)
{
	struct ac_effect_base * base = NULL;
	size_t size = 0;
	if (type < 0 || type >= AC_EFFECT_COUNT) {
		assert(0);
		return NULL;
	}
	size = EFFECT_SIZE[type];
	base = alloc(size);
	constructbase(base, type, size);
	switch (type) {
	case AC_EFFECT_BOARD: {
		struct ac_effect_board * effect = (struct ac_effect_board *)base;
		base->StreamRead = streamreadboard;
		effect->base.blend_type = AC_EFFECT_BLEND_ADD_ONE_ONE;
		break;
	}
	case AC_EFFECT_PSYS: {
		struct ac_effect_particle_system * effect = (struct ac_effect_particle_system *)base;
		base->StreamRead = streamreadparticlesystem;
		effect->base.blend_type = AC_EFFECT_BLEND_ADD_ONE_ONE;
		break;
	}
	case AC_EFFECT_PSYS_SIMPLEBLACKHOLE: {
		struct ac_effect_particle_system_simple_black_hole * effect = (struct ac_effect_particle_system_simple_black_hole *)base;
		base->StreamRead = streamreadparticlesystemsimpleblackhole;
		effect->base.blend_type = AC_EFFECT_BLEND_ADD_ONE_ONE;
		break;
	}
	case AC_EFFECT_TAIL: {
		struct ac_effect_tail * effect = (struct ac_effect_tail *)base;
		base->StreamRead = streamreadtail;
		effect->base.blend_type = AC_EFFECT_BLEND_ADD_ONE_ONE;
		break;
	}
	case AC_EFFECT_OBJECT: {
		struct ac_effect_object * effect = (struct ac_effect_object *)base;
		base->StreamRead = streamreadobject;
		effect->base.blend_type = AC_EFFECT_BLEND_ADD_ONE_ONE;
		break;
	}
	case AC_EFFECT_LIGHT: {
		struct ac_effect_light * effect = (struct ac_effect_light *)base;
		base->StreamRead = streamreadlight;
		effect->light_type = AC_EFFECT_LIGHT_POINT;
		break;
	}
	case AC_EFFECT_SOUND: {
		struct ac_effect_sound * effect = (struct ac_effect_sound *)base;
		base->StreamRead = streamreadsound;
		effect->sound_type = AC_EFFECT_SOUND_3DSOUND;
		break;
	}
	case AC_EFFECT_MOVINGFRAME: {
		struct ac_effect_moving_frame * effect = (struct ac_effect_moving_frame *)base;
		base->StreamRead = streamreadmovingfame;
		effect->type = AC_EFFECT_MFRM_SHAKE;
		break;
	}
	case AC_EFFECT_TERRAINBOARD: {
		struct ac_effect_terrain_board * effect = (struct ac_effect_terrain_board *)base;
		base->StreamRead = streamreadterrainboard;
		effect->base.blend_type = AC_EFFECT_BLEND_ADD_ONE_ONE;
		break;
	}
	case AC_EFFECT_POSTFX: {
		struct ac_effect_post_fx * effect = (struct ac_effect_post_fx *)base;
		base->StreamRead = streamreadpostfx;
		effect->fx_type = AC_EFFECT_FX_WAVE;
		break;
	}
	case AC_EFFECT_CAMERA: {
		struct ac_effect_camera * effect = (struct ac_effect_camera *)base;
		/* Not supported! */
		assert(0);
		break;
	}
	}
	return base;
}

void ac_effect_free_effect(
	struct ac_effect_module * mod, 
	struct ac_effect_base * effect)
{
	dealloc(effect);
}

struct ac_effect_animation * ac_effect_new_animation(
	struct ac_effect_module * mod, 
	enum ac_effect_animation_type type)
{
	struct ac_effect_animation * base;
	size_t size;
	if (type < 0 || type >= AC_EFFECT_ANIMATION_COUNT) {
		assert(0);
		return NULL;
	}
	size = ANIMATION_SIZE[type];
	base = alloc(size);
	memset(base, 0, size);
	base->type = type;
	switch (type) {
	case AC_EFFECT_ANIMATION_COLOR: {
		struct ac_effect_animation_color * anim = (struct ac_effect_animation_color *)base;
		base->StreamRead = streamreadanimationcolor;
		break;
	}
	case AC_EFFECT_ANIMATION_TUTV: {
		struct ac_effect_animation_uv * anim = (struct ac_effect_animation_uv *)base;
		base->StreamRead = streamreadanimationuv;
		break;
	}
	case AC_EFFECT_ANIMATION_MISSILE: {
		struct ac_effect_animation_missile * anim = (struct ac_effect_animation_missile *)base;
		base->StreamRead = streamreadanimationmissile;
		anim->speed0 = 1000.0f;
		break;
	}
	case AC_EFFECT_ANIMATION_LINEAR: {
		struct ac_effect_animation_linear * anim = (struct ac_effect_animation_linear *)base;
		base->StreamRead = streamreadanimationlinear;
		break;
	}
	case AC_EFFECT_ANIMATION_REVOLUTION: {
		struct ac_effect_animation_revolution * anim = (struct ac_effect_animation_revolution *)base;
		base->StreamRead = streamreadanimationrevolution;
		anim->rotation_axis = AXISWY;
		anim->rotation_side = AXISWX;
		break;
	}
	case AC_EFFECT_ANIMATION_ROTATION: {
		struct ac_effect_animation_rotation * anim = (struct ac_effect_animation_rotation *)base;
		base->StreamRead = streamreadanimationrotation;
		anim->rotation_axis = AXISWZ;
		break;
	}
	case AC_EFFECT_ANIMATION_RPSPLINE: {
		struct ac_effect_animation_spline * anim = (struct ac_effect_animation_spline *)base;
		base->StreamRead = streamreadanimationspline;
		break;
	}
	case AC_EFFECT_ANIMATION_RTANIM: {
		struct ac_effect_animation_rt_anim * anim = (struct ac_effect_animation_rt_anim *)base;
		base->StreamRead = streamreadanimationrtanim;
		break;
	}
	case AC_EFFECT_ANIMATION_SCALE: {
		struct ac_effect_animation_scale * anim = (struct ac_effect_animation_scale *)base;
		base->StreamRead = streamreadanimationscale;
		break;
	}
	case AC_EFFECT_ANIMATION_PARTICLEPOSSCALE: {
		struct ac_effect_animation_particle_pos_scale * anim = (struct ac_effect_animation_particle_pos_scale *)base;
		base->StreamRead = streamreadanimationparticleposscale;
		break;
	}
	case AC_EFFECT_ANIMATION_POSTFX: {
		struct ac_effect_animation_post_fx * anim = (struct ac_effect_animation_post_fx *)base;
		break;
	}
	case AC_EFFECT_ANIMATION_CAMERA: {
		struct ac_effect_animation_camera * anim = (struct ac_effect_animation_camera *)base;
		break;
	}
	}
	return base;
}

void ac_effect_free_animation(
	struct ac_effect_module * mod, 
	struct ac_effect_animation * anim)
{
	dealloc(anim);
}
