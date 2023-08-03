#include "editor/ae_map.h"

#include "core/log.h"

#include "public/ap_config.h"
#include "public/ap_map.h"

#include "editor/ae_editor_action.h"

#include <assert.h>

struct ae_map_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	struct ap_map_module * ap_map;
	struct ae_editor_action_module * ae_editor_action;
	boolean has_pending_changes;
};

static boolean cbaddglossary(struct ae_map_module * mod, void * data)
{
	mod->has_pending_changes = TRUE;
	return TRUE;
}

static boolean cbcommitchanges(struct ae_map_module * mod, void * data)
{
	char paths[2][1024];
	boolean encrypt[2] = { FALSE, TRUE };
	uint32_t i;
	if (!mod->has_pending_changes)
		return TRUE;
	snprintf(paths[0], sizeof(paths[0]), "%s/regionglossary.txt", 
		ap_config_get(mod->ap_config, "ServerIniDir"));
	snprintf(paths[1], sizeof(paths[1]), "%s/ini/regionglossary.txt",
		ap_config_get(mod->ap_config, "ClientDir"));
	for (i = 0; i < 2; i++) {
		if (ap_map_write_region_glossary(mod->ap_map,
				paths[i], encrypt[i])) {
			INFO("Saved region glossary to: %s.", paths[i]);
		}
		else {
			ERROR("Failed to save region glossary to: %s.", paths[i]);
		}
	}
	mod->has_pending_changes = FALSE;
	return FALSE;
}

static boolean onregister(
	struct ae_map_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_map, AP_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ae_editor_action, AE_EDITOR_ACTION_MODULE_NAME);
	ap_map_add_callback(mod->ap_map, AP_MAP_CB_ADD_GLOSSARY, mod, cbaddglossary);
	ae_editor_action_add_callback(mod->ae_editor_action, 
		AE_EDITOR_ACTION_CB_COMMIT_CHANGES, mod, cbcommitchanges);
	return TRUE;
}

struct ae_map_module * ae_map_create_module()
{
	struct ae_map_module * mod = (struct ae_map_module *)ap_module_instance_new(AE_MAP_MODULE_NAME, 
		sizeof(*mod), (ap_module_instance_register_t)onregister, NULL, NULL, NULL);
	return mod;
}
