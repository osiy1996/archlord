#ifndef _AE_OBJECT_H_
#define _AE_OBJECT_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module_instance.h"

#include "vendor/bgfx/c99/bgfx.h"

#define AE_OBJECT_MODULE_NAME "AgemObject"

BEGIN_DECLS

extern size_t g_AeObjectTemplateOffset;

struct ae_object_template {
	int dummy;
};

struct ac_camera;

struct ae_object_module * ae_object_create_module();

void ae_object_update(struct ae_object_module * mod, float dt);

void ae_object_render_outline(struct ae_object_module * mod, struct ac_camera * cam);

void ae_object_render(struct ae_object_module * mod, struct ac_camera * cam);

void ae_object_imgui(struct ae_object_module * mod);

static inline struct ae_object_template * ae_object_get_template(
	struct ap_object_template * temp)
{
	return (struct ae_object_template *)ap_module_get_attached_data(temp, 
		g_AeObjectTemplateOffset);
}

END_DECLS

#endif /* _AE_OBJECT_H_ */

