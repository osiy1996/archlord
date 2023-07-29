#ifndef _AE_OBJECT_H_
#define _AE_OBJECT_H_

#include "core/macros.h"
#include "core/types.h"

#define AE_OBJECT_MODULE_NAME "AgemObject"

BEGIN_DECLS

struct ac_camera;

struct ae_object_module * ae_object_create_module();

void ae_object_update(struct ae_object_module * mod, float dt);

void ae_object_render_outline(struct ae_object_module * mod, struct ac_camera * cam);

void ae_object_render(struct ae_object_module * mod, struct ac_camera * cam);

void ae_object_imgui(struct ae_object_module * mod);

boolean ae_object_on_mdown(
	struct ae_object_module * mod,
	struct ac_camera * cam,
	int mouse_x,
	int mouse_y);

boolean ae_object_on_mmove(
	struct ae_object_module * mod,
	struct ac_camera * cam,
	int mouse_x,
	int mouse_y,
	int dx,
	int dy);

boolean ae_object_on_mwheel(struct ae_object_module * mod, float delta);

boolean ae_object_on_key_down(
	struct ae_object_module * mod,
	struct ac_camera * cam,
	uint32_t keycode);

END_DECLS

#endif /* _AE_OBJECT_H_ */

