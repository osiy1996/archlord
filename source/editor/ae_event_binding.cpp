#include "editor/ae_event_binding.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_config.h"
#include "public/ap_event_binding.h"
#include "public/ap_map.h"

#include "client/ac_imgui.h"

#include "editor/ae_editor_action.h"

#include <assert.h>

struct ae_event_binding_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_event_binding_module * ap_event_binding;
	struct ap_map_module * ap_map;
	struct ae_editor_action_module * ae_editor_action;
	boolean has_pending_changes;
	bool display_group_editor;
	char group_search_input[AP_MAP_MAX_REGION_NAME_SIZE];
	bool filter_empty_groups;
	bool add_point;
	struct ap_event_binding_group * add_point_to_group;
	boolean add_point_to_source;
	char point_search_input[AP_EVENT_BINDING_MAX_NAME + 1];
};

static boolean cbcommitchanges(
	struct ae_event_binding_module * mod,
	void * data)
{
	if (!mod->has_pending_changes)
		return TRUE;
	return TRUE;
}

static boolean cbrenderviewmenu(struct ae_event_binding_module * mod, void * data)
{
	return TRUE;
}

static boolean cbrendereditors(struct ae_event_binding_module * mod, void * data)
{
	return TRUE;
}

static boolean onregister(
	struct ae_event_binding_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_binding, AP_EVENT_BINDING_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_map, AP_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ae_editor_action, AE_EDITOR_ACTION_MODULE_NAME);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_COMMIT_CHANGES, mod, (ap_module_default_t)cbcommitchanges);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_RENDER_EDITORS, mod, (ap_module_default_t)cbrendereditors);
	return TRUE;
}

struct ae_event_binding_module * ae_event_binding_create_module()
{
	struct ae_event_binding_module * mod = (struct ae_event_binding_module *)ap_module_instance_new(AE_EVENT_BINDING_MODULE_NAME, 
		sizeof(*mod), (ap_module_instance_register_t)onregister, NULL, NULL, NULL);
	return mod;
}

boolean ae_event_binding_render_as_node(
	struct ae_event_binding_module * mod,
	void * source,
	struct ap_event_manager_attachment * attachment)
{
	static bool checkbox;
	uint32_t i;
	uint32_t index;
	struct ap_event_manager_event * e = NULL;
	struct ap_event_binding_event * eventbinding;
	struct ap_event_binding * binding;
	struct ap_map_region_template * region;
	bool changed;
	bool nodeopen;
	char label[256];
	for (i = 0 ; i < attachment->event_count; i++) {
		if (attachment->events[i].function == AP_EVENT_MANAGER_FUNCTION_BINDING) {
			index = i;
			e = &attachment->events[i];
			break;
		}
	}
	checkbox = e != NULL;
	changed = ImGui::Checkbox("##EventBindingEnabled", &checkbox);
	ImGui::SameLine();
	nodeopen = ImGui::TreeNodeEx("Event Function (Binding)", ImGuiTreeNodeFlags_Framed);
	if (!e) {
		if (nodeopen)
			ImGui::TreePop();
		if (changed && checkbox) {
			e = ap_event_manager_add_function(mod->ap_event_manager, attachment,
				AP_EVENT_MANAGER_FUNCTION_BINDING, source);
			if (!e)
				return FALSE;
			return TRUE;
		}
		return FALSE;
	}
	eventbinding = (struct ap_event_binding_event *)e->data;
	binding = &eventbinding->binding;
	if (changed && !checkbox) {
		if (nodeopen)
			ImGui::TreePop();
		ap_event_manager_remove_function(mod->ap_event_manager, attachment, index);
		return TRUE;
	}
	if (!nodeopen)
		return FALSE;
	changed = false;
	region = ap_map_get_region_template_by_name(mod->ap_map, binding->town_name);
	if (region) {
		if (!strisempty(region->glossary))
			strlcpy(label, region->glossary, sizeof(label));
		else
			snprintf(label, sizeof(label), "Region Id %u", region->id);
	}
	else {
		strcpy(label, "Select Binding Region");
	}
	if (ImGui::Button(label, ImVec2(ImGui::GetContentRegionAvail().x, 25.0f))) {
	}
	ImGui::TreePop();
	return changed;
}
