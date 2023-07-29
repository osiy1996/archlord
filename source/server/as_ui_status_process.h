#ifndef _AS_UI_STATUS_PROCESS_H_
#define _AS_UI_STATUS_PROCESS_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module.h"

#define AS_UI_STATUS_PROCESS_MODULE_NAME "AgsmUIStatusProcess"

BEGIN_DECLS

struct as_ui_status_process_module * as_ui_status_process_create_module();

END_DECLS

#endif /* _AS_UI_STATUS_PROCESS_H_ */
