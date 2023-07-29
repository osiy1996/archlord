#ifndef _AS_AI2_PROCESS_H_
#define _AS_AI2_PROCESS_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module.h"

#define AS_AI2_PROCESS_MODULE_NAME "AgsmAI2Process"

BEGIN_DECLS

enum as_ai2_process_callback_id {
	AS_AI2_PROCESS_CB_,
};

struct as_ai2_process_module * as_ai2_process_create_module();

void as_ai2_process_add_callback(
	struct as_ai2_process_module * mod,
	enum as_ai2_process_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

void as_ai2_process_end_frame(struct as_ai2_process_module * mod);

END_DECLS

#endif /* _AS_AI2_PROCESS_H_ */
