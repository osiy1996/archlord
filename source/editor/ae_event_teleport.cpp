#include "editor/ae_event_teleport.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_config.h"
#include "public/ap_event_teleport.h"
#include "public/ap_map.h"

#include "client/ac_imgui.h"

#include "editor/ae_editor_action.h"

#include <assert.h>

struct ae_event_teleport_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_event_teleport_module * ap_event_teleport;
	struct ap_map_module * ap_map;
	struct ae_editor_action_module * ae_editor_action;
	boolean has_pending_changes;
};

static boolean cbcommitchanges(
	struct ae_event_teleport_module * mod,
	void * data)
{
	char paths[2][1024];
	boolean encrypt[2] = { FALSE, TRUE };
	uint32_t i;
	if (!mod->has_pending_changes)
		return TRUE;
	snprintf(paths[0], sizeof(paths[0]), "%s/teleportpoint.ini", 
		ap_config_get(mod->ap_config, "ServerIniDir"));
	snprintf(paths[1], sizeof(paths[1]), "%s/ini/teleportpoint.ini",
		ap_config_get(mod->ap_config, "ClientDir"));
	for (i = 0; i < 2; i++) {
		if (ap_event_teleport_write_teleport_points(mod->ap_event_teleport,
				paths[i], encrypt[i])) {
			INFO("Saved teleport points to: %s.", paths[i]);
		}
		else {
			ERROR("Failed to save teleport points to: %s.", paths[i]);
		}
	}
	mod->has_pending_changes = FALSE;
	return TRUE;
}

static boolean onregister(
	struct ae_event_teleport_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_teleport, AP_EVENT_TELEPORT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_map, AP_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ae_editor_action, AE_EDITOR_ACTION_MODULE_NAME);
	ae_editor_action_add_callback(mod->ae_editor_action, 
		AE_EDITOR_ACTION_CB_COMMIT_CHANGES, 
		mod, (ap_module_default_t)cbcommitchanges);
	return TRUE;
}

struct ae_event_teleport_module * ae_event_teleport_create_module()
{
	struct ae_event_teleport_module * mod = (struct ae_event_teleport_module *)ap_module_instance_new(AE_EVENT_TELEPORT_MODULE_NAME, 
		sizeof(*mod), (ap_module_instance_register_t)onregister, NULL, NULL, NULL);
	return mod;
}

boolean ae_event_teleport_render_as_node(
	struct ae_event_teleport_module * mod,
	void * source,
	struct ap_event_manager_attachment * attachment)
{
	static bool checkbox;
	static char glossary[64];
	uint32_t i;
	uint32_t index;
	struct ap_event_manager_event * e = NULL;
	struct ap_event_teleport_event * teleport;
	bool changed;
	bool nodeopen;
	const char * glossaryentry;
	for (i = 0 ; i < attachment->event_count; i++) {
		if (attachment->events[i].function == AP_EVENT_MANAGER_FUNCTION_TELEPORT) {
			index = i;
			e = &attachment->events[i];
			break;
		}
	}
	checkbox = e != NULL;
	changed = ImGui::Checkbox("##EventTeleportEnabled", &checkbox);
	ImGui::SameLine();
	nodeopen = ImGui::TreeNodeEx("Event Function (Teleport)", ImGuiTreeNodeFlags_Framed);
	if (!e) {
		if (nodeopen)
			ImGui::TreePop();
		if (changed && checkbox) {
			e = ap_event_manager_add_function(mod->ap_event_manager, attachment,
				AP_EVENT_MANAGER_FUNCTION_TELEPORT, source);
			if (!e)
				return FALSE;
			return TRUE;
		}
		return FALSE;
	}
	teleport = (struct ap_event_teleport_event *)e->data;
	if (changed && !checkbox) {
		if (nodeopen)
			ImGui::TreePop();
		if (teleport->point) {
			ap_event_teleport_remove_point(mod->ap_event_teleport, teleport->point);
			mod->has_pending_changes = TRUE;
		}
		else {
			ap_event_manager_remove_function(mod->ap_event_manager, attachment, index);
		}
		return TRUE;
	}
	if (!nodeopen)
		return FALSE;
	changed = false;
	if (teleport->point) {
		struct ap_event_teleport_point * point = teleport->point;
		bool pointchanged = false;
		glossaryentry = ap_map_get_glossary(mod->ap_map, point->point_name);
		if (glossaryentry)
			strlcpy(glossary, glossaryentry, sizeof(glossary));
		else
			strcpy(glossary, "(missing glossary)");
		ImGui::InputText("Point Name", glossary, sizeof(glossary), 
			ImGuiInputTextFlags_ReadOnly);
		pointchanged |= ImGui::InputText("Description", point->description, 
			sizeof(point->description));
		pointchanged |= ImGui::InputFloat("Min. Radius", &point->radius_min);
		pointchanged |= ImGui::InputFloat("Max. Radius", &point->radius_max);
		if (pointchanged)
			mod->has_pending_changes = TRUE;
	}
	else {
		static char label[AP_MAP_MAX_REGION_NAME_SIZE];
		ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, 30.0f);
		ImGui::InputText("Point Name", label, sizeof(label));
		if (ImGui::Button("Establish Teleport Point", size)) {
			uint32_t i;
			for (i = 0; i < UINT16_MAX; i++) {
				struct ap_event_teleport_point * point;
				char pointname[AP_EVENT_TELEPORT_MAX_POINT_NAME_LENGTH + 1];
				snprintf(pointname, sizeof(pointname), "P%08u%u",
					ap_base_get_id(source), i);
				point = ap_event_teleport_add_point(mod->ap_event_teleport, 
					pointname, source);
				if (point) {
					strlcpy(teleport->point_name, pointname,
						sizeof(teleport->point_name));
					teleport->point = point;
					ap_map_add_glossary(mod->ap_map, pointname, label);
					changed = true;
					break;
				}
			}
			memset(label, 0, sizeof(label));
		}
	}
	ImGui::TreePop();
	/*
	while (TRUE) {
		struct ap_event_teleport_point * point = 
			ap_event_teleport_iterate(mod->ap_event_teleport, &pointindex);
		if (!point)
			break;
		glossaryentry = ap_map_get_glossary(mod->ap_map, 
			point->point_name);
		if (!strisempty(search) && 
			(!glossaryentry || !stristr(glossaryentry, search))) {
			continue;
		}
		if (glossaryentry) {
			snprintf(glossary, sizeof(glossary), "%s##%zu", 
				glossaryentry, pointindex);
		}
		else {
			snprintf(glossary, sizeof(glossary), "(missing_glossary)##%zu", 
				pointindex);
		}
		if (ImGui::Selectable(glossary)) {
			teleport->point = point;
			strlcpy(teleport->point_name, point->point_name,
				sizeof(teleport->point_name));
			pick = false;
			break;
		}
	}
	*/
	return changed;
}
