#ifndef _AE_TRANSFORM_TOOL_H_
#define _AE_TRANSFORM_TOOL_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module_instance.h"

#define AE_TRANSFORM_TOOL_MODULE_NAME "AgemTransformTool"

BEGIN_DECLS

typedef void (*ae_transform_tool_translate_callback_t)(
	ap_module_t mod, 
	const struct au_pos * destination);

struct ae_transform_tool_module * ae_transform_tool_create_module();

void ae_transform_tool_set_target(
	struct ae_transform_tool_module * mod,
	ap_module_t callback_module,
	ae_transform_tool_translate_callback_t translate_cb,
	void * target_base,
	const struct au_pos * position,
	float target_height);

void ae_transform_tool_switch_translate(struct ae_transform_tool_module * mod);

void ae_transform_tool_cancel_target(struct ae_transform_tool_module * mod);

END_DECLS

#endif /* _AE_TRANSFORM_TOOL_H_ */

