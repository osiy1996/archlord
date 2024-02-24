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
#include "editor/ae_transform_tool.h"

#include <ctype.h>
#include <unordered_map>
#include <vector>

#define MAX_EDIT_TEMPLATE_COUNT 32

size_t g_AeObjectTemplateOffset = SIZE_MAX;

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
	struct ae_transform_tool_module * ae_transform_tool;
	boolean display_outliner;
	boolean display_properties;
	struct ap_object ** objects;
	struct ap_object * active_object;
	bgfx_program_handle_t program;
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
	if (mod->active_object == obj) {
		ae_transform_tool_cancel_target(mod->ae_transform_tool);
		mod->active_object = NULL;
	}
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
				char srv[1024];
				char cli[1024];
				uint32_t div = (x / AP_SECTOR_DEFAULT_DEPTH) * 100 + (z / AP_SECTOR_DEFAULT_DEPTH);
				ac_object_export_sector(mod->ac_object, s, serverpath);
				if (make_path(srv, sizeof(srv), "%s/obj%05u.ini", serverpath, div) &&
					make_path(cli, sizeof(cli), "%s/obj%05u.ini", clientpath, div)) {
					copy_file(srv, cli, FALSE);
				}
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

static void cbtooltranslate(
	struct ae_object_module * mod, 
	const struct au_pos * pos)
{
	if (mod->active_object)
		ap_object_move_object(mod->ap_object, mod->active_object, pos);
}

static float getminheight(struct ae_object_module * mod, struct ap_object * obj)
{
	float minh;
	struct ac_object_template * temp = ac_object_get_template(obj->temp);
	if (ac_object_get_min_height(mod->ac_object, temp, &minh))
		return minh;
	else
		return 10.0f;
}

static void settooltarget(struct ae_object_module * mod, struct ap_object * obj)
{
	ae_transform_tool_set_target(mod->ae_transform_tool, mod, 
		(ae_transform_tool_translate_callback_t)cbtooltranslate, 
		obj, &obj->position, getminheight(mod, obj));
}

static boolean cbpick(
	struct ae_object_module * mod, 
	struct ae_editor_action_cb_pick * cb)
{
	struct ap_object * obj;
	struct au_pos eye;
	if (ae_transform_tool_complete_transform(mod->ae_transform_tool))
		return TRUE;
	if (mod->active_object)
		ae_transform_tool_cancel_target(mod->ae_transform_tool);
	obj = pick_object(mod, cb->camera, cb->x, cb->y);
	if (!obj)
		return TRUE;
	eye.x = cb->camera->eye[0];
	eye.y = cb->camera->eye[1];
	eye.z = cb->camera->eye[2];
	if (!cb->picked_any) {
		cb->picked_any = TRUE;
		cb->distance = au_distance2d(&obj->position, &eye);
		settooltarget(mod, obj);
		mod->active_object = obj;
	}
	return TRUE;
}

static void cbrenderoutliner(
	struct ae_object_module * mod,
	struct ae_editor_action_cb_render_outliner * cb)
{
	ImGuiListClipper clipper;
	vec_clear(mod->objects);
	ac_object_query_visible_objects(mod->ac_object, &mod->objects);
	clipper.Begin((int)vec_count(mod->objects));
	if (cb->selected_new_entity && mod->active_object)
		ae_transform_tool_cancel_target(mod->ae_transform_tool);
	while (clipper.Step()) {
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
			struct ap_object * obj = mod->objects[i];
			struct ac_object * objc = ac_object_get_object(mod->ac_object, obj);
			char label[128];
			snprintf(label, sizeof(label), "[OBJ] %u##%p", obj->object_id, obj);
			if (ImGui::Selectable(label, obj == mod->active_object) && 
				!cb->selected_new_entity) {
				ae_transform_tool_cancel_target(mod->ae_transform_tool);
				cb->selected_new_entity = TRUE;
				settooltarget(mod, obj);
				mod->active_object = obj;
			}
		}
	}
	clipper.End();
}

static boolean objtransform(struct ae_object_module * mod, struct ap_object * obj)
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

static boolean cbrenderproperties(struct ae_object_module * mod, void * data)
{
	struct ap_object * obj = mod->active_object;
	struct ac_object * objc;
	struct ap_event_manager_attachment * eventattachment;
	boolean changed = FALSE;
	char label[128];
	if (!obj)
		return TRUE;
	objc = ac_object_get_object(mod->ac_object, obj);
	ImGui::InputScalar("Object ID", ImGuiDataType_U32, 
		&obj->object_id, NULL, NULL, NULL, 
		ImGuiInputTextFlags_ReadOnly);
	snprintf(label, sizeof(label), "[%u] %s", obj->tid, obj->temp->name);
	if (ImGui::Button(label, ImVec2(ImGui::GetContentRegionAvail().x, 25.0f)))
		mod->select_object_template = true;
	changed |= objtransform(mod, obj);
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
	return TRUE;
}

static boolean cbrenderaddmenu(struct ae_object_module * mod, void * data)
{
	if (ImGui::Selectable("Object")) {
		mod->add_object = true;
		return TRUE;
	}
	return FALSE;
}

static boolean cbtransformcanceltarget(struct ae_object_module * mod, void * data)
{
	if (mod->active_object)
		mod->active_object = NULL;
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
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ae_transform_tool, AE_TRANSFORM_TOOL_MODULE_NAME);
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
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_PICK, mod, (ap_module_default_t)cbpick);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_RENDER_OUTLINER, mod, (ap_module_default_t)cbrenderoutliner);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_RENDER_PROPERTIES, mod, (ap_module_default_t)cbrenderproperties);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_RENDER_ADD_MENU, mod, (ap_module_default_t)cbrenderaddmenu);
	ae_transform_tool_add_callback(mod->ae_transform_tool, AE_TRANSFORM_TOOL_CB_CANCEL_TARGET, mod, (ap_module_default_t)cbtransformcanceltarget);
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

boolean ae_object_on_key_down(
	struct ae_object_module * mod,
	struct ac_camera * cam,
	uint32_t keycode)
{
	const boolean * state = ac_render_get_key_state(mod->ac_render);
	switch (keycode) {
	case SDLK_d:
		if (state[SDL_SCANCODE_LSHIFT] && mod->active_object) {
			mod->active_object = ap_object_duplicate(mod->ap_object, 
				mod->active_object);
		}
		break;
	case SDLK_DELETE:
		if (mod->active_object) {
			struct ap_object * obj = mod->active_object;
			ae_transform_tool_cancel_target(mod->ae_transform_tool);
			ap_object_destroy(mod->ap_object, obj);
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
			settooltarget(mod, obj);
			mod->active_object = obj;
			ae_transform_tool_switch_translate(mod->ae_transform_tool);
			mod->add_object = false;
			break;
		}
	}
	ImGui::End();
}

void ae_object_imgui(struct ae_object_module * mod)
{
	rendertemplateselectionwindow(mod);
	renderaddobjectpopup(mod);
}

static void add_object_geometry_to_json(
	struct ae_object_module * mod, 
	JSON_Object * json, 
	struct ap_object * obj, 
	struct ac_object_template * temp,
	struct ac_mesh_geometry * g)
{
	uint32_t splitid = 0;
	for (uint32_t j = 0; j < g->split_count; j++) {
		const struct ac_mesh_split * split = &g->splits[j];
		char name[128];
		std::vector<uint32_t> vertices;
		std::unordered_map<uint32_t, uint32_t> vertexmap;
		uint32_t matindex = -1;
		const struct ac_mesh_material * mat = &g->materials[split->material_index];
		char splitpath[1024];
		char proppath[1024];
		JSON_Value * arrayvalue;
		JSON_Array * arr;
		snprintf(splitpath, sizeof(splitpath), "object_splits.%u_%u", obj->object_id, splitid++);
		for (uint32_t k = 0; k < split->index_count; k++) {
			uint32_t idx;
			boolean add = TRUE;
			uint32_t vertexidx = g->indices[split->index_offset + k];
			for (idx = 0; idx < vertices.size(); idx++) {
				if (vertices[idx] == vertexidx) {
					break;
				}
			}
			if (std::find(vertices.begin(), vertices.end(), vertexidx) == vertices.end()) {
				vertexmap[vertexidx] = (uint32_t)vertices.size();
				vertices.push_back(vertexidx);
			}
		}
		arrayvalue = json_value_init_array();
		arr = json_value_get_array(arrayvalue);
		for (size_t k = 0; k < vertices.size(); k++) {
			const struct ac_mesh_vertex * v = &g->vertices[vertices[k]];
			JSON_Value * val = json_value_init_array();
			JSON_Array * a = json_value_get_array(val);
			json_array_append_number(a, v->position[0]);
			json_array_append_number(a, v->position[1]);
			json_array_append_number(a, v->position[2]);
			json_array_append_value(arr, val);
		}
		snprintf(proppath, sizeof(proppath), "%s.vertices", splitpath);
		json_object_dotset_value(json, proppath, arrayvalue);
		arrayvalue = json_value_init_array();
		arr = json_value_get_array(arrayvalue);
		for (size_t k = 0; k < vertices.size(); k++) {
			const struct ac_mesh_vertex * v = &g->vertices[vertices[k]];
			JSON_Value * val = json_value_init_array();
			JSON_Array * a = json_value_get_array(val);
			json_array_append_number(a, v->normal[0]);
			json_array_append_number(a, v->normal[1]);
			json_array_append_number(a, v->normal[2]);
			json_array_append_value(arr, val);
		}
		snprintf(proppath, sizeof(proppath), "%s.normals", splitpath);
		json_object_dotset_value(json, proppath, arrayvalue);
		arrayvalue = json_value_init_array();
		arr = json_value_get_array(arrayvalue);
		for (uint32_t k = 0; k < COUNT_OF(mat->tex_name); k++) {
			if (!strisempty(mat->tex_name[k])) {
				snprintf(name, sizeof(name), "%s.png", mat->tex_name[k]);
				json_array_append_string(arr, name);
			}
			else {
				json_array_append_string(arr, "");
			}
		}
		snprintf(proppath, sizeof(proppath), "%s.textures", splitpath);
		json_object_dotset_value(json, proppath, arrayvalue);
		for (uint32_t uv = 0; uv < 5; uv++) {
			const char * uvnames[]= { "uvbase", "uvalpha1", "uvcolor1",
				"uvalpha2", "uvcolor2" };
			arrayvalue = json_value_init_array();
			arr = json_value_get_array(arrayvalue);
			for (uint32_t k = 0; k < vertices.size(); k++) {
				const struct ac_mesh_vertex * v = &g->vertices[vertices[k]];
				JSON_Value * val = json_value_init_array();
				JSON_Array * a = json_value_get_array(val);
				json_array_append_number(a, v->texcoord[uv][0]);
				json_array_append_number(a, v->texcoord[uv][1]);
				json_array_append_value(arr, val);
			}
			snprintf(proppath, sizeof(proppath), "%s.%s", splitpath, uvnames[uv]);
			json_object_dotset_value(json, proppath, arrayvalue);
		}
		arrayvalue = json_value_init_array();
		arr = json_value_get_array(arrayvalue);
		for (uint32_t k = 0; k < split->index_count / 3; k++) {
			JSON_Value * val = json_value_init_array();
			JSON_Array * a = json_value_get_array(val);
			for (uint32_t l = 0; l < 3; l++) {
				uint32_t vertexidx = g->indices[split->index_offset + k * 3 + l];
				uint32_t mapped = vertexmap[vertexidx];
				json_array_append_number(a, mapped);
			}
			json_array_append_value(arr, val);
		}
		snprintf(proppath, sizeof(proppath), "%s.faces", splitpath);
		json_object_dotset_value(json, proppath, arrayvalue);
		arrayvalue = json_value_init_array();
		arr = json_value_get_array(arrayvalue);
		json_array_append_number(arr, obj->position.x);
		json_array_append_number(arr, obj->position.y);
		json_array_append_number(arr, obj->position.z);
		snprintf(proppath, sizeof(proppath), "%s.position", splitpath);
		json_object_dotset_value(json, proppath, arrayvalue);
		snprintf(proppath, sizeof(proppath), "%s.faces", splitpath);
		json_object_dotset_value(json, proppath, arrayvalue);
		arrayvalue = json_value_init_array();
		arr = json_value_get_array(arrayvalue);
		json_array_append_number(arr, obj->scale.x);
		json_array_append_number(arr, obj->scale.y);
		json_array_append_number(arr, obj->scale.z);
		snprintf(proppath, sizeof(proppath), "%s.scale", splitpath);
		json_object_dotset_value(json, proppath, arrayvalue);
		arrayvalue = json_value_init_array();
		arr = json_value_get_array(arrayvalue);
		json_array_append_number(arr, obj->rotation_x);
		json_array_append_number(arr, obj->rotation_y);
		snprintf(proppath, sizeof(proppath), "%s.rotation", splitpath);
		json_object_dotset_value(json, proppath, arrayvalue);
		snprintf(proppath, sizeof(proppath), "%s.alphablend", splitpath);
		json_object_dotset_boolean(json, proppath, 
			(temp->object_type & AC_OBJECT_TYPE_USE_ALPHA) != 0 ||
			(temp->object_type & AC_OBJECT_TYPE_USE_ALPHAFUNC) != 0 ||
			(temp->object_type & AC_OBJECT_TYPE_RENDER_UDA) != 0);
		snprintf(proppath, sizeof(proppath), "%s.showback", splitpath);
		json_object_dotset_boolean(json, proppath, 
			(temp->object_type & AC_OBJECT_TYPE_RENDER_UDA) != 0);
	}
}

static void add_object_to_json(
	struct ae_object_module * mod, 
	JSON_Object * json, 
	struct ap_object * obj)
{
	struct ac_object * o = ac_object_get_object(mod->ac_object, obj);
	struct ac_object_template * temp = ac_object_get_template(obj->temp);
	struct ac_object_template_group * grp;
	grp = temp->group_list;
	while (grp) {
		if (grp->clump) {
			struct ac_mesh_geometry * g = grp->clump->glist;
			while (g) {
				add_object_geometry_to_json(mod, json, obj, temp, g);
				g = g->next;
			}
		}
		grp = grp->next;
	}
}

void ae_object_export_scene(struct ae_object_module * mod, JSON_Object * json)
{
	struct ap_object ** list = 
		(struct ap_object **)vec_new_reserved(sizeof(*list), 128);
	ac_object_query_visible_objects(mod->ac_object, &list);
	uint32_t count = vec_count(list);
	for (uint32_t i = 0; i < count; i++) {
		add_object_to_json(mod, json, list[i]);
	}
	vec_free(list);
}

void ae_object_export_active(struct ae_object_module * mod)
{
	char path[512];
	if (!mod->active_object) {
		return;
	}
	make_path(path, sizeof(path), "content/exports/Object%u.json", 
		mod->active_object->object_id);
	file f = open_file(path, FILE_ACCESS_WRITE);
	if (!f) {
		ERROR("Failed to create file (%s).", path);
		return;
	}
	JSON_Value * root_value = json_value_init_object();
	JSON_Object * root_obj = json_value_get_object(root_value);
	add_object_to_json(mod, root_obj, mod->active_object);
	char * serialized_str = json_serialize_to_string_pretty(root_value);
	write_file(f, serialized_str, strlen(serialized_str));
	close_file(f);
	json_value_free(root_value);
}
