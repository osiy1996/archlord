#include "editor/ae_npc.h"

#include "core/core.h"
#include "core/file_system.h"
#include "core/malloc.h"
#include "core/log.h"
#include "core/string.h"
#include "core/vector.h"

#include "task/task.h"

#include "utility/au_md5.h"

#include "public/ap_admin.h"
#include "public/ap_character.h"
#include "public/ap_config.h"
#include "public/ap_map.h"
#include "public/ap_sector.h"

#include "client/ac_camera.h"
#include "client/ac_character.h"
#include "client/ac_imgui.h"
#include "client/ac_mesh.h"
#include "client/ac_renderware.h"
#include "client/ac_render.h"
#include "client/ac_terrain.h"

#include "editor/ae_editor_action.h"
#include "editor/ae_event_binding.h"
#include "editor/ae_event_refinery.h"
#include "editor/ae_event_teleport.h"
#include "editor/ae_texture.h"


#include <assert.h>
#include <ctype.h>
#include <string.h>

#define MAX_EDIT_COUNT 32

struct ae_npc_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	struct ap_character_module * ap_character;
	struct ap_event_manager_module * ap_event_manager;
	struct ac_camera_module * ac_camera;
	struct ac_character_module * ac_character;
	struct ac_render_module * ac_render;
	struct ac_terrain_module * ac_terrain;
	struct ae_editor_action_module * ae_editor_action;
	struct ap_admin npc_admin;
	bool add_npc;
	char add_npc_search_input[256];
	bool display_npc_editor;
	uint32_t npcs_in_edit[MAX_EDIT_COUNT];
	uint32_t npcs_in_edit_count;
	char npc_editor_search_input[128];
	boolean has_pending_changes;
	struct ap_character * active_npc;
	bool select_npc_template;
};

static boolean cbcharinitstatic(
	struct ae_npc_module * mod,
	struct ap_character_cb_init_static * cb)
{
	void * obj = ap_admin_add_object_by_id(&mod->npc_admin, cb->character->id);
	if (!obj) {
		ERROR("Failed to add NPC (%s).", cb->character->name);
		return FALSE;
	}
	*(struct ap_character **)obj = cb->character;
	cb->acquired_ownership = TRUE;
	return TRUE;
}

static boolean cbcommitchanges(
	struct ae_npc_module * mod,
	void * data)
{
	char serverpath[1024];
	char clientpath[1024];
	snprintf(serverpath, sizeof(serverpath), "%s/npcs", 
		ap_config_get(mod->ap_config, "ServerIniDir"));
	snprintf(clientpath, sizeof(clientpath), "%s/ini",
		ap_config_get(mod->ap_config, "ClientDir"));
	return TRUE;
}

static boolean cbrenderviewmenu(struct ae_npc_module * mod, void * data)
{
	ImGui::Selectable("NPC Editor", &mod->display_npc_editor);
	return TRUE;
}

static boolean isinedit(
	struct ae_npc_module * mod, 
	struct ap_character * npc)
{
	uint32_t i;
	for (i = 0; i < mod->npcs_in_edit_count; i++) {
		if (mod->npcs_in_edit[i] == npc->id)
			return TRUE;
	}
	return FALSE;
}

static inline size_t eraseinedit(
	struct ae_npc_module * mod, 
	size_t index)
{
	memmove(&mod->npcs_in_edit[index], &mod->npcs_in_edit[index + 1],
		(size_t)--mod->npcs_in_edit_count * sizeof(mod->npcs_in_edit[0]));
	return index;
}

static void rendertemplateeditor(struct ae_npc_module * mod)
{
	ImVec2 size = ImVec2(350.0f, 300.0f);
	ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter() - (size / 2.0f);
	size_t index = 0;
	void * obj = NULL;
	ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("NPC Editor", &mod->display_npc_editor)) {
		ImGui::End();
		return;
	}
	ImGui::InputText("Search", mod->npc_editor_search_input, 
		sizeof(mod->npc_editor_search_input));
	while (ap_admin_iterate_id(&mod->npc_admin, &index, &obj)) {
		char label[128];
		struct ap_character * npc = *(struct ap_character **)obj;
		snprintf(label, sizeof(label), "[%04u] %s", npc->id, npc->name);
		if (!strisempty(mod->npc_editor_search_input) &&
			!stristr(label, mod->npc_editor_search_input)) {
			continue;
		}
		if (ImGui::Selectable(label) && 
			mod->npcs_in_edit_count < MAX_EDIT_COUNT &&
			!isinedit(mod, npc)) {
			mod->npcs_in_edit[mod->npcs_in_edit_count++] = npc->id;
		}
	}
	ImGui::End();
	for (index = 0; index < mod->npcs_in_edit_count; index++) {
		char label[128];
		bool open = true;
		bool changed = FALSE;
		struct ap_character * npc;
		obj = ap_admin_get_object_by_id(&mod->npc_admin, mod->npcs_in_edit[index]);
		if (!obj) {
			index = eraseinedit(mod, index);
			continue;
		}
		npc = *(struct ap_character **)obj;
		size = ImVec2(300.0f, 300.0f);
		center = ImGui::GetMainViewport()->GetWorkCenter() - (size / 2.0f);
		ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing);
		snprintf(label, sizeof(label), "NPC - [%04u] %s###NPC%u", 
			npc->id, npc->name, npc->id);
		if (!ImGui::Begin(label, &open) || !open) {
			if (!open)
				index = eraseinedit(mod, index);
			ImGui::End();
			continue;
		}
		struct ac_character_template * attachment = ac_character_get_template(npc->temp);
		changed |= ImGui::InputText("Name", npc->name, sizeof(npc->name));
		changed |= ImGui::DragFloat("Position X", &npc->pos.x);
		changed |= ImGui::DragFloat("Position Y", &npc->pos.y);
		changed |= ImGui::DragFloat("Position Z", &npc->pos.z);
		changed |= ImGui::DragFloat("Rotation X", &npc->rotation_x);
		changed |= ImGui::DragFloat("Rotation Y", &npc->rotation_y);
		if (changed)
			mod->has_pending_changes = TRUE;
		ImGui::End();
	}
}

static boolean cbrendereditors(struct ae_npc_module * mod, void * data)
{
	if (mod->display_npc_editor)
		rendertemplateeditor(mod);
	return TRUE;
}

static void createtransform(
	struct ap_character * npc, 
	struct ac_character_template * temp,
	mat4 m)
{
	vec3 p = { npc->pos.x, npc->pos.y, npc->pos.z };
	vec3 s = { temp->scale, temp->scale, temp->scale };
	glm_mat4_identity(m);
	glm_translate(m, p);
	glm_rotate_x(m, glm_rad(npc->rotation_x), m);
	glm_rotate_y(m, glm_rad(npc->rotation_y), m);
	glm_scale(m, s);
}

static void transformbsphere(
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

static void transformvertex(mat4 m, struct ac_mesh_vertex * v)
{
	vec3 c = { v->position[0], v->position[1], v->position[2] };
	glm_mat4_mulv3(m, c, 1.0f, c);
	v->position[0] = c[0];
	v->position[1] = c[1];
	v->position[2] = c[2];
}

static boolean raycasttriangle(
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
		transformvertex(m, &v[i]);
	return glm_ray_triangle(origin, dir, 
		v[0].position,
		v[1].position, 
		v[2].position,
		hit_distance);
}

static struct ap_character * picknpc(
	struct ae_npc_module * mod,
	struct ac_camera * cam,
	int mouse_x,
	int mouse_y)
{
	int w;
	int h;
	vec3 dir;
	size_t index = 0;
	boolean hit = FALSE;
	float closest = 0.0f;
	struct ap_character * hitnpc = NULL;
	void * obj;
	SDL_GetWindowSize(ac_render_get_window(mod->ac_render), &w, &h);
	if (mouse_x < 0 || mouse_y < 0 || mouse_x > w || mouse_y > h)
		return NULL;
	ac_camera_ray(cam, (mouse_x / (w * 0.5f)) - 1.f,
		1.f - (mouse_y / (h * 0.5f)), dir);
	while (ap_admin_iterate_id(&mod->npc_admin, &index, &obj)) {
		struct ap_character * npc = *(struct ap_character **)obj;
		struct ac_character_template * temp = ac_character_get_template(npc->temp);
		float rd;
		mat4 m;
		struct ac_mesh_geometry * g;
		struct au_pos eye = { cam->eye[0], cam->eye[1], cam->eye[2] };
		if (!temp->clump)
			continue;
		if (au_distance2d(&npc->pos, &eye) > ac_terrain_get_view_distance(mod->ac_terrain))
			continue;
		createtransform(npc, temp, m);
		g = temp->clump->glist;
		while (g) {
			RwSphere bs = g->bsphere;
			uint32_t k;
			/* Transform and hit-test 
			 * bounding-sphere. */
			transformbsphere(&bs, m);
			if (!bsphere_raycast(&bs, cam->eye, dir, &rd) ||
				(hit && closest < rd)) {
				g = g->next;
				continue;
			}
			for (k = 0; k < g->index_count / 3; k++) {
				const uint16_t * indices = &g->indices[k * 3];
				struct ac_mesh_vertex * v = g->vertices;
				float dist;
				if (raycasttriangle(m, cam->eye, dir, &v[indices[0]],
						&v[indices[1]], &v[indices[2]], &dist) && 
					(!hit || dist < closest)) {
					hit = TRUE;
					closest = dist;
					hitnpc = npc;
				}
			}
			g = g->next;
		}
	}
	return hitnpc;
}

static boolean cbpick(
	struct ae_npc_module * mod, 
	struct ae_editor_action_cb_pick * cb)
{
	struct ap_character * npc = picknpc(mod, cb->camera, cb->x, cb->y);
	struct au_pos eye = { cb->camera->eye[0], cb->camera->eye[1], cb->camera->eye[2] };
	float distance;
	mod->active_npc = NULL;
	if (!npc)
		return TRUE;
	distance = au_distance2d(&npc->pos, &eye);
	if (!cb->picked_any) {
		cb->picked_any = TRUE;
		cb->distance = distance;
		mod->active_npc = npc;
	}
	else if (cb->distance > distance) {
		cb->distance = distance;
		mod->active_npc = npc;
	}
	return TRUE;
}

static boolean npctransform(struct ae_npc_module * mod, struct ap_character * npc)
{
	struct au_pos pos = npc->pos;
	bool move = false;
	bool changed = false;
	move |= ImGui::DragFloat("Position X", &pos.x);
	move |= ImGui::DragFloat("Position Y", &pos.y);
	move |= ImGui::DragFloat("Position Z", &pos.z);
	changed |= ImGui::DragFloat("Rotation X", &npc->rotation_x);
	changed |= ImGui::DragFloat("Rotation Y", &npc->rotation_y);
	if (move)
		ap_character_move(mod->ap_character, npc, &pos);
	return (move | changed);
}

static void cbrenderproperties(struct ae_npc_module * mod, void * data)
{
	struct ap_character * npc = mod->active_npc;
	struct ap_event_manager_attachment * eventattachment;
	boolean changed = FALSE;
	char label[128];
	if (!npc)
		return;
	ImGui::InputScalar("NPC ID", ImGuiDataType_U32, 
		&npc->id, NULL, NULL, NULL, 
		ImGuiInputTextFlags_ReadOnly);
	snprintf(label, sizeof(label), "[%u] %s", npc->tid, npc->temp->name);
	if (ImGui::Button(label, ImVec2(ImGui::GetContentRegionAvail().x, 25.0f)))
		mod->select_npc_template = true;
	changed |= npctransform(mod, npc);
	eventattachment = ap_event_manager_get_attachment(mod->ap_event_manager, npc);
	if (changed)
		mod->has_pending_changes = TRUE;
}

static boolean onregister(
	struct ae_npc_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_camera, AC_CAMERA_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_character, AC_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_render, AC_RENDER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_terrain, AC_TERRAIN_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ae_editor_action, AE_EDITOR_ACTION_MODULE_NAME);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_INIT_STATIC, mod, (ap_module_default_t)cbcharinitstatic);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_COMMIT_CHANGES, mod, (ap_module_default_t)cbcommitchanges);
	ae_editor_action_add_view_menu_callback(mod->ae_editor_action, "NPC Editor", mod, (ap_module_default_t)cbrenderviewmenu);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_RENDER_EDITORS, mod, (ap_module_default_t)cbrendereditors);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_PICK, mod, (ap_module_default_t)cbpick);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_RENDER_PROPERTIES, mod, (ap_module_default_t)cbrenderproperties);
	return TRUE;
}

static boolean oninitialize(struct ae_npc_module * mod)
{
	return TRUE;
}

static void onclose(struct ae_npc_module * mod)
{
	size_t index = 0;
	void * obj = NULL;
	while (ap_admin_iterate_id(&mod->npc_admin, &index, &obj)) {
		struct ap_character * character = *(struct ap_character **)obj;
		ap_character_free(mod->ap_character, character);
	}
	ap_admin_clear_objects(&mod->npc_admin);
}

static void onshutdown(struct ae_npc_module * mod)
{
	ap_admin_destroy(&mod->npc_admin);
}

struct ae_npc_module * ae_npc_create_module()
{
	struct ae_npc_module * mod = (struct ae_npc_module *)ap_module_instance_new(
		AE_NPC_MODULE_NAME, sizeof(*mod), 
		(ap_module_instance_register_t)onregister, 
		(ap_module_instance_initialize_t)oninitialize, 
		(ap_module_instance_close_t)onclose, 
		(ap_module_instance_shutdown_t)onshutdown);
	ap_admin_init(&mod->npc_admin, sizeof(struct ap_character *), 128);
	return mod;
}

void ae_npc_render(struct ae_npc_module * mod)
{
	size_t index = 0;
	void * obj;
	float viewdistance = ac_terrain_get_view_distance(mod->ac_terrain);
	struct ac_camera * maincam = ac_camera_get_main(mod->ac_camera);
	struct au_pos cameracenter = { maincam->center[0], maincam->center[1], maincam->center[2] };
	while (ap_admin_iterate_id(&mod->npc_admin, &index, &obj)) {
		struct ap_character * npc = *(struct ap_character **)obj;
		if (au_distance2d(&npc->pos, &cameracenter) <= viewdistance)
			ac_character_render(mod->ac_character, npc);
	}
}
