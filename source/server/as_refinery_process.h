#ifndef _AS_DROP_REFINERY_PROCESS_H_
#define _AS_DROP_REFINERY_PROCESS_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module.h"

#define AS_REFINERY_PROCESS_MODULE_NAME "AgsmRefineryProcess"

BEGIN_DECLS

struct as_refinery_process_module * as_refinery_process_create_module();

END_DECLS

#endif /* _AS_DROP_REFINERY_PROCESS_H_ */
