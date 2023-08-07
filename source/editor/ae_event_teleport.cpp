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
	bool display_group_editor;
	char group_search_input[AP_MAP_MAX_REGION_NAME_SIZE];
	bool filter_empty_groups;
	bool add_point;
	struct ap_event_teleport_group * add_point_to_group;
	boolean add_point_to_source;
	char point_search_input[AP_EVENT_TELEPORT_MAX_POINT_NAME_LENGTH + 1];
};

static boolean cbcommitchanges(
	struct ae_event_teleport_module * mod,
	void * data)
{
	char paths[2][1024];
	boolean encrypt[2] = { FALSE, TRUE };
	uint32_t i;
	boolean success = TRUE;
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
			success = FALSE;
		}
	}
	snprintf(paths[1], sizeof(paths[1]), "%s/ini/teleportgroup.ini",
		ap_config_get(mod->ap_config, "ClientDir"));
	if (ap_event_teleport_write_teleport_groups(mod->ap_event_teleport,
			paths[1], encrypt[1])) {
		INFO("Saved teleport groups to: %s.", paths[1]);
	}
	else {
		ERROR("Failed to save teleport groups to: %s.", paths[1]);
		success = FALSE;
	}
	if (success)
		mod->has_pending_changes = FALSE;
	return TRUE;
}

static boolean cbrenderviewmenu(struct ae_event_teleport_module * mod, void * data)
{
	ImGui::Selectable("Teleport Group Editor", &mod->display_group_editor);
	return TRUE;
}

static void rendergroupeditor(struct ae_event_teleport_module * mod)
{
	ImVec2 size = ImVec2(400.0f, 300.0f);
	ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter() - (size / 2.0f);
	size_t index = 0;
	struct ap_event_teleport_group * group;
	struct ap_event_teleport_point * point;
	ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Teleport Group Editor", &mod->display_group_editor)) {
		ImGui::End();
		return;
	}
	ImGui::InputText("Search", mod->group_search_input, sizeof(mod->group_search_input));
	ImGui::Checkbox("Empty Only", &mod->filter_empty_groups);
	size = ImVec2(ImGui::GetContentRegionAvail().x, 25.0f);
	if (ImGui::Button("Add Empty Teleport Group", size)) {
		uint32_t i;
		char name[AP_EVENT_TELEPORT_MAX_GROUP_NAME_LENGTH + 1];
		group = NULL;
		for (i = 1; i < 10000 && !group; i++) {
			snprintf(name, sizeof(name), "TeleportGroup%u", i);
			group = ap_event_teleport_add_group(mod->ap_event_teleport, name);
		}
	}
	while (group = ap_event_teleport_iterate_groups(mod->ap_event_teleport, &index)) {
		uint32_t i;
		uint32_t count = MAX(group->source_point_count, group->destination_point_count) + 1;
		char label[128];
		bool filterout = !strisempty(mod->group_search_input);
		if (mod->filter_empty_groups && count > 1)
			continue;
		if (filterout) {
			for (i = 0; i < group->source_point_count; i++) {
				const char * glossary = ap_map_get_glossary(mod->ap_map, 
					group->source_points[i]->point_name);
				if (glossary && stristr(glossary, mod->group_search_input)) {
					filterout = false;
					break;
				}
			}
			for (i = 0; i < group->destination_point_count; i++) {
				const char * glossary = ap_map_get_glossary(mod->ap_map, 
					group->destination_points[i]->point_name);
				if (glossary && stristr(glossary, mod->group_search_input)) {
					filterout = false;
					break;
				}
			}
		}
		if (filterout)
			continue;
		ImGui::Separator();
		snprintf(label, sizeof(label), "Group Table %zu", index);
		if (!ImGui::BeginTable(label, 2))
			continue;
		ImGui::TableSetupColumn("Source Teleport Point");
		ImGui::TableSetupColumn("Destination Teleport Point");
		ImGui::TableHeadersRow();
		for (i = 0; i < count; i++) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (i < group->source_point_count) {
				struct ap_event_teleport_point * point = group->source_points[i];
				const char * glossary = ap_map_get_glossary(mod->ap_map, 
					point->point_name);
				snprintf(label, sizeof(label), "X##S%s", point->point_name);
				if (ImGui::SmallButton(label)) {
					uint32_t j;
					for (j = 0; j < point->target_group_count; j++) {
						if (point->target_groups[j] == group) {
							memmove(&point->target_groups[j], 
								&point->target_groups[j + 1],
								(size_t)--point->target_group_count * sizeof(point->target_groups[0]));
							break;
						}
					}
					memmove(&group->source_points[i], &group->source_points[i + 1],
						(size_t)--group->source_point_count * sizeof(group->source_points[0]));
					mod->has_pending_changes = TRUE;
				}
				ImGui::SameLine();
				if (glossary)
					ImGui::Text("%s", glossary);
				else
					ImGui::Text("(missing glossary)", index);
			}
			else if (i == group->source_point_count && 
				group->source_point_count < AP_EVENT_TELEPORT_MAX_GROUP_POINT_COUNT &&
				ImGui::SmallButton("+##Source")) {
				mod->add_point = true;
				mod->add_point_to_group = group;
				mod->add_point_to_source = TRUE;
			}
			ImGui::TableNextColumn();
			if (i < group->destination_point_count) {
				struct ap_event_teleport_point * point = group->destination_points[i];
				const char * glossary = ap_map_get_glossary(mod->ap_map, 
					point->point_name);
				snprintf(label, sizeof(label), "X##D%s", point->point_name);
				if (ImGui::SmallButton(label)) {
					uint32_t j;
					for (j = 0; j < point->group_count; j++) {
						if (point->groups[j] == group) {
							memmove(&point->groups[j], &point->groups[j + 1],
								(size_t)--point->group_count * sizeof(point->groups[0]));
							break;
						}
					}
					memmove(&group->destination_points[i], 
						&group->destination_points[i + 1],
						(size_t)--group->destination_point_count * sizeof(group->destination_points[0]));
					mod->has_pending_changes = TRUE;
				}
				ImGui::SameLine();
				if (glossary)
					ImGui::Text("%s", glossary);
				else
					ImGui::Text("(missing glossary)", index);
			}
			else if (i == group->destination_point_count && 
				group->destination_point_count < AP_EVENT_TELEPORT_MAX_GROUP_POINT_COUNT &&
				ImGui::SmallButton("+##Destination")) {
				mod->add_point = true;
				mod->add_point_to_group = group;
				mod->add_point_to_source = FALSE;
			}
		}
		ImGui::EndTable();
	}
	ImGui::End();
	if (!mod->add_point)
		return;
	size = ImVec2(200.0f, 400.0f);
	center = ImGui::GetMainViewport()->GetWorkCenter() - (size / 2.0f);
	ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Add Teleport Point", &mod->add_point)) {
		ImGui::End();
		return;
	}
	ImGui::InputText("Search", mod->point_search_input, sizeof(mod->point_search_input));
	index = 0;
	while (point = ap_event_teleport_iterate(mod->ap_event_teleport, &index)) {
		const char * glossary = ap_map_get_glossary(mod->ap_map, 
			point->point_name);
		char label[128];
		if (!strisempty(mod->point_search_input) && 
			(!glossary || !stristr(glossary, mod->point_search_input))) {
			continue;
		}
		if (glossary)
			snprintf(label, sizeof(label), "%s##%zu", glossary, index);
		else
			snprintf(label, sizeof(label), "(missing_glossary)##%zu", index);
		if (ImGui::Selectable(label)) {
			mod->add_point = false;
			group = mod->add_point_to_group;
			if (mod->add_point_to_source) {
				if (group->source_point_count < COUNT_OF(group->source_points) &&
					point->target_group_count < COUNT_OF(point->target_groups)) {
					group->source_points[group->source_point_count++] = point;
					point->target_groups[point->target_group_count++] = group;
					mod->has_pending_changes = TRUE;
				}
			}
			else {
				if (group->destination_point_count < COUNT_OF(group->destination_points) &&
					point->group_count < COUNT_OF(point->groups)) {
					group->destination_points[group->destination_point_count++] = point;
					point->groups[point->group_count++] = group;
					mod->has_pending_changes = TRUE;
				}
			}
			break;
		}
	}
	ImGui::End();
}

static boolean cbrendereditors(struct ae_event_teleport_module * mod, void * data)
{
	if (mod->display_group_editor)
		rendergroupeditor(mod);
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
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_COMMIT_CHANGES, mod, (ap_module_default_t)cbcommitchanges);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_RENDER_EDITORS, mod, (ap_module_default_t)cbrendereditors);
	ae_editor_action_add_view_menu_callback(mod->ae_editor_action, "Teleport Group Editor", mod, (ap_module_default_t)cbrenderviewmenu);
	return TRUE;
}

struct ae_event_teleport_module * ae_event_teleport_create_module()
{
	struct ae_event_teleport_module * mod = (struct ae_event_teleport_module *)ap_module_instance_new(AE_EVENT_TELEPORT_MODULE_NAME, 
		sizeof(*mod), (ap_module_instance_register_t)onregister, NULL, NULL, NULL);
	return mod;
}

static const char * getregiontypelabel(enum ap_event_teleport_region_type type)
{
	switch (type) {
	case AP_EVENT_TELEPORT_REGION_NORMAL:
		return "Normal";
	case AP_EVENT_TELEPORT_REGION_PVP:
		return "PvP";
	default:
		return "Invalid";
	}
}

static const char * getspecialtypelabel(enum ap_event_teleport_special_type type)
{
	switch (type) {
	case AP_EVENT_TELEPORT_SPECIAL_NORMAL:
		return "Normal";
	case AP_EVENT_TELEPORT_SPECIAL_RETURN_ONLY:
		return "Return Only";
	case AP_EVENT_TELEPORT_SPECIAL_SIEGEWAR:
		return "Siegewar";
	case AP_EVENT_TELEPORT_SPECIAL_CASTLE_TO_DUNGEON:
		return "Castle To Dungeon";
	case AP_EVENT_TELEPORT_SPECIAL_DUNGEON_TO_LANSPHERE:
		return "Dungeon To Lansphere";
	case AP_EVENT_TELEPORT_SPECIAL_LANSPHERE:
		return "Lansphere";
	default:
		return "Invalid";
	}
}

static const char * getusetypelabel(enum ap_event_teleport_use_type type)
{
	switch (type) {
	case AP_EVENT_TELEPORT_USE_HUMAN:
		return "Human";
	case AP_EVENT_TELEPORT_USE_ORC:
		return "Orc";
	case AP_EVENT_TELEPORT_USE_MOONELF:
		return "Moonelf";
	case AP_EVENT_TELEPORT_USE_DRAGONSCION:
		return "Dragonscion";
	default:
		return "Invalid";
	}
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
		bool check;
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
		if (ImGui::BeginCombo("Region Type", getregiontypelabel(point->region_type))) {
			const enum ap_event_teleport_region_type types[] = { 
				AP_EVENT_TELEPORT_REGION_NORMAL,
				AP_EVENT_TELEPORT_REGION_PVP };
			for (i = 0; i < COUNT_OF(types); i++) {
				if (ImGui::Selectable(getregiontypelabel(types[i]), point->region_type == types[i])) {
					point->region_type = types[i];
					pointchanged = true;
				}
			}
			ImGui::EndCombo();
		}
		if (ImGui::BeginCombo("Special Type", getspecialtypelabel(point->special_type))) {
			const enum ap_event_teleport_special_type types[] = { 
				AP_EVENT_TELEPORT_SPECIAL_NORMAL,
				AP_EVENT_TELEPORT_SPECIAL_RETURN_ONLY,
				AP_EVENT_TELEPORT_SPECIAL_SIEGEWAR,
				AP_EVENT_TELEPORT_SPECIAL_CASTLE_TO_DUNGEON,
				AP_EVENT_TELEPORT_SPECIAL_DUNGEON_TO_LANSPHERE,
				AP_EVENT_TELEPORT_SPECIAL_LANSPHERE };
			for (i = 0; i < COUNT_OF(types); i++) {
				if (ImGui::Selectable(getspecialtypelabel(types[i]), point->special_type == types[i])) {
					point->special_type = types[i];
					pointchanged = true;
				}
			}
			ImGui::EndCombo();
		}
		{
			uint32_t types[] = { 
				AP_EVENT_TELEPORT_USE_HUMAN,
				AP_EVENT_TELEPORT_USE_ORC,
				AP_EVENT_TELEPORT_USE_MOONELF,
				AP_EVENT_TELEPORT_USE_DRAGONSCION };
			const char * labels[] = {
				"Can Be Used By Humans",
				"Can Be Used By Orcs",
				"Can Be Used By Moonelves",
				"Can Be Used By Dragonscions" };
			for (i = 0; i < COUNT_OF(types); i++) {
				check = (point->use_type & types[i]) != 0 || !point->use_type;
				if (ImGui::Checkbox(labels[i], &check)) {
					if (check) {
						point->use_type |= types[i];
					}
					else if (!point->use_type) {
						uint32_t j;
						for (j = 0; j < COUNT_OF(types); j++) {
							if (i != j)
								point->use_type |= types[j];
						}
					}
					else {
						point->use_type &= ~types[i];
					}
					pointchanged = true;
				}
			}
		}
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
	return changed;
}
