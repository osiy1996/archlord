#include "editor/ae_object.h"

#include <assert.h>
#include <string.h>

#include "core/core.h"
#include "core/file_system.h"
#include "core/malloc.h"
#include "core/log.h"
#include "core/string.h"
#include "core/vector.h"

#include "task/task.h"

#include "utility/au_md5.h"

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

#include "editor/ae_editor_action.h"
#include "editor/ae_event_binding.h"
#include "editor/ae_event_refinery.h"
#include "editor/ae_event_teleport.h"
#include "editor/ae_texture.h"

#include <ctype.h>

#define MAX_EDIT_TEMPLATE_COUNT 32

size_t g_AeObjectTemplateOffset = SIZE_MAX;

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
	struct ap_config_module * ap_config;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_object_module * ap_object;
	struct ac_camera_module * ac_camera;
	struct ac_object_module * ac_object;
	struct ac_render_module * ac_render;
	struct ac_terrain_module * ac_terrain;
	struct ae_editor_action_module * ae_editor_action;
	struct ae_event_binding_module * ae_event_binding;
	struct ae_event_refinery_module * ae_event_refinery;
	struct ae_event_teleport_module * ae_event_teleport;
	boolean display_outliner;
	boolean display_properties;
	struct ap_object ** objects;
	struct ap_object * active_object;
	bgfx_program_handle_t program;
	/* Active tool type. */
	enum tool_type tool_type;
	/* Object move tool context. */
	struct move_tool move_tool;
	bool select_object_template;
	char select_object_template_search_input[256];
	bool add_object;
	char add_object_search_input[256];
	bool display_template_editor;
	uint32_t templates_in_edit[MAX_EDIT_TEMPLATE_COUNT];
	uint32_t templates_in_edit_count;
	char template_editor_search_input[128];
	boolean has_pending_template_changes;
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

static boolean cbobjecttemplatector(
	struct ae_object_module * mod, 
	struct ap_object_template * temp)
{
	struct ae_object_template * attachment = ae_object_get_template(temp);
	return TRUE;
}

static boolean cbobjecttemplatedtor(
	struct ae_object_module * mod, 
	struct ap_object_template * temp)
{
	struct ae_object_template * attachment = ae_object_get_template(temp);
	return TRUE;
}

static boolean cbobjectdtor(struct ae_object_module * mod, struct ap_object * obj)
{
	assert(obj != NULL);
	if (mod->active_object == obj)
		mod->active_object = NULL;
	return TRUE;
}

static boolean cbobjectmove(
	struct ae_object_module * mod, 
	struct ap_object_cb_move_object_data * data)
{
	ae_event_binding_sync_region(mod->ae_event_binding, data->obj, 
		&data->obj->position);
	return TRUE;
}

static boolean cbcommitchanges(
	struct ae_object_module * mod,
	void * data)
{
	char serverpath[1024];
	char clientpath[1024];
	snprintf(serverpath, sizeof(serverpath), "%s/objects", 
		ap_config_get(mod->ap_config, "ServerIniDir"));
	snprintf(clientpath, sizeof(clientpath), "%s/ini",
		ap_config_get(mod->ap_config, "ClientDir"));
	uint32_t x;
	for (x = 0; x < AP_SECTOR_WORLD_INDEX_WIDTH; x++) {
		uint32_t z;
		for (z = 0; z < AP_SECTOR_WORLD_INDEX_HEIGHT; z++) {
			struct ac_object_sector * s = 
				ac_object_get_sector_by_index(mod->ac_object, x, z);
			if (s->flags & AC_OBJECT_SECTOR_HAS_CHANGES) {
				ac_object_export_sector(mod->ac_object, s, serverpath);
				copy_file(serverpath, clientpath, FALSE);
				s->flags &= ~AC_OBJECT_SECTOR_HAS_CHANGES;
			}
		}
	}
	if (mod->has_pending_template_changes) {
		snprintf(serverpath, sizeof(serverpath), "%s/objecttemplate.ini", 
			ap_config_get(mod->ap_config, "ServerIniDir"));
		if (!ap_object_write_templates(mod->ap_object, serverpath, FALSE)) {
			ERROR("Failed to commit object template changes.");
		}
		else {
			mod->has_pending_template_changes = FALSE;
		}
		snprintf(clientpath, sizeof(clientpath), "%s/ini/objecttemplate.ini", 
			ap_config_get(mod->ap_config, "ClientDir"));
		if (!au_md5_copy_and_encrypt_file(serverpath, clientpath)) {
			ERROR("Failed to reflect object template changes to client.");
		}
	}
	return TRUE;
}

static boolean cbrenderviewmenu(struct ae_object_module * mod, void * data)
{
	ImGui::Selectable("Object Template Editor", &mod->display_template_editor);
	return TRUE;
}

static bool rendertypeprop(
	uint32_t * object_type, 
	uint32_t bit,
	const char * label)
{
	bool b = (*object_type & bit) != 0;
	if (ImGui::Checkbox(label, &b)) {
		if (b) {
			*object_type |= bit;
			return true;
		}
		else {
			*object_type &= ~bit;
			return true;
		}
	}
	return true;
}

static bool rendertypeprops(uint32_t * type)
{
	bool changed = false;
	if (ImGui::TreeNodeEx("Public Properties", ImGuiTreeNodeFlags_Framed)) {
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_BLOCKING, "BLOCKING");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_RIDABLE, "RIDABLE");
		ImGui::TreePop();
	}
	return changed;
}

static bool renderclienttypeprops(uint32_t * type)
{
	bool changed = false;
	if (ImGui::TreeNodeEx("Client Properties", ImGuiTreeNodeFlags_Framed)) {
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_BLOCKING, "BLOCKING");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_RIDABLE, "RIDABLE");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_NO_CAMERA_ALPHA, "NO_CAMERA_ALPHA");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_USE_ALPHA, "USE_ALPHA");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_USE_AMBIENT, "USE_AMBIENT");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_USE_LIGHT, "USE_LIGHT");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_USE_PRE_LIGHT, "USE_PRE_LIGHT");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_USE_FADE_IN_OUT, "USE_FADE_IN_OUT");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_IS_SYSTEM_OBJECT, "IS_SYSTEM_OBJECT");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_MOVE_INSECTOR, "MOVE_INSECTOR");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_NO_INTERSECTION, "NO_INTERSECTION");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_WORLDADD, "WORLDADD");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_OBJECTSHADOW, "OBJECTSHADOW");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_RENDER_UDA, "RENDER_UDA");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_USE_ALPHAFUNC, "USE_ALPHAFUNC");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_OCCLUDER, "OCCLUDER");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_DUNGEON_STRUCTURE, "DUNGEON_STRUCTURE");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_DUNGEON_DOME, "DUNGEON_DOME");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_CAM_ZOOM, "CAM_ZOOM");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_CAM_ALPHA, "CAM_ALPHA");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_INVISIBLE, "INVISIBLE");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_FORCED_RENDER_EFFECT, "FORCED_RENDER_EFFECT");
		changed |= rendertypeprop(type, AC_OBJECT_TYPE_DONOT_CULL, "DONOT_CULL");
		ImGui::TreePop();
	}
	return changed;
}

static boolean isinedit(
	struct ae_object_module * mod, 
	struct ap_object_template * temp)
{
	uint32_t i;
	for (i = 0; i < mod->templates_in_edit_count; i++) {
		if (mod->templates_in_edit[i] == temp->tid)
			return TRUE;
	}
	return FALSE;
}

static inline size_t eraseinedit(
	struct ae_object_module * mod, 
	size_t index)
{
	memmove(&mod->templates_in_edit[index], &mod->templates_in_edit[index + 1],
		(size_t)--mod->templates_in_edit_count * sizeof(mod->templates_in_edit[0]));
	return index;
}

static void rendertemplateeditor(struct ae_object_module * mod)
{
	ImVec2 size = ImVec2(350.0f, 300.0f);
	ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter() - (size / 2.0f);
	size_t index = 0;
	struct ap_object_template * temp;
	ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Object Template Editor", &mod->display_template_editor)) {
		ImGui::End();
		return;
	}
	ImGui::InputText("Search", mod->template_editor_search_input, 
		sizeof(mod->template_editor_search_input));
	while (temp = ap_object_iterate_templates(mod->ap_object, &index)) {
		char label[128];
		struct ac_object_template * attachment = ac_object_get_template(temp);
		if (!strisempty(attachment->category)) {
			snprintf(label, sizeof(label), "[%05u] (%s) %s", temp->tid, 
				attachment->category, temp->name);
		}
		else {
			snprintf(label, sizeof(label), "[%05u] %s", temp->tid, temp->name);
		}
		if (!strisempty(mod->template_editor_search_input) &&
			!stristr(label, mod->template_editor_search_input)) {
			continue;
		}
		if (ImGui::Selectable(label) && 
			mod->templates_in_edit_count < MAX_EDIT_TEMPLATE_COUNT &&
			!isinedit(mod, temp)) {
			mod->templates_in_edit[mod->templates_in_edit_count++] = temp->tid;
		}
	}
	ImGui::End();
	for (index = 0; index < mod->templates_in_edit_count; index++) {
		char label[128];
		bool open = true;
		struct ac_object_template * attachment;
		bool changed = FALSE;
		temp = ap_object_get_template(mod->ap_object, mod->templates_in_edit[index]);
		if (!temp) {
			index = eraseinedit(mod, index);
			continue;
		}
		size = ImVec2(300.0f, 300.0f);
		center = ImGui::GetMainViewport()->GetWorkCenter() - (size / 2.0f);
		ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing);
		snprintf(label, sizeof(label), "Object Template - [%05u] %s###ObjTemp%u", 
			temp->tid, temp->name, temp->tid);
		if (!ImGui::Begin(label, &open) || !open) {
			if (!open)
				index = eraseinedit(mod, index);
			ImGui::End();
			continue;
		}
		attachment = ac_object_get_template(temp);
		changed |= ImGui::InputText("Name", temp->name, sizeof(temp->name));
		changed |= ImGui::InputText("Category", attachment->category, sizeof(attachment->category));
		changed |= ImGui::InputText("Collision DFF Name", attachment->collision_dff_name, sizeof(attachment->collision_dff_name));
		changed |= ImGui::InputText("Picking DFF Name", attachment->picking_dff_name, sizeof(attachment->picking_dff_name));
		changed |= rendertypeprops(&temp->type);
		changed |= renderclienttypeprops(&attachment->object_type);
		if (changed)
			mod->has_pending_template_changes = TRUE;
		ImGui::End();
	}
}

static boolean cbrendereditors(struct ae_object_module * mod, void * data)
{
	if (mod->display_template_editor)
		rendertemplateeditor(mod);
	return TRUE;
}


static boolean onregister(
	struct ae_object_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_object, AP_OBJECT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_camera, AC_CAMERA_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_object, AC_OBJECT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_render, AC_RENDER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_terrain, AC_TERRAIN_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ae_editor_action, AE_EDITOR_ACTION_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ae_event_binding, AE_EVENT_BINDING_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ae_event_refinery, AE_EVENT_REFINERY_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ae_event_teleport, AE_EVENT_TELEPORT_MODULE_NAME);
	g_AeObjectTemplateOffset = ap_object_attach_data(mod->ap_object, 
		AP_OBJECT_MDI_OBJECT_TEMPLATE, sizeof(struct ae_object_template), mod, 
		(ap_module_default_t)cbobjecttemplatector, 
		(ap_module_default_t)cbobjecttemplatedtor);
	ap_object_attach_data(mod->ap_object, AP_OBJECT_MDI_OBJECT, 0, mod, NULL, 
		(ap_module_default_t)cbobjectdtor);
	ap_object_add_callback(mod->ap_object, AP_OBJECT_CB_MOVE_OBJECT, mod, (ap_module_default_t)cbobjectmove);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_COMMIT_CHANGES, mod, (ap_module_default_t)cbcommitchanges);
	ae_editor_action_add_view_menu_callback(mod->ae_editor_action, "Object Template Editor", mod, (ap_module_default_t)cbrenderviewmenu);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_RENDER_EDITORS, mod, (ap_module_default_t)cbrendereditors);
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
	if (BGFX_HANDLE_IS_VALID(mod->program))
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

void ae_object_update(struct ae_object_module * mod, float dt)
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

boolean ae_object_on_lmb_down(
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

boolean ae_object_on_rmb_down(
	struct ae_object_module * mod,
	struct ac_camera * cam,
	int mouse_x,
	int mouse_y)
{
	switch (mod->tool_type) {
	case TOOL_TYPE_MOVE:
		if (mod->active_object) {
			ap_object_move_object(mod->ap_object, mod->active_object, 
				&mod->move_tool.prev_pos);
		}
		mod->tool_type = TOOL_TYPE_NONE;
		break;
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
				struct ac_object_template * temp = 
					ac_object_get_template(mod->active_object->temp);
				if (ac_object_get_min_height(mod->ac_object, temp, &minh)) {
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
	case SDLK_a:
		if (state[SDL_SCANCODE_LSHIFT])
			mod->add_object = true;
		break;
	case SDLK_d:
		if (state[SDL_SCANCODE_LSHIFT] && mod->active_object) {
			mod->active_object = ap_object_duplicate(mod->ap_object, 
				mod->active_object);
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
				ap_object_move_object(mod->ap_object, mod->active_object, 
					&mod->move_tool.prev_pos);
			}
			break;
		default:
			return FALSE;
		}
		mod->tool_type = TOOL_TYPE_NONE;
		return TRUE;
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

static boolean obj_transform(struct ae_object_module * mod, struct ap_object * obj)
{
	struct au_pos pos = obj->position;
	bool move = false;
	bool changed = false;
	move |= ImGui::DragFloat("Position X", &pos.x);
	move |= ImGui::DragFloat("Position Y", &pos.y);
	move |= ImGui::DragFloat("Position Z", &pos.z);
	changed |= ImGui::DragFloat("Scale X", &obj->scale.x, 0.001f);
	changed |= ImGui::DragFloat("Scale Y", &obj->scale.y, 0.001f);
	changed |= ImGui::DragFloat("Scale Z", &obj->scale.z, 0.001f);
	changed |= ImGui::DragFloat("Rotation X", &obj->rotation_x);
	changed |= ImGui::DragFloat("Rotation Y", &obj->rotation_y);
	if (move)
		ap_object_move_object(mod->ap_object, obj, &pos);
	return (move | changed);
}

static void render_properties(struct ae_object_module * mod)
{
	struct ap_object * obj = mod->active_object;
	struct ac_object * objc;
	struct ap_event_manager_attachment * eventattachment;
	boolean changed = FALSE;
	char label[128];
	if (!ImGui::Begin("Properties", (bool *)&mod->display_properties) || !obj) {
		ImGui::End();
		return;
	}
	objc = ac_object_get_object(mod->ac_object, obj);
	ImGui::InputScalar("Object ID", ImGuiDataType_U32, 
		&obj->object_id, NULL, NULL, NULL, 
		ImGuiInputTextFlags_ReadOnly);
	snprintf(label, sizeof(label), "[%u] %s", obj->tid, obj->temp->name);
	if (ImGui::Button(label, ImVec2(ImGui::GetContentRegionAvail().x, 25.0f)))
		mod->select_object_template = true;
	changed |= obj_transform(mod, obj);
	changed |= (boolean)rendertypeprops(&obj->object_type);
	changed |= (boolean)renderclienttypeprops(&objc->object_type);
	eventattachment = ap_event_manager_get_attachment(mod->ap_event_manager, obj);
	changed |= ae_event_refinery_render_as_node(mod->ae_event_refinery, obj, 
		eventattachment);
	changed |= ae_event_teleport_render_as_node(mod->ae_event_teleport, obj, 
		eventattachment);
	changed |= ae_event_binding_render_as_node(mod->ae_event_binding, obj, 
		eventattachment);
	if (changed)
		objc->sector->flags |= AC_OBJECT_SECTOR_HAS_CHANGES;
	ImGui::End();
}

static void rendertemplateselectionwindow(struct ae_object_module * mod)
{
	ImVec2 size = ImVec2(350.0f, 400.0f);
	ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter() - (size / 2.0f);
	size_t index = 0;
	struct ap_object_template * temp;
	if (!mod->select_object_template)
		return;
	if (!mod->active_object) {
		mod->select_object_template = false;
		return;
	}
	ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Select Object Template", &mod->select_object_template)) {
		ImGui::End();
		return;
	}
	ImGui::InputText("Search", mod->select_object_template_search_input, 
		sizeof(mod->select_object_template_search_input));
	while (temp = ap_object_iterate_templates(mod->ap_object, &index)) {
		struct ac_object_template * attachment = ac_object_get_template(temp);
		char label[128];
		snprintf(label, sizeof(label), "[%05u] %s", temp->tid, temp->name);
		if (!strisempty(mod->select_object_template_search_input) && 
			!stristr(label, mod->select_object_template_search_input)) {
			continue;
		}
		if (ImGui::Selectable(label)) {
			ac_object_release_template(mod->ac_object,
				ac_object_get_template(mod->active_object->temp));
			ac_object_reference_template(mod->ac_object, attachment);
			mod->active_object->temp = temp;
			mod->active_object->tid = temp->tid;
			ac_object_get_object(mod->ac_object, mod->active_object)->sector->flags |= 
				AC_OBJECT_SECTOR_HAS_CHANGES;
			mod->select_object_template = false;
			mod->select_object_template_search_input[0] = '\0';
			break;
		}
	}
	ImGui::End();
}

static void renderaddobjectpopup(struct ae_object_module * mod)
{
	ImVec2 size = ImVec2(350.0f, 400.0f);
	ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter() - (size / 2.0f);
	size_t index = 0;
	struct ap_object_template * temp;
	if (!mod->add_object)
		return;
	ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Add Object", &mod->add_object)) {
		ImGui::End();
		return;
	}
	ImGui::InputText("Search", mod->add_object_search_input, 
		sizeof(mod->add_object_search_input));
	while (temp = ap_object_iterate_templates(mod->ap_object, &index)) {
		struct ac_object_template * attachment = ac_object_get_template(temp);
		char label[128];
		snprintf(label, sizeof(label), "[%05u] %s", temp->tid, temp->name);
		if (!strisempty(mod->add_object_search_input) && 
			!stristr(label, mod->add_object_search_input)) {
			continue;
		}
		if (ImGui::Selectable(label)) {
			struct ap_object * obj = ap_object_create(mod->ap_object);
			struct ac_object * objc = ac_object_get_object(mod->ac_object, obj);
			struct ac_camera * cam = ac_camera_get_main(mod->ac_camera);
			struct au_pos pos = { cam->center[0], cam->center[1], cam->center[2] };
			obj->tid = temp->tid;
			obj->temp = temp;
			obj->scale.x = 1.0f;
			obj->scale.y = 1.0f;
			obj->scale.z = 1.0f;
			obj->object_type = temp->type;
			objc->object_type = attachment->object_type;
			ap_object_move_object(mod->ap_object, obj, &pos);
			ac_object_reference_template(mod->ac_object, attachment);
			mod->active_object = obj;
			mod->tool_type = TOOL_TYPE_MOVE;
			mod->move_tool.axis = MOVE_AXIS_NONE;
			mod->move_tool.prev_pos = pos;
			mod->move_tool.dx = 0.0f;
			mod->move_tool.dy = 0.0f;
			mod->add_object = false;
			break;
		}
	}
	ImGui::End();
}

void ae_object_imgui(struct ae_object_module * mod)
{
	render_outliner(mod);
	render_properties(mod);
	rendertemplateselectionwindow(mod);
	renderaddobjectpopup(mod);
}
