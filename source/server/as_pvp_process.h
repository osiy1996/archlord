#ifndef _AS_PVP_PROCESS_H_
#define _AS_PVP_PROCESS_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module.h"

#define AS_PVP_PROCESS_MODULE_NAME "AgsmPvPProcess"

BEGIN_DECLS

struct as_pvp_process_module * as_pvp_process_create_module();

END_DECLS

#endif /* _AS_PVP_PROCESS_H_ */
