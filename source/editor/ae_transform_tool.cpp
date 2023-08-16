#include "editor/ae_transform_tool.h"

#include "core/log.h"

#include "public/ap_define.h"

#include "client/ac_camera.h"
#include "client/ac_render.h"
#include "client/ac_terrain.h"

#include "editor/ae_editor_action.h"

#include <assert.h>

enum transform_type {
	TRANSFORM_NONE,
	TRANSFORM_TRANSLATE,
	TRANSFORM_ROTATE,
	TRANSFORM_SCALE
};

enum bound_axis {
	BOUND_AXIS_NONE,
	BOUND_AXIS_X,
	BOUND_AXIS_Y,
	BOUND_AXIS_Z,
	BOUND_AXIS_COUNT
};

struct ae_transform_tool_module {
	struct ap_module_instance instance;
	struct ac_camera_module * ac_camera;
	struct ac_render_module * ac_render;
	struct ac_terrain_module * ac_terrain;
	struct ae_editor_action_module * ae_editor_action;
	enum transform_type transform_type;
	enum bound_axis axis;
	struct au_pos initial_pos;
	float dx;
	float dy;
	ap_module_t callback_module;
	ae_transform_tool_translate_callback_t translate_cb;
	void * target_base;
	float target_height;
};

static inline void bindtoaxis(
	struct ae_transform_tool_module * mod,
	enum bound_axis axis)
{
	if (mod->translate_cb)
		mod->translate_cb(mod->callback_module, &mod->initial_pos);
	mod->axis = axis;
}

static boolean raycast_terrain(
	struct ae_transform_tool_module * mod,
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

static void applytranslate(
	struct ae_transform_tool_module * mod,
	struct ac_camera * cam)
{
	assert(mod->translate_cb != NULL);
	switch (mod->axis) {
	case BOUND_AXIS_NONE:
		break;
	case BOUND_AXIS_X: {
		vec3 right;
		struct au_pos pos = mod->initial_pos;
		ac_camera_right(cam, right);
		pos.x = mod->initial_pos.x + right[0] * mod->dx;
		pos.z = mod->initial_pos.z + right[2] * mod->dx;
		mod->translate_cb(mod->callback_module, &pos);
		break;
	}
	case BOUND_AXIS_Y: {
		struct au_pos pos = mod->initial_pos;
		pos.y = mod->initial_pos.y - mod->dy;
		mod->translate_cb(mod->callback_module, &pos);
		break;
	}
	case BOUND_AXIS_Z: {
		vec3 front;
		struct au_pos pos = mod->initial_pos;
		ac_camera_front(cam, front);
		pos.x = mod->initial_pos.x - front[0] * mod->dy;
		pos.z = mod->initial_pos.z - front[2] * mod->dy;
		mod->translate_cb(mod->callback_module, &pos);
		break;
	}
	}
}

static boolean cbhandleinput(
	struct ae_transform_tool_module * mod,
	struct ae_editor_action_cb_handle_input * cb)
{
	const SDL_Event * e = cb->input;
	switch (cb->input->type) {
	case SDL_MOUSEMOTION:
		switch (mod->transform_type) {
		case TRANSFORM_TRANSLATE: {
			float mul = 25.0f;
			const boolean * state = ac_render_get_key_state(mod->ac_render);
			if (state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT])
				mul = 5.0f;
			mod->dx += e->motion.xrel * mul;
			mod->dy += e->motion.yrel * mul;
			switch (mod->axis) {
			case BOUND_AXIS_NONE: {
				vec3 point = { 0 };
				boolean hit;
				hit = raycast_terrain(mod, ac_camera_get_main(mod->ac_camera), 
					e->motion.x, e->motion.y, point);
				if (hit) {
					struct au_pos pos =  {
						pos.x = point[0],
						pos.y = point[1] - mod->target_height,
						pos.z = point[2] };
					mod->translate_cb(mod->callback_module, &pos);
				}
				break;
			}
			default:
				applytranslate(mod, ac_camera_get_main(mod->ac_camera));
				break;
			}
			return TRUE;
		}
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
		if (e->button.button == SDL_BUTTON_LEFT) {
			switch (mod->transform_type) {
			case TRANSFORM_TRANSLATE:
				mod->transform_type = TRANSFORM_NONE;
				return TRUE;
			}
		}
		else if (e->button.button == SDL_BUTTON_RIGHT) {
			switch (mod->transform_type) {
			case TRANSFORM_TRANSLATE:
				mod->translate_cb(mod->callback_module, &mod->initial_pos);
				mod->transform_type = TRANSFORM_NONE;
				return TRUE;
			}
		}
		break;
	case SDL_KEYDOWN:
		switch (e->key.keysym.sym) {
		case SDLK_g:
			if (mod->transform_type != TRANSFORM_TRANSLATE && mod->target_base) {
				mod->transform_type = TRANSFORM_TRANSLATE;
				mod->axis = BOUND_AXIS_NONE;
				mod->dx = 0.0f;
				mod->dy = 0.0f;
				return TRUE;
			}
			break;
		case SDLK_x:
			switch (mod->transform_type) {
			case TRANSFORM_TRANSLATE:
				bindtoaxis(mod, BOUND_AXIS_X);
				applytranslate(mod, ac_camera_get_main(mod->ac_camera));
				return TRUE;
			}
			break;
		case SDLK_y:
			switch (mod->transform_type) {
			case TRANSFORM_TRANSLATE:
				bindtoaxis(mod, BOUND_AXIS_Y);
				applytranslate(mod, ac_camera_get_main(mod->ac_camera));
				return TRUE;
			}
			break;
		case SDLK_z:
			switch (mod->transform_type) {
			case TRANSFORM_TRANSLATE:
				bindtoaxis(mod, BOUND_AXIS_Z);
				applytranslate(mod, ac_camera_get_main(mod->ac_camera));
				return TRUE;
			}
			break;
		case SDLK_ESCAPE:
			switch (mod->transform_type) {
			case TRANSFORM_TRANSLATE:
				mod->translate_cb(mod->callback_module, &mod->initial_pos);
				mod->transform_type = TRANSFORM_NONE;
				return TRUE;
			}
			break;
		}
		break;
	}
	return FALSE;
}

static boolean onregister(
	struct ae_transform_tool_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_camera, AC_CAMERA_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_render, AC_RENDER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_terrain, AC_TERRAIN_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ae_editor_action, AE_EDITOR_ACTION_MODULE_NAME);
	ae_editor_action_add_input_handler(mod->ae_editor_action, mod, (ap_module_default_t)cbhandleinput);
	return TRUE;
}

struct ae_transform_tool_module * ae_transform_tool_create_module()
{
	struct ae_transform_tool_module * mod = (struct ae_transform_tool_module *)ap_module_instance_new(
		AE_TRANSFORM_TOOL_MODULE_NAME, sizeof(*mod), 
		(ap_module_instance_register_t)onregister, NULL, NULL, NULL);
	return mod;
}

void ae_transform_tool_set_target(
	struct ae_transform_tool_module * mod,
	ap_module_t callback_module,
	ae_transform_tool_translate_callback_t translate_cb,
	void * target_base,
	const struct au_pos * position,
	float target_height)
{
	mod->callback_module = callback_module;
	mod->translate_cb = translate_cb;
	mod->target_base = target_base;
	mod->target_height = target_height;
	mod->initial_pos = *position;
}

void ae_transform_tool_switch_translate(struct ae_transform_tool_module * mod)
{
	assert(mod->target_base != NULL);
	mod->transform_type = TRANSFORM_TRANSLATE;
	mod->axis = BOUND_AXIS_NONE;
	mod->dx = 0.0f;
	mod->dy = 0.0f;
}

void ae_transform_tool_cancel_target(struct ae_transform_tool_module * mod)
{
	if (!mod->target_base)
		return;
	switch (mod->transform_type) {
	case TRANSFORM_TRANSLATE:
		mod->translate_cb(mod->callback_module, &mod->initial_pos);
		break;
	}
	mod->target_base = NULL;
	mod->transform_type = TRANSFORM_NONE;
}
