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
#include "public/ap_event_manager.h"
#include "public/ap_event_npc_dialog.h"
#include "public/ap_event_npc_trade.h"
#include "public/ap_event_product.h"
#include "public/ap_event_skill_master.h"
#include "public/ap_event_quest.h"
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
#include "editor/ae_transform_tool.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>

#define MAX_EDIT_COUNT 32

struct ae_npc_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	struct ap_character_module * ap_character;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_event_npc_dialog_module * ap_event_npc_dialog;
	struct ap_event_npc_trade_module * ap_event_npc_trade;
	struct ac_camera_module * ac_camera;
	struct ac_character_module * ac_character;
	struct ac_render_module * ac_render;
	struct ac_terrain_module * ac_terrain;
	struct ae_editor_action_module * ae_editor_action;
	struct ae_transform_tool_module * ae_transform_tool;
	struct ap_admin npc_admin;
	bgfx_program_handle_t program;
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
	snprintf(serverpath, sizeof(serverpath), "%s/npc.ini", 
		ap_config_get(mod->ap_config, "ServerIniDir"));
	snprintf(clientpath, sizeof(clientpath), "%s/ini/npc.ini",
		ap_config_get(mod->ap_config, "ClientDir"));
	if (mod->has_pending_changes) {
		if (!ap_character_write_static(mod->ap_character, serverpath, FALSE, 
				&mod->npc_admin)) {
			ERROR("Failed to write NPCs.");
		}
		else if (!copy_file(serverpath, clientpath, FALSE)) {
			ERROR("Failed to copy npc.ini to client.");
		}
		else {
			mod->has_pending_changes = FALSE;
		}
	}
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

static boolean rendereventfunction(
	struct ae_npc_module * mod,
	enum ap_event_manager_function_type function,
	const char * checkbox_id,
	const char * label,
	void * source,
	struct ap_event_manager_attachment * attachment)
{
	static bool checkbox;
	uint32_t i;
	uint32_t index;
	struct ap_event_manager_event * e = NULL;
	bool changed;
	for (i = 0 ; i < attachment->event_count; i++) {
		if (attachment->events[i].function == function) {
			index = i;
			e = &attachment->events[i];
			break;
		}
	}
	checkbox = e != NULL;
	changed = ImGui::Checkbox(checkbox_id, &checkbox);
	ImGui::SameLine();
	ImGui::Text(label);
	if (changed) {
		if (checkbox) {
			if (!e) {
				e = ap_event_manager_add_function(mod->ap_event_manager, attachment,
					function, source);
				if (!e)
					return FALSE;
				return TRUE;
			}
		}
		else if (e) {
			ap_event_manager_remove_function(mod->ap_event_manager, attachment, index);
			return TRUE;
		}
	}
	return FALSE;
}

static boolean rendereventtrade(
	struct ae_npc_module * mod,
	void * source,
	struct ap_event_manager_attachment * attachment)
{
	static bool checkbox;
	uint32_t i;
	uint32_t index;
	struct ap_event_manager_event * e = NULL;
	struct ap_event_npc_trade_data * eventtrade;
	bool changed;
	bool nodeopen;
	uint32_t template_id;
	for (i = 0 ; i < attachment->event_count; i++) {
		if (attachment->events[i].function == AP_EVENT_MANAGER_FUNCTION_NPCTRADE) {
			index = i;
			e = &attachment->events[i];
			break;
		}
	}
	checkbox = e != NULL;
	changed = ImGui::Checkbox("##EventNPCTradeEnabled", &checkbox);
	ImGui::SameLine();
	nodeopen = ImGui::TreeNodeEx("Event Function (Trade)", ImGuiTreeNodeFlags_Framed);
	if (!e) {
		if (nodeopen)
			ImGui::TreePop();
		if (changed && checkbox) {
			e = ap_event_manager_add_function(mod->ap_event_manager, attachment,
				AP_EVENT_MANAGER_FUNCTION_NPCTRADE, source);
			if (!e)
				return FALSE;
			return TRUE;
		}
		return FALSE;
	}
	eventtrade = (struct ap_event_npc_trade_data *)e->data;
	if (changed && !checkbox) {
		if (nodeopen)
			ImGui::TreePop();
		ap_event_manager_remove_function(mod->ap_event_manager, attachment, index);
		return TRUE;
	}
	if (!nodeopen)
		return FALSE;
	changed = false;
	template_id = eventtrade->npc_trade_template_id;
	if (ImGui::InputScalar("Trade Template Id", ImGuiDataType_U32, &template_id)) {
		struct ap_event_npc_trade_template * temp = 
			ap_event_npc_trade_get_template(mod->ap_event_npc_trade, template_id);
		if (temp) {
			eventtrade->npc_trade_template_id = template_id;
			eventtrade->temp = temp;
			changed = true;
		}
	}
	ImGui::TreePop();
	return changed;
}

static boolean rendereventdialog(
	struct ae_npc_module * mod,
	void * source,
	struct ap_event_manager_attachment * attachment)
{
	static bool checkbox;
	uint32_t i;
	uint32_t index;
	struct ap_event_manager_event * e = NULL;
	struct ap_event_npc_dialog_data * eventdialog;
	bool changed;
	bool nodeopen;
	for (i = 0 ; i < attachment->event_count; i++) {
		if (attachment->events[i].function == AP_EVENT_MANAGER_FUNCTION_NPCDIALOG) {
			index = i;
			e = &attachment->events[i];
			break;
		}
	}
	checkbox = e != NULL;
	changed = ImGui::Checkbox("##EventNPCDialogEnabled", &checkbox);
	ImGui::SameLine();
	nodeopen = ImGui::TreeNodeEx("Event Function (Dialog)", ImGuiTreeNodeFlags_Framed);
	if (!e) {
		if (nodeopen)
			ImGui::TreePop();
		if (changed && checkbox) {
			e = ap_event_manager_add_function(mod->ap_event_manager, attachment,
				AP_EVENT_MANAGER_FUNCTION_NPCDIALOG, source);
			if (!e)
				return FALSE;
			return TRUE;
		}
		return FALSE;
	}
	eventdialog = (struct ap_event_npc_dialog_data *)e->data;
	if (changed && !checkbox) {
		if (nodeopen)
			ImGui::TreePop();
		ap_event_manager_remove_function(mod->ap_event_manager, attachment, index);
		return TRUE;
	}
	if (!nodeopen)
		return FALSE;
	changed = ImGui::InputScalar("Dialog Text Id", ImGuiDataType_U32, 
		&eventdialog->npc_dialog_text_id);
	ImGui::TreePop();
	return changed;
}

static boolean rendereventproduct(
	struct ae_npc_module * mod,
	void * source,
	struct ap_event_manager_attachment * attachment)
{
	static bool checkbox;
	uint32_t i;
	uint32_t index;
	struct ap_event_manager_event * e = NULL;
	struct ap_event_product_event * eventproduct;
	bool changed;
	bool nodeopen;
	for (i = 0 ; i < attachment->event_count; i++) {
		if (attachment->events[i].function == AP_EVENT_MANAGER_FUNCTION_PRODUCT) {
			index = i;
			e = &attachment->events[i];
			break;
		}
	}
	checkbox = e != NULL;
	changed = ImGui::Checkbox("##EventProductEnabled", &checkbox);
	ImGui::SameLine();
	nodeopen = ImGui::TreeNodeEx("Event Function (Product)", ImGuiTreeNodeFlags_Framed);
	if (!e) {
		if (nodeopen)
			ImGui::TreePop();
		if (changed && checkbox) {
			e = ap_event_manager_add_function(mod->ap_event_manager, attachment,
				AP_EVENT_MANAGER_FUNCTION_PRODUCT, source);
			if (!e)
				return FALSE;
			return TRUE;
		}
		return FALSE;
	}
	eventproduct = (struct ap_event_product_event *)e->data;
	if (changed && !checkbox) {
		if (nodeopen)
			ImGui::TreePop();
		ap_event_manager_remove_function(mod->ap_event_manager, attachment, index);
		return TRUE;
	}
	if (!nodeopen)
		return FALSE;
	changed = ImGui::InputScalar("Product Category", ImGuiDataType_U32, 
		&eventproduct->product_category);
	ImGui::TreePop();
	return changed;
}

static boolean rendereventskillmaster(
	struct ae_npc_module * mod,
	void * source,
	struct ap_event_manager_attachment * attachment)
{
	static bool checkbox;
	uint32_t i;
	uint32_t index;
	struct ap_event_manager_event * e = NULL;
	struct ap_event_skill_master_data * eventskillmaster;
	bool changed;
	bool nodeopen;
	for (i = 0 ; i < attachment->event_count; i++) {
		if (attachment->events[i].function == AP_EVENT_MANAGER_FUNCTION_SKILLMASTER) {
			index = i;
			e = &attachment->events[i];
			break;
		}
	}
	checkbox = e != NULL;
	changed = ImGui::Checkbox("##EventSkillMasterEnabled", &checkbox);
	ImGui::SameLine();
	nodeopen = ImGui::TreeNodeEx("Event Function (Skill Master)", ImGuiTreeNodeFlags_Framed);
	if (!e) {
		if (nodeopen)
			ImGui::TreePop();
		if (changed && checkbox) {
			e = ap_event_manager_add_function(mod->ap_event_manager, attachment,
				AP_EVENT_MANAGER_FUNCTION_SKILLMASTER, source);
			if (!e)
				return FALSE;
			return TRUE;
		}
		return FALSE;
	}
	eventskillmaster = (struct ap_event_skill_master_data *)e->data;
	if (changed && !checkbox) {
		if (nodeopen)
			ImGui::TreePop();
		ap_event_manager_remove_function(mod->ap_event_manager, attachment, index);
		return TRUE;
	}
	if (!nodeopen)
		return FALSE;
	changed = ImGui::InputScalar("Race", ImGuiDataType_U32, 
		&eventskillmaster->race);
	changed = ImGui::InputScalar("Class", ImGuiDataType_U32, 
		&eventskillmaster->class_);
	ImGui::TreePop();
	return changed;
}

static boolean rendereventquest(
	struct ae_npc_module * mod,
	void * source,
	struct ap_event_manager_attachment * attachment)
{
	static bool checkbox;
	uint32_t i;
	uint32_t index;
	struct ap_event_manager_event * e = NULL;
	struct ap_event_quest_event * eventquest;
	bool changed;
	bool nodeopen;
	for (i = 0 ; i < attachment->event_count; i++) {
		if (attachment->events[i].function == AP_EVENT_MANAGER_FUNCTION_QUEST) {
			index = i;
			e = &attachment->events[i];
			break;
		}
	}
	checkbox = e != NULL;
	changed = ImGui::Checkbox("##EventQuestEnabled", &checkbox);
	ImGui::SameLine();
	nodeopen = ImGui::TreeNodeEx("Event Function (Quest)", ImGuiTreeNodeFlags_Framed);
	if (!e) {
		if (nodeopen)
			ImGui::TreePop();
		if (changed && checkbox) {
			e = ap_event_manager_add_function(mod->ap_event_manager, attachment,
				AP_EVENT_MANAGER_FUNCTION_QUEST, source);
			if (!e)
				return FALSE;
			return TRUE;
		}
		return FALSE;
	}
	eventquest = (struct ap_event_quest_event *)e->data;
	if (changed && !checkbox) {
		if (nodeopen)
			ImGui::TreePop();
		ap_event_manager_remove_function(mod->ap_event_manager, attachment, index);
		return TRUE;
	}
	if (!nodeopen)
		return FALSE;
	changed = ImGui::InputScalar("Quest Group Id", ImGuiDataType_U32, 
		&eventquest->quest_group_id);
	ImGui::TreePop();
	return changed;
}

static boolean rendernpcprops(struct ae_npc_module * mod, struct ap_character * npc)
{
	struct ap_event_manager_attachment * eventattachment;
	boolean changed = FALSE;
	char label[128];
	ImGui::InputScalar("ID", ImGuiDataType_U32, 
		&npc->id, NULL, NULL, NULL, 
		ImGuiInputTextFlags_ReadOnly);
	ImGui::InputText("Name", npc->name, sizeof(npc->name));
	snprintf(label, sizeof(label), "[%u] %s", npc->tid, npc->temp->name);
	if (ImGui::Button(label, ImVec2(ImGui::GetContentRegionAvail().x, 25.0f)))
		mod->select_npc_template = true;
	changed |= npctransform(mod, npc);
	eventattachment = ap_event_manager_get_attachment(mod->ap_event_manager, npc);
	changed |= rendereventfunction(mod, AP_EVENT_MANAGER_FUNCTION_AUCTION, 
		"##EventAuctionEnabled", "Event Function (Auction)", npc, eventattachment);
	changed |= rendereventfunction(mod, AP_EVENT_MANAGER_FUNCTION_BANK, 
		"##EventBankEnabled", "Event Function (Bank)", npc, eventattachment);
	changed |= rendereventdialog(mod, npc, eventattachment);
	changed |= rendereventfunction(mod, AP_EVENT_MANAGER_FUNCTION_GUILD, 
		"##EventGuildEnabled", "Event Function (Guild)", npc, eventattachment);
	changed |= rendereventfunction(mod, AP_EVENT_MANAGER_FUNCTION_GUILD_WAREHOUSE, 
		"##EventGuildBankEnabled", "Event Function (Guild Bank)", npc, eventattachment);
	changed |= rendereventproduct(mod, npc, eventattachment);
	changed |= rendereventquest(mod, npc, eventattachment);
	changed |= rendereventfunction(mod, AP_EVENT_MANAGER_FUNCTION_REMISSION, 
		"##EventRemissionEnabled", "Event Function (Remission)", npc, eventattachment);
	changed |= rendereventfunction(mod, AP_EVENT_MANAGER_FUNCTION_SIEGEWAR_NPC, 
		"##EventSiegeEnabled", "Event Function (Siege)", npc, eventattachment);
	changed |= rendereventskillmaster(mod, npc, eventattachment);
	changed |= rendereventfunction(mod, AP_EVENT_MANAGER_FUNCTION_TAX, 
		"##EventTaxEnabled", "Event Function (Tax)", npc, eventattachment);
	changed |= rendereventtrade(mod, npc, eventattachment);
	changed |= rendereventfunction(mod, AP_EVENT_MANAGER_FUNCTION_ITEMCONVERT, 
		"##EventUpgradeEnabled", "Event Function (Upgrade)", npc, eventattachment);
	changed |= rendereventfunction(mod, AP_EVENT_MANAGER_FUNCTION_WANTEDCRIMINAL, 
		"##EventWantedEnabled", "Event Function (Wanted)", npc, eventattachment);
	if (changed)
		mod->has_pending_changes = TRUE;
	return TRUE;
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
		struct ap_character * npc;
		struct ac_character_template * attachment;
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
		attachment = ac_character_get_template(npc->temp);
		if (rendernpcprops(mod, npc))
			mod->has_pending_changes = TRUE;
		ImGui::End();
	}
}

static float getminheight(struct ap_character * npc)
{
	float minh;
	struct ac_character_template * temp = ac_character_get_template(npc->temp);
	boolean first = TRUE;
	struct ac_mesh_geometry * cg;
	if (!temp->clump)
		return 10.0f;
	cg = temp->clump->glist;
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
	return first ? 10.0f : minh;
}

static void cbtooltranslate(
	struct ae_npc_module * mod, 
	const struct au_pos * pos)
{
	if (mod->active_npc)
		ap_character_move(mod->ap_character, mod->active_npc, pos);
}

static void settooltarget(struct ae_npc_module * mod, struct ap_character * npc)
{
	ae_transform_tool_set_target(mod->ae_transform_tool, mod, 
		(ae_transform_tool_translate_callback_t)cbtooltranslate, 
		npc, &npc->pos, getminheight(npc));
}

static uint32_t getuniqueid(struct ae_npc_module * mod)
{
	uint32_t i;
	for (i = 1; i < 10000; i++) {
		if (!ap_admin_get_object_by_id(&mod->npc_admin, i))
			return i;
	}
	return 0;
}

static void renderaddnpcpopup(struct ae_npc_module * mod)
{
	ImVec2 size = ImVec2(350.0f, 400.0f);
	ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter() - (size / 2.0f);
	size_t index = 0;
	struct ap_character_template * temp;
	ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Add NPC", &mod->add_npc)) {
		ImGui::End();
		return;
	}
	ImGui::InputText("Search", mod->add_npc_search_input, 
		sizeof(mod->add_npc_search_input));
	while (temp = ap_character_iterate_templates(mod->ap_character, &index)) {
		char label[128];
		if (!(temp->char_type & AP_CHARACTER_TYPE_NPC))
			continue;
		snprintf(label, sizeof(label), "[%04u] %s", temp->tid, temp->name);
		if (!strisempty(mod->add_npc_search_input) && 
			!stristr(label, mod->add_npc_search_input)) {
			continue;
		}
		if (ImGui::Selectable(label)) {
			struct ap_character * npc = ap_character_new(mod->ap_character);
			struct ac_camera * cam = ac_camera_get_main(mod->ac_camera);
			struct au_pos pos = { cam->center[0], cam->center[1], cam->center[2] };
			struct ap_character_cb_init_static cb = { 0 };
			npc->id = getuniqueid(mod);
			if (!npc->id) {
				WARN("Failed to assign unique NPC id.");
				ap_character_free(mod->ap_character, npc);
				assert(0);
				continue;
			}
			npc->login_status = AP_CHARACTER_STATUS_IN_GAME_WORLD;
			npc->char_type = AP_CHARACTER_TYPE_NPC;
			npc->pos = pos;
			npc->npc_display_for_map = TRUE;
			npc->npc_display_for_nameboard = TRUE;
			ap_character_set_template(mod->ap_character, npc, temp);
			cb.character = npc;
			cb.acquired_ownership = FALSE;
			if (!ap_module_enum_callback(mod->ap_character, 
					AP_CHARACTER_CB_INIT_STATIC, &cb)) {
				ERROR("Failed to initialize static character.");
				ap_character_free(mod->ap_character, npc);
				continue;
			}
			if (!cb.acquired_ownership) {
				WARN("Static character ownership was not acquired by any module.");
				ap_character_free(mod->ap_character, npc);
				assert(0);
				continue;
			}
			settooltarget(mod, npc);
			mod->active_npc = npc;
			ae_transform_tool_switch_translate(mod->ae_transform_tool);
			mod->add_npc = false;
			mod->has_pending_changes = TRUE;
			break;
		}
	}
	ImGui::End();
}

static boolean cbrendereditors(struct ae_npc_module * mod, void * data)
{
	if (mod->display_npc_editor)
		rendertemplateeditor(mod);
	if (mod->add_npc)
		renderaddnpcpopup(mod);
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
	struct ap_character * npc;
	struct au_pos eye;
	if (ae_transform_tool_complete_transform(mod->ae_transform_tool))
		return TRUE;
	if (mod->active_npc)
		ae_transform_tool_cancel_target(mod->ae_transform_tool);
	npc = picknpc(mod, cb->camera, cb->x, cb->y);
	if (!npc)
		return TRUE;
	eye.x = cb->camera->eye[0];
	eye.y = cb->camera->eye[1];
	eye.z = cb->camera->eye[2];
	if (!cb->picked_any) {
		cb->picked_any = TRUE;
		cb->distance = au_distance2d(&npc->pos, &eye);
		settooltarget(mod, npc);
		mod->active_npc = npc;
	}
	return TRUE;
}

static boolean cbrenderproperties(struct ae_npc_module * mod, void * data)
{
	struct ap_character * npc = mod->active_npc;
	if (npc && rendernpcprops(mod, npc))
		mod->has_pending_changes = TRUE;
	return TRUE;
}

static boolean cbhandleinput(
	struct ae_npc_module * mod,
	struct ae_editor_action_cb_handle_input * cb)
{
	const SDL_Event * e = cb->input;
	switch (cb->input->type) {
	case SDL_KEYDOWN: {
		const boolean * state = ac_render_get_key_state(mod->ac_render);
		switch (e->key.keysym.sym) {
		case SDLK_DELETE:
			if (mod->active_npc) {
				struct ap_character * npc = mod->active_npc;
				ae_transform_tool_cancel_target(mod->ae_transform_tool);
				ap_admin_remove_object_by_id(&mod->npc_admin, npc->id);
				ap_character_free(mod->ap_character, npc);
				mod->active_npc = NULL;
			}
			break;
		case SDLK_KP_PERIOD:
			if (mod->active_npc) {
				vec3 center = { mod->active_npc->pos.x,
					mod->active_npc->pos.y,
					mod->active_npc->pos.z };
				ac_camera_focus_on(ac_camera_get_main(mod->ac_camera), center, 
					1000.0f);
				return TRUE;
			}
			break;
		}
		return FALSE;
		break;
	}
	}
	return FALSE;
}

static boolean cbrenderaddmenu(struct ae_npc_module * mod, void * data)
{
	if (ImGui::Selectable("NPC")) {
		mod->add_npc = true;
		return TRUE;
	}
	return FALSE;
}

static boolean cbtransformcanceltarget(struct ae_npc_module * mod, void * data)
{
	if (mod->active_npc)
		mod->active_npc = NULL;
	return TRUE;
}

static boolean onregister(
	struct ae_npc_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_npc_dialog, AP_EVENT_NPC_DIALOG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_npc_trade, AP_EVENT_NPC_TRADE_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_camera, AC_CAMERA_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_character, AC_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_render, AC_RENDER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_terrain, AC_TERRAIN_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ae_editor_action, AE_EDITOR_ACTION_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ae_transform_tool, AE_TRANSFORM_TOOL_MODULE_NAME);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_INIT_STATIC, mod, (ap_module_default_t)cbcharinitstatic);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_COMMIT_CHANGES, mod, (ap_module_default_t)cbcommitchanges);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_RENDER_EDITORS, mod, (ap_module_default_t)cbrendereditors);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_PICK, mod, (ap_module_default_t)cbpick);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_RENDER_PROPERTIES, mod, (ap_module_default_t)cbrenderproperties);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_RENDER_ADD_MENU, mod, (ap_module_default_t)cbrenderaddmenu);
	ae_editor_action_add_view_menu_callback(mod->ae_editor_action, "NPC Editor", mod, (ap_module_default_t)cbrenderviewmenu);
	ae_editor_action_add_input_handler(mod->ae_editor_action, mod, (ap_module_default_t)cbhandleinput);
	ae_transform_tool_add_callback(mod->ae_transform_tool, AE_TRANSFORM_TOOL_CB_CANCEL_TARGET, mod, (ap_module_default_t)cbtransformcanceltarget);
	return TRUE;
}

static boolean createshaders(struct ae_npc_module * mod)
{
	bgfx_shader_handle_t vsh;
	bgfx_shader_handle_t fsh;
	if (!ac_render_load_shader("ae_npc_outline.vs", &vsh)) {
		ERROR("Failed to load vertex shader.");
		return FALSE;
	}
	if (!ac_render_load_shader("ae_npc_outline.fs", &fsh)) {
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

static boolean oninitialize(struct ae_npc_module * mod)
{
	if (!createshaders(mod)) {
		ERROR("Failed to create shaders.");
		return FALSE;
	}
	return TRUE;
}

static void onclose(struct ae_npc_module * mod)
{
	size_t index = 0;
	void * obj = NULL;
	if (mod->has_pending_changes) {
		WARN("There are pending NPC changes that will be lost.");
	}
	while (ap_admin_iterate_id(&mod->npc_admin, &index, &obj)) {
		struct ap_character * character = *(struct ap_character **)obj;
		ap_character_free(mod->ap_character, character);
	}
	ap_admin_clear_objects(&mod->npc_admin);
}

static void onshutdown(struct ae_npc_module * mod)
{
	ap_admin_destroy(&mod->npc_admin);
	bgfx_destroy_program(mod->program);
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
		struct ac_character_render_data render = { 0 };
		render.program = BGFX_INVALID_HANDLE;
		render.no_texture = FALSE;
		render.state = BGFX_STATE_WRITE_MASK |
			BGFX_STATE_DEPTH_TEST_LESS |
			BGFX_STATE_CULL_CW;
		if (au_distance2d(&npc->pos, &cameracenter) <= viewdistance)
			ac_character_render(mod->ac_character, npc, &render);
	}
	if (mod->active_npc) {
		struct ac_character_render_data render = { 0 };
		render.state = BGFX_STATE_WRITE_MASK | BGFX_STATE_DEPTH_TEST_ALWAYS |
			BGFX_STATE_BLEND_FUNC_SEPARATE(
				BGFX_STATE_BLEND_SRC_ALPHA,
				BGFX_STATE_BLEND_INV_SRC_ALPHA,
				BGFX_STATE_BLEND_ZERO,
				BGFX_STATE_BLEND_ONE);
		render.program = mod->program;
		render.no_texture = TRUE;
		ac_character_render(mod->ac_character, mod->active_npc, &render);
	}
}
