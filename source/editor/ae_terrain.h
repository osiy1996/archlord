#ifndef _AE_TERRAIN_H_
#define _AE_TERRAIN_H_

#include "core/macros.h"
#include "core/types.h"

#define AE_TERRAIN_MODULE_NAME "AgemTerrain"

BEGIN_DECLS

struct ac_camera;

struct ae_terrain_module * ae_terrain_create_module();

void ae_terrain_update(struct ae_terrain_module * mod, float dt);

void ae_terrain_render(struct ae_terrain_module * mod, struct ac_camera * cam);

void ae_terrain_toolbar(struct ae_terrain_module * mod);

void ae_terrain_toolbox(struct ae_terrain_module * mod);

void ae_terrain_imgui(struct ae_terrain_module * mod);

void ae_terrain_on_mdown(
	struct ae_terrain_module * mod,
	struct ac_camera*  cam,
	int mouse_x,
	int mouse_y);

boolean ae_terrain_on_mmove(
	struct ae_terrain_module * mod,
	struct ac_camera*  cam,
	int mouse_x,
	int mouse_y);

boolean ae_terrain_on_mwheel(struct ae_terrain_module * mod, float delta);

boolean ae_terrain_on_key_down(struct ae_terrain_module * mod, uint32_t keycode);

END_DECLS

#endif /* _AE_TERRAIN_H_ */
