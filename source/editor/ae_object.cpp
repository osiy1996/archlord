#include "editor/ae_object.h"

#include <assert.h>
#include <string.h>

#include "core/core.h"
#include "core/malloc.h"
#include "core/log.h"
#include "core/string.h"
#include "core/vector.h"
#include "task/task.h"

#include "public/ap_config.h"
#include "public/ap_map.h"
#include "public/ap_sector.h"

#include "client/ac_object.h"
#include "client/ac_camera.h"
#include "client/ac_imgui.h"
#include "client/ac_mesh.h"
#include "client/ac_renderware.h"
#include "client/ac_render.h"
#include "client/ac_terrain.h"
#include "client/ac_texture.h"

#include "editor/ae_texture.h"

enum tool_type {
	TOOL_TYPE_NONE,
	TOOL_TYPE_MOVE,
	TOOL_TYPE_COUNT
};

enum move_axis {
	MOVE_AXIS_NONE,
	MOVE_AXIS_X,
	MOVE_AXIS_Y,
	MOVE_AXIS_Z,
	MOVE_AXIS_COUNT
};

struct move_tool {
	enum move_axis axis;
	struct au_pos prev_pos;
	float dx;
	float dy;
};

struct ae_object_module {
	struct ap_module_instance instance;
	struct ap_object_module * ap_object;
	struct ac_object_module * ac_object;
	struct ac_render_module * ac_render;
	struct ac_terrain_module * ac_terrain;
	boolean display_outliner;
	boolean display_properties;
	struct ap_object ** objects;
	struct ap_object * active_object;
	bgfx_program_handle_t program;
	/* Active tool type. */
	enum tool_type tool_type;
	/* Object move tool context. */
	struct move_tool move_tool;
};

inline void set_move_tool_axis(
	struct move_tool * t, 
	enum move_axis axis,
	struct ap_object * obj)
{
	if (obj)
		obj->position = t->prev_pos;
	t->axis = axis;
}

static struct ap_object * pick_object(
	struct ae_object_module * mod,
	struct ac_camera * cam,
	int mouse_x,
	int mouse_y)
{
	int w;
	int h;
	vec3 dir;
	SDL_GetWindowSize(ac_render_get_window(mod->ac_render), &w, &h);
	if (mouse_x < 0 || mouse_y < 0 || mouse_x > w || mouse_y > h)
		return NULL;
	ac_camera_ray(cam, (mouse_x / (w * 0.5f)) - 1.f,
		1.f - (mouse_y / (h * 0.5f)), dir);
	//return ac_terrain_raycast(cam->eye, dir, point);
	return ac_object_pick(mod->ac_object, cam->eye, dir);
}

static boolean raycast_terrain(
	struct ae_object_module * mod,
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

static void apply_move(
	struct ae_object_module * mod,
	struct ac_camera * cam,
	struct move_tool * tool,
	struct ap_object * obj)
{
	if (!obj)
		return;
	switch (tool->axis) {
	case MOVE_AXIS_NONE:
		break;
	case MOVE_AXIS_X: {
		vec3 right;
		struct au_pos pos = obj->position;
		ac_camera_right(cam, right);
		pos.x = tool->prev_pos.x + right[0] * tool->dx;
		pos.z = tool->prev_pos.z + right[2] * tool->dx;
		ap_object_move_object(mod->ap_object, obj, &pos);
		break;
	}
	case MOVE_AXIS_Y: {
		struct au_pos pos = obj->position;
		pos.y = tool->prev_pos.y - tool->dy;
		ap_object_move_object(mod->ap_object, obj, &pos);
		break;
	}
	case MOVE_AXIS_Z: {
		vec3 front;
		struct au_pos pos = obj->position;
		ac_camera_front(cam, front);
		pos.x = tool->prev_pos.x - front[0] * tool->dy;
		pos.z = tool->prev_pos.z - front[2] * tool->dy;
		ap_object_move_object(mod->ap_object, obj, &pos);
		break;
	}
	}
}

static boolean create_shaders(struct ae_object_module * mod)
{
	bgfx_shader_handle_t vsh;
	bgfx_shader_handle_t fsh;
	if (!ac_render_load_shader("ae_object_outline.vs", &vsh)) {
		ERROR("Failed to load vertex shader.");
		return FALSE;
	}
	if (!ac_render_load_shader("ae_object_outline.fs", &fsh)) {
		ERROR("Failed to load fragment shader.");
		return FALSE;
	}
	mod->program = bgfx_create_program(vsh, fsh, true);
	if (!BGFX_HANDLE_IS_VALID(mod->program)) {
		ERROR("Failed to create program.");
		return FALSE;
	}
	return TRUE;
}

static boolean cbobjectdtor(struct ae_object_module * mod, struct ap_object * obj)
{
	assert(obj != NULL);
	if (mod->active_object == obj)
		mod->active_object = NULL;
	return TRUE;
}

static boolean onregister(
	struct ae_object_module * mod,
	struct ap_module_registry * registry)
{
	mod->ap_object = (struct ap_object_module *)ap_module_registry_get_module(registry, AP_OBJECT_MODULE_NAME);
	if (!mod->ap_object) {
		ERROR("Failed to retrieve module (%s).", AP_OBJECT_MODULE_NAME);
		return FALSE;
	}
	if (ap_object_attach_data(mod->ap_object, AP_OBJECT_MDI_OBJECT, 
			0, mod, NULL, (ap_module_default_t)cbobjectdtor) == SIZE_MAX) {
		ERROR("Failed to attach object data.");
		return FALSE;
	}
	mod->ac_object = (struct ac_object_module *)ap_module_registry_get_module(registry, AC_OBJECT_MODULE_NAME);
	if (!mod->ac_object) {
		ERROR("Failed to retrieve module (%s).", AC_OBJECT_MODULE_NAME);
		return FALSE;
	}
	mod->ac_render = (struct ac_render_module *)ap_module_registry_get_module(registry, AC_RENDER_MODULE_NAME);
	if (!mod->ac_render) {
		ERROR("Failed to retrieve module (%s).", AC_RENDER_MODULE_NAME);
		return FALSE;
	}
	mod->ac_terrain = (struct ac_terrain_module *)ap_module_registry_get_module(registry, AC_TERRAIN_MODULE_NAME);
	if (!mod->ac_terrain) {
		ERROR("Failed to retrieve module (%s).", AC_TERRAIN_MODULE_NAME);
		return FALSE;
	}
	return TRUE;
}

static boolean oninitialize(struct ae_object_module * mod)
{
	if (!create_shaders(mod)) {
		ERROR("Failed to create editor:object shaders.");
		return FALSE;
	}
	return TRUE;
}

static void onshutdown(struct ae_object_module * mod)
{
	vec_free(mod->objects);
	bgfx_destroy_program(mod->program);
}

struct ae_object_module * ae_object_create_module()
{
	struct ae_object_module * mod = (struct ae_object_module *)ap_module_instance_new(
		AE_OBJECT_MODULE_NAME, sizeof(*mod), 
		(ap_module_instance_register_t)onregister, 
		(ap_module_instance_initialize_t)oninitialize, NULL, 
		(ap_module_instance_shutdown_t)onshutdown);
	mod->display_outliner = TRUE;
	mod->objects = (struct ap_object **)vec_new_reserved(sizeof(*mod->objects), 128);
	BGFX_INVALIDATE_HANDLE(mod->program);
	return mod;
}

void ae_object_update(float dt)
{
}

void ae_object_render_outline(struct ae_object_module * mod, struct ac_camera * cam)
{
	if (mod->active_object) {
		struct ac_object_render_data rd = { 0 };
		rd.state = 
			BGFX_STATE_WRITE_MASK |
			BGFX_STATE_DEPTH_TEST_ALWAYS |
			BGFX_STATE_BLEND_FUNC_SEPARATE(
				BGFX_STATE_BLEND_SRC_ALPHA,
				BGFX_STATE_BLEND_INV_SRC_ALPHA,
				BGFX_STATE_BLEND_ZERO,
				BGFX_STATE_BLEND_ONE);
		rd.program = mod->program;
		rd.no_texture = TRUE;
		ac_object_render_object(mod->ac_object, mod->active_object, &rd);
	}
}

void ae_object_render(struct ae_object_module * mod, struct ac_camera * cam)
{
}

boolean ae_object_on_mdown(
	struct ae_object_module * mod,
	struct ac_camera *  cam,
	int mouse_x,
	int mouse_y)
{
	switch (mod->tool_type) {
	case TOOL_TYPE_NONE:
		mod->active_object = 
			pick_object(mod, cam, mouse_x, mouse_y);
		if (mod->active_object)
			return TRUE;
		break;
	case TOOL_TYPE_MOVE:
		mod->tool_type = TOOL_TYPE_NONE;
		return TRUE;
	}
	return FALSE;
}

boolean ae_object_on_mmove(
	struct ae_object_module * mod,
	struct ac_camera *  cam,
	int mouse_x,
	int mouse_y,
	int dx,
	int dy)
{
	switch (mod->tool_type) {
	case TOOL_TYPE_MOVE: {
		struct move_tool * tool = &mod->move_tool;
		float mul = 25.0f;
		const boolean * state = ac_render_get_key_state(mod->ac_render);
		if (!mod->active_object)
			break;
		if (state[SDL_SCANCODE_LSHIFT] ||
			state[SDL_SCANCODE_RSHIFT]) {
			mul = 5.0f;
		}
		tool->dx += dx * mul;
		tool->dy += dy * mul;
		switch (tool->axis) {
		case MOVE_AXIS_NONE: {
			vec3 point = { 0 };
			boolean hit;
			hit = raycast_terrain(mod, cam, mouse_x, mouse_y, point);
			if (hit) {
				struct ac_object * objc = 
					ac_object_get_object(mod->ac_object, mod->active_object);
				float minh;
				if (objc->temp && 
					ac_object_get_min_height(mod->ac_object, objc->temp, &minh)) {
					struct au_pos pos =  {
						pos.x = point[0],
						pos.y = point[1] - minh,
						pos.z = point[2] };
					ap_object_move_object(mod->ap_object, mod->active_object, &pos);
				}
				return TRUE;
			}
			break;
		}
		default:
			apply_move(mod, cam, &mod->move_tool, mod->active_object);
			return TRUE;
		}
		break;
	}
	}
	return FALSE;
}

boolean ae_object_on_mwheel(struct ae_object_module * mod, float delta)
{
	return FALSE;
}

boolean ae_object_on_key_down(
	struct ae_object_module * mod,
	struct ac_camera * cam,
	uint32_t keycode)
{
	const boolean * state = ac_render_get_key_state(mod->ac_render);
	switch (keycode) {
	case SDLK_d:
		if (state[SDL_SCANCODE_LSHIFT] && mod->active_object) {
			mod->active_object = 
				ap_object_duplicate(mod->ap_object, mod->active_object);
		}
	case SDLK_g:
		if (mod->tool_type != TOOL_TYPE_MOVE && 
			mod->active_object) {
			mod->tool_type = TOOL_TYPE_MOVE;
			mod->move_tool.axis = MOVE_AXIS_NONE;
			mod->move_tool.prev_pos = mod->active_object->position;
			mod->move_tool.dx = 0;
			mod->move_tool.dy = 0;
		}
		break;
	case SDLK_s: {
		if (state[SDL_SCANCODE_LCTRL] ||
			state[SDL_SCANCODE_RCTRL]) {
			ac_object_commit_changes(mod->ac_object);
			return TRUE;
		}
		break;
	case SDLK_x:
		switch (mod->tool_type) {
		case TOOL_TYPE_MOVE:
			set_move_tool_axis(&mod->move_tool, MOVE_AXIS_X, 
				mod->active_object);
			apply_move(mod, cam, &mod->move_tool, mod->active_object);
			break;
		}
		break;
	case SDLK_y:
		switch (mod->tool_type) {
		case TOOL_TYPE_MOVE:
			set_move_tool_axis(&mod->move_tool, MOVE_AXIS_Y, 
				mod->active_object);
			apply_move(mod, cam, &mod->move_tool, mod->active_object);
			break;
		}
		break;
	case SDLK_z:
		switch (mod->tool_type) {
		case TOOL_TYPE_MOVE:
			set_move_tool_axis(&mod->move_tool, MOVE_AXIS_Z, 
				mod->active_object);
			apply_move(mod, cam, &mod->move_tool, mod->active_object);
			break;
		}
		break;
	case SDLK_ESCAPE:
		switch (mod->tool_type) {
		case TOOL_TYPE_MOVE:
			if (mod->active_object) {
				mod->active_object->position = 
					mod->move_tool.prev_pos;
			}
			break;
		default:
			return FALSE;
		}
		mod->tool_type = TOOL_TYPE_NONE;
		return TRUE;
	}
	case SDLK_DELETE:
		if (mod->active_object) {
			ap_object_destroy(mod->ap_object, mod->active_object);
			mod->active_object = NULL;
		}
		break;
	case SDLK_KP_PERIOD:
		if (mod->active_object) {
			vec3 center = { mod->active_object->position.x,
				mod->active_object->position.y,
				mod->active_object->position.z };
			ac_camera_focus_on(cam, center, 1000.0f);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static void render_outliner(struct ae_object_module * mod)
{
	ImGuiListClipper clipper;
	if (!ImGui::Begin("Objects", (bool *)&mod->display_outliner)) {
		ImGui::End();
		return;
	}
	vec_clear(mod->objects);
	ac_object_query_visible_objects(mod->ac_object, &mod->objects);
	clipper.Begin((int)vec_count(mod->objects));
	while (clipper.Step()) {
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
			struct ap_object * obj = mod->objects[i];
			struct ac_object * objc = ac_object_get_object(mod->ac_object, obj);
			char label[128];
			snprintf(label, sizeof(label), "%u (tid = %u)##%p", 
				obj->object_id, obj->tid, obj);
			if (ImGui::Selectable(label, 
					obj == mod->active_object)) {
				mod->active_object = obj;
			}
		}
	}
	clipper.End();
	ImGui::End();
}

static void obj_type_prop(
	uint32_t * object_type, 
	uint32_t bit,
	const char * label)
{
	bool b = (*object_type & bit) != 0;
	if (ImGui::Checkbox(label, &b)) {
		if (b) {
			*object_type |= bit;
		}
		else {
			*object_type &= ~bit;
		}
	}
}

static void obj_transform(struct ae_object_module * mod, struct ap_object * obj)
{
	struct au_pos pos = obj->position;
	bool move = false;
	move |= ImGui::DragFloat("Position X", &pos.x);
	move |= ImGui::DragFloat("Position Y", &pos.y);
	move |= ImGui::DragFloat("Position Z", &pos.z);
	ImGui::DragFloat("Scale X", &obj->scale.x, 0.001f);
	ImGui::DragFloat("Scale Y", &obj->scale.y, 0.001f);
	ImGui::DragFloat("Scale Z", &obj->scale.z, 0.001f);
	ImGui::DragFloat("Rotation X", &obj->rotation_x);
	ImGui::DragFloat("Rotation Y", &obj->rotation_y);
	if (move)
		ap_object_move_object(mod->ap_object, obj, &pos);
}

static void render_properties(struct ae_object_module * mod)
{
	struct ap_object * obj = mod->active_object;
	struct ac_object * objc;
	if (!ImGui::Begin("Properties", (bool *)&mod->display_properties) || !obj) {
		ImGui::End();
		return;
	}
	objc = ac_object_get_object(mod->ac_object, obj);
	ImGui::InputScalar("Object ID", ImGuiDataType_U32, 
		&obj->object_id, NULL, NULL, NULL, 
		ImGuiInputTextFlags_ReadOnly);
	ImGui::InputScalar("Object TID", ImGuiDataType_U32, 
		&obj->tid, NULL, NULL, NULL, 
		ImGuiInputTextFlags_ReadOnly);
	obj_transform(mod, obj);
	if (ImGui::TreeNodeEx("Public Properties", 
			ImGuiTreeNodeFlags_Framed)) {
		obj_type_prop(&obj->object_type, AC_OBJECT_TYPE_BLOCKING, 
			"BLOCKING");
		obj_type_prop(&obj->object_type, AC_OBJECT_TYPE_RIDABLE, 
			"RIDABLE");
		ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("Client Properties", 
			ImGuiTreeNodeFlags_Framed)) {
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_BLOCKING, 
			"BLOCKING");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_RIDABLE, 
			"RIDABLE");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_NO_CAMERA_ALPHA, 
			"NO_CAMERA_ALPHA");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_USE_ALPHA, 
			"USE_ALPHA");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_USE_AMBIENT, 
			"USE_AMBIENT");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_USE_LIGHT, 
			"USE_LIGHT");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_USE_PRE_LIGHT, 
			"USE_PRE_LIGHT");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_USE_FADE_IN_OUT, 
			"USE_FADE_IN_OUT");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_IS_SYSTEM_OBJECT, 
			"IS_SYSTEM_OBJECT");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_MOVE_INSECTOR, 
			"MOVE_INSECTOR");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_NO_INTERSECTION, 
			"NO_INTERSECTION");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_WORLDADD, 
			"WORLDADD");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_OBJECTSHADOW, 
			"OBJECTSHADOW");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_RENDER_UDA, 
			"RENDER_UDA");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_USE_ALPHAFUNC, 
			"USE_ALPHAFUNC");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_OCCLUDER, 
			"OCCLUDER");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_DUNGEON_STRUCTURE, 
			"DUNGEON_STRUCTURE");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_DUNGEON_DOME, 
			"DUNGEON_DOME");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_CAM_ZOOM, 
			"CAM_ZOOM");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_CAM_ALPHA, 
			"CAM_ALPHA");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_INVISIBLE, 
			"INVISIBLE");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_FORCED_RENDER_EFFECT, 
			"FORCED_RENDER_EFFECT");
		obj_type_prop(&objc->object_type, AC_OBJECT_TYPE_DONOT_CULL, 
			"DONOT_CULL");
		ImGui::TreePop();
	}
	ImGui::End();
}

void ae_object_imgui(struct ae_object_module * mod)
{
	render_outliner(mod);
	render_properties(mod);
}
