#include "editor/ae_editor_action.h"

#include "core/log.h"

#include <assert.h>

struct ae_editor_action_module {
	struct ap_module_instance instance;
};

static boolean onregister(
	struct ae_editor_action_module * mod,
	struct ap_module_registry * registry)
{
	return TRUE;
}

struct ae_editor_action_module * ae_editor_action_create_module()
{
	struct ae_editor_action_module * mod = (struct ae_editor_action_module *)ap_module_instance_new(AE_EDITOR_ACTION_MODULE_NAME, 
		sizeof(*mod), (ap_module_instance_register_t)onregister, NULL, NULL, NULL);
	return mod;
}

void ae_editor_action_add_callback(
	struct ae_editor_action_module * mod,
	enum ae_editor_action_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

void ae_editor_action_commit_changes(struct ae_editor_action_module * mod)
{
	ap_module_enum_callback(mod, AE_EDITOR_ACTION_CB_COMMIT_CHANGES, NULL);
}
