#include "editor/ae_editor_action.h"

#include "core/log.h"

#include "client/ac_imgui.h"

#include <assert.h>
#include <stdlib.h>

struct sorted_callback {
	const char * label;
	ap_module_t callback_module;
	ap_module_default_t callback;
};

struct ae_editor_action_module {
	struct ap_module_instance instance;
	struct sorted_callback callbacks[AP_MODULE_MAX_CALLBACK_ID][AP_MODULE_MAX_CALLBACK_COUNT];
	uint32_t callback_count[AP_MODULE_MAX_CALLBACK_ID];
	bool display_outliner;
	bool display_properties;
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

static int sortlabels(
	const void * a, 
	const void * b)
{
	const struct sorted_callback * c1 = (const struct sorted_callback *)a;
	const struct sorted_callback * c2 = (const struct sorted_callback *)b;
	return strcmp(c1->label, c2->label);
}

void ae_editor_action_add_view_menu_callback(
	struct ae_editor_action_module * mod,
	const char * label,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	uint32_t id = AE_EDITOR_ACTION_CB_RENDER_VIEW_MENU;
	uint32_t index;
	assert(mod->callback_count[id] < AP_MODULE_MAX_CALLBACK_COUNT);
	index = mod->callback_count[id]++;
	mod->callbacks[id][index].label = label;
	mod->callbacks[id][index].callback_module = callback_module;
	mod->callbacks[id][index].callback = callback;
	qsort(mod->callbacks[id], (size_t)index + 1, sizeof(struct sorted_callback), 
		sortlabels);
}

void ae_editor_action_render_view_menu(struct ae_editor_action_module * mod)
{
	uint32_t id = AE_EDITOR_ACTION_CB_RENDER_VIEW_MENU;
	uint32_t i;
	for (i = 0; i < mod->callback_count[id]; i++) {
		struct sorted_callback * callback = &mod->callbacks[id][i];
		callback->callback(callback->callback_module, NULL);
	}
}

void ae_editor_action_render_editors(struct ae_editor_action_module * mod)
{
	ap_module_enum_callback(mod, AE_EDITOR_ACTION_CB_RENDER_EDITORS, NULL);
}

void ae_editor_action_pick(
	struct ae_editor_action_module * mod,
	struct ac_camera * cam,
	int x,
	int y)
{
	struct ae_editor_action_cb_pick cb = { 0 };
	cb.camera = cam;
	cb.x = x;
	cb.y = y;
	ap_module_enum_callback(mod, AE_EDITOR_ACTION_CB_PICK, &cb);
}

void ae_editor_action_render_outliner(struct ae_editor_action_module * mod)
{
	if (ImGui::Begin("Outliner", &mod->display_outliner)) {
		struct ae_editor_action_cb_render_outliner cb = { 0 };
		ap_module_enum_callback(mod, AE_EDITOR_ACTION_CB_RENDER_OUTLINER, &cb);
	}
	ImGui::End();
}

void ae_editor_action_render_properties(struct ae_editor_action_module * mod)
{
	if (ImGui::Begin("Properties", &mod->display_properties))
		ap_module_enum_callback(mod, AE_EDITOR_ACTION_CB_RENDER_PROPERTIES, NULL);
	ImGui::End();
}
