#ifndef _AE_EDITOR_ACTION_H_
#define _AE_EDITOR_ACTION_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module_instance.h"

#include "vendor/SDL2/SDL.h"

#define AE_EDITOR_ACTION_MODULE_NAME "AgemEditorAction"

BEGIN_DECLS

enum ae_editor_action_callback_id {
	AE_EDITOR_ACTION_CB_COMMIT_CHANGES,
	AE_EDITOR_ACTION_CB_RENDER_VIEW_MENU,
	AE_EDITOR_ACTION_CB_RENDER_EDITORS,
	AE_EDITOR_ACTION_CB_PICK,
	AE_EDITOR_ACTION_CB_RENDER_OUTLINER,
	AE_EDITOR_ACTION_CB_RENDER_PROPERTIES,
	AE_EDITOR_ACTION_CB_HANDLE_INPUT,
	AE_EDITOR_ACTION_CB_RENDER_ADD_MENU,
};

struct ae_editor_action_cb_pick {
	boolean picked_any;
	float distance;
	struct ac_camera * camera;
	int x;
	int y;
};

struct ae_editor_action_cb_render_outliner {
	boolean selected_new_entity;
};

struct ae_editor_action_cb_handle_input {
	const SDL_Event * input;
};

struct ae_editor_action_module * ae_editor_action_create_module();

void ae_editor_action_add_callback(
	struct ae_editor_action_module * mod,
	enum ae_editor_action_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

void ae_editor_action_commit_changes(struct ae_editor_action_module * mod);

void ae_editor_action_add_view_menu_callback(
	struct ae_editor_action_module * mod,
	const char * label,
	ap_module_t callback_module,
	ap_module_default_t callback);

void ae_editor_action_render_view_menu(struct ae_editor_action_module * mod);

void ae_editor_action_render_editors(struct ae_editor_action_module * mod);

void ae_editor_action_pick(
	struct ae_editor_action_module * mod,
	struct ac_camera * cam,
	int x,
	int y);

void ae_editor_action_render_outliner(struct ae_editor_action_module * mod);

void ae_editor_action_render_properties(struct ae_editor_action_module * mod);

void ae_editor_action_add_input_handler(
	struct ae_editor_action_module * mod,
	ap_module_t callback_module,
	ap_module_default_t callback);

void ae_editor_action_handle_input(
	struct ae_editor_action_module * mod,
	const SDL_Event * input);

void ae_editor_action_render_add_menu(struct ae_editor_action_module * mod);

END_DECLS

#endif /* _AE_EDITOR_ACTION_H_ */

