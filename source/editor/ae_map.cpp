#include "editor/ae_map.h"

#include "core/log.h"
#include "core/string.h"

#include "utility/au_md5.h"

#include "public/ap_config.h"
#include "public/ap_map.h"

#include "editor/ae_editor_action.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "vendor/imgui/imgui.h"

#include <assert.h>

#define MAX_EDIT_COUNT 128

struct ae_map_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	struct ap_map_module * ap_map;
	struct ae_editor_action_module * ae_editor_action;
	boolean has_pending_glossary_changes;
	boolean has_pending_region_template_changes;
	bool display_region_editor;
	uint32_t templates_in_edit[MAX_EDIT_COUNT];
	uint32_t templates_in_edit_count;
	char template_editor_search_input[256];
};

static boolean cbaddglossary(struct ae_map_module * mod, void * data)
{
	mod->has_pending_glossary_changes = TRUE;
	return TRUE;
}

static boolean cbupdateglossary(struct ae_map_module * mod, void * data)
{
	mod->has_pending_glossary_changes = TRUE;
	return TRUE;
}

static boolean cbcommitchanges(struct ae_map_module * mod, void * data)
{
	char serverpath[1024];
	char clientpath[1024];
	if (mod->has_pending_glossary_changes) {
		snprintf(serverpath, sizeof(serverpath), "%s/regionglossary.txt", 
			ap_config_get(mod->ap_config, "ServerIniDir"));
		if (ap_map_write_region_glossary(mod->ap_map,
				serverpath, FALSE)) {
			INFO("Saved region glossary to: %s.", serverpath);
			snprintf(clientpath, sizeof(clientpath), "%s/ini/regionglossary.txt",
				ap_config_get(mod->ap_config, "ClientDir"));
			if (au_md5_copy_and_encrypt_file(serverpath, clientpath)) {
				INFO("Saved region glossary to: %s.", clientpath);
			}
			else {
				ERROR("Failed to save region glossary to: %s.", clientpath);
			}
			mod->has_pending_glossary_changes = FALSE;
		}
		else {
			ERROR("Failed to save region glossary to: %s.", serverpath);
		}
	}
	if (mod->has_pending_region_template_changes) {
		snprintf(serverpath, sizeof(serverpath), "%s/regiontemplate.ini", 
			ap_config_get(mod->ap_config, "ServerIniDir"));
		if (ap_map_write_region_templates(mod->ap_map, serverpath, FALSE)) {
			INFO("Saved region template to: %s.", serverpath);
			snprintf(clientpath, sizeof(clientpath), "%s/ini/regiontemplate.ini",
				ap_config_get(mod->ap_config, "ClientDir"));
			if (au_md5_copy_and_encrypt_file(serverpath, clientpath)) {
				INFO("Saved region template to: %s.", clientpath);
			}
			else {
				ERROR("Failed to save region template to: %s.", clientpath);
			}
			mod->has_pending_region_template_changes = FALSE;
		}
		else {
			ERROR("Failed to save region template to: %s.", serverpath);
		}
	}
	return FALSE;
}

static boolean cbrenderviewmenu(struct ae_map_module * mod, void * data)
{
	ImGui::Selectable("Region Template Editor", &mod->display_region_editor);
	return TRUE;
}

static boolean isinedit(
	struct ae_map_module * mod, 
	struct ap_map_region_template * temp)
{
	uint32_t i;
	for (i = 0; i < mod->templates_in_edit_count; i++) {
		if (mod->templates_in_edit[i] == temp->id)
			return TRUE;
	}
	return FALSE;
}

static inline uint32_t eraseinedit(
	struct ae_map_module * mod, 
	uint32_t index)
{
	memmove(&mod->templates_in_edit[index], &mod->templates_in_edit[index + 1],
		(size_t)--mod->templates_in_edit_count * sizeof(mod->templates_in_edit[0]));
	return index;
}

static bool rendertypeprop(
	const char * label,
	uint8_t value,
	bool * changed)
{
	bool b = (value != 0);
	bool c = ImGui::Checkbox(label, &b);
	if (c)
		*changed = true;
	return c;
}

static bool renderpeculiarityprop(
	uint32_t * peculiarity, 
	uint32_t bit,
	const char * label)
{
	bool b = (*peculiarity & bit) != 0;
	if (ImGui::Checkbox(label, &b)) {
		if (b) {
			*peculiarity |= bit;
			return true;
		}
		else {
			*peculiarity &= ~bit;
			return true;
		}
	}
	return true;
}

static bool rendertypeprops(
	struct ap_map_region_properties * prop,
	uint32_t * peculiarity)
{
	bool changed = false;
	if (ImGui::TreeNodeEx("Properties", ImGuiTreeNodeFlags_Framed)) {
		changed |= renderpeculiarityprop(peculiarity, AP_MAP_RP_DISABLE_SUMMON, "Disable Summon");
		changed |= renderpeculiarityprop(peculiarity, AP_MAP_RP_DISABLE_GO, "Disable Go");
		changed |= renderpeculiarityprop(peculiarity, AP_MAP_RP_DISABLE_PARTY, "Disable Party");
		changed |= renderpeculiarityprop(peculiarity, AP_MAP_RP_DISABLE_LOGIN, "Disable Login");
		if (rendertypeprop("Is Ridable", prop->is_ridable, &changed))
			prop->is_ridable ^= 1;
		if (rendertypeprop("Allow Pets", prop->allow_pets, &changed))
			prop->allow_pets ^= 1;
		if (rendertypeprop("Allow Roundtrip", prop->allow_round_trip, &changed))
			prop->allow_round_trip ^= 1;
		if (rendertypeprop("Allow Potion", prop->allow_potion, &changed))
			prop->allow_potion ^= 1;
		if (rendertypeprop("Allow Resurrection", prop->allow_resurrection, &changed))
			prop->allow_resurrection ^= 1;
		if (rendertypeprop("Disable Minimap", prop->disable_minimap, &changed))
			prop->disable_minimap ^= 1;
		if (rendertypeprop("Is Jail", prop->is_jail, &changed))
			prop->is_jail ^= 1;
		if (rendertypeprop("Zone Load Area", prop->zone_load_area, &changed))
			prop->zone_load_area ^= 1;
		if (rendertypeprop("Block Characters", prop->block_characters, &changed))
			prop->block_characters ^= 1;
		if (rendertypeprop("Allow Potion Type2", prop->allow_potion_type2, &changed))
			prop->allow_potion_type2 ^= 1;
		if (rendertypeprop("Allow Guild Potion", prop->allow_guild_potion, &changed))
			prop->allow_guild_potion ^= 1;
		if (rendertypeprop("Is Interior", prop->is_interior, &changed))
			prop->is_interior ^= 1;
		if (rendertypeprop("Allow Summon Orb", prop->allow_summon_orb, &changed))
			prop->allow_summon_orb ^= 1;
		if (rendertypeprop("Allow Vitality Potion", prop->allow_vitality_potion, &changed))
			prop->allow_vitality_potion ^= 1;
		if (rendertypeprop("Allow Cure Potion", prop->allow_cure_potion, &changed))
			prop->allow_cure_potion ^= 1;
		if (rendertypeprop("Allow Recovery Potion", prop->allow_recovery_potion, &changed))
			prop->allow_recovery_potion ^= 1;
		ImGui::TreePop();
	}
	return changed;
}

static const char * getfieldtypetag(uint8_t type)
{
	switch (type) {
	case AP_MAP_FT_FIELD:
		return "Field";
	case AP_MAP_FT_TOWN:
		return "Town";
	case AP_MAP_FT_PVP:
		return "PvP";
	default:
		return "Invalid";
	}
}

static const char * getsafetytypetag(uint8_t type)
{
	switch (type) {
	case AP_MAP_ST_SAFE:
		return "Safe Area";
	case AP_MAP_ST_FREE:
		return "Free Battlefield";
	case AP_MAP_ST_DANGER:
		return "Dangerous Battlefield";
	default:
		return "Invalid";
	}
}

static void renderregioneditor(struct ae_map_module * mod)
{
	ImVec2 size = ImVec2(350.0f, 300.0f);
	ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter() - (size / 2.0f);
	uint32_t i;
	ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Region Template Editor", &mod->display_region_editor)) {
		ImGui::End();
		return;
	}
	if (mod->templates_in_edit_count < MAX_EDIT_COUNT && 
		ImGui::Button("New Region Template", 
			ImVec2(ImGui::GetContentRegionAvail().x, 25.0f))) {
		struct ap_map_region_template * temp = ap_map_add_region_template(mod->ap_map);
		if (temp)
			mod->templates_in_edit[mod->templates_in_edit_count++] = temp->id;
	}
	ImGui::InputText("Search", mod->template_editor_search_input, 
		sizeof(mod->template_editor_search_input));
	for (i = 0; i < AP_MAP_MAX_REGION_ID; i++) {
		char label[128];
		struct ap_map_region_template * temp = 
			ap_map_get_region_template(mod->ap_map, i);
		if (!temp)
			continue;
		snprintf(label, sizeof(label), "[%03u] %s", temp->id, temp->glossary);
		if (!strisempty(mod->template_editor_search_input) &&
			!stristr(label, mod->template_editor_search_input)) {
			continue;
		}
		if (ImGui::Selectable(label) && 
			mod->templates_in_edit_count < MAX_EDIT_COUNT &&
			!isinedit(mod, temp)) {
			mod->templates_in_edit[mod->templates_in_edit_count++] = temp->id;
		}
	}
	ImGui::End();
	for (i = 0; i < mod->templates_in_edit_count; i++) {
		char label[128];
		bool open = true;
		bool changed = FALSE;
		struct ap_map_region_template * temp = ap_map_get_region_template(
			mod->ap_map, mod->templates_in_edit[i]);
		if (!temp) {
			i = eraseinedit(mod, i);
			continue;
		}
		size = ImVec2(300.0f, 500.0f);
		center = ImGui::GetMainViewport()->GetWorkCenter() - (size / 2.0f);
		ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing);
		snprintf(label, sizeof(label), "Region Template - [%03u] %s###RegionTemp%u", 
			temp->id, temp->glossary, temp->id);
		if (!ImGui::Begin(label, &open) || !open) {
			if (!open)
				i = eraseinedit(mod, i);
			ImGui::End();
			continue;
		}
		if (ImGui::InputText("Name", temp->glossary, sizeof(temp->glossary))) {
			if (!ap_map_update_glossary(mod->ap_map, temp->name, temp->glossary))
				changed = TRUE;
		}
		changed |= ImGui::InputInt("Parent Region Id", &temp->parent_id);
		if (ImGui::BeginCombo("Field Type", getfieldtypetag(temp->type.props.field_type))) {
			enum ap_map_field_type types[] = {
				AP_MAP_FT_FIELD,
				AP_MAP_FT_TOWN,
				AP_MAP_FT_PVP };
			uint32_t j;
			for (j = 0; j < COUNT_OF(types); j++) {
				if (ImGui::Selectable(getfieldtypetag(types[j]), temp->type.props.field_type == types[j]))
					temp->type.props.field_type = types[j];
			}
			ImGui::EndCombo();
		}
		if (ImGui::BeginCombo("Safety Type", getsafetytypetag(temp->type.props.safety_type))) {
			enum ap_map_safety_type types[] = {
				AP_MAP_ST_SAFE,
				AP_MAP_ST_FREE,
				AP_MAP_ST_DANGER };
			uint32_t j;
			for (j = 0; j < COUNT_OF(types); j++) {
				if (ImGui::Selectable(getsafetytypetag(types[j]), temp->type.props.safety_type == types[j]))
					temp->type.props.safety_type = types[j];
			}
			ImGui::EndCombo();
		}
		changed |= rendertypeprops(&temp->type.props, &temp->peculiarity);
		changed |= ImGui::InputScalar("World Map", ImGuiDataType_S32, &temp->world_map_index);
		changed |= ImGui::InputScalar("Sky Index", ImGuiDataType_U32, &temp->sky_index);
		changed |= ImGui::InputFloat("Visible Distance", &temp->visible_distance);
		changed |= ImGui::InputFloat("Max. Camera Height", &temp->max_camera_height);
		changed |= ImGui::InputScalar("Level Limit", ImGuiDataType_U32, &temp->level_limit);
		changed |= ImGui::InputScalar("Disabled Item Section", ImGuiDataType_U32, &temp->disabled_item_section);
		changed |= ImGui::InputFloat2("Zone Src", temp->zone_src);
		changed |= ImGui::InputFloat2("Zone Dst", temp->zone_dst);
		changed |= ImGui::InputFloat("Zone Height", &temp->zone_height);
		changed |= ImGui::InputFloat("Zone Radius", &temp->zone_radius);
		if (changed)
			mod->has_pending_region_template_changes = TRUE;
		ImGui::End();
	}
}

static boolean cbrendereditors(struct ae_map_module * mod, void * data)
{
	if (mod->display_region_editor)
		renderregioneditor(mod);
	return TRUE;
}

static boolean onregister(
	struct ae_map_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_map, AP_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ae_editor_action, AE_EDITOR_ACTION_MODULE_NAME);
	ap_map_add_callback(mod->ap_map, AP_MAP_CB_ADD_GLOSSARY, mod, (ap_module_default_t)cbaddglossary);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_COMMIT_CHANGES, mod, (ap_module_default_t)cbcommitchanges);
	ae_editor_action_add_view_menu_callback(mod->ae_editor_action, "Region Template Editor", mod, (ap_module_default_t)cbrenderviewmenu);
	ae_editor_action_add_callback(mod->ae_editor_action, AE_EDITOR_ACTION_CB_RENDER_EDITORS, mod, (ap_module_default_t)cbrendereditors);
	return TRUE;
}

struct ae_map_module * ae_map_create_module()
{
	struct ae_map_module * mod = (struct ae_map_module *)ap_module_instance_new(AE_MAP_MODULE_NAME, 
		sizeof(*mod), (ap_module_instance_register_t)onregister, NULL, NULL, NULL);
	return mod;
}
