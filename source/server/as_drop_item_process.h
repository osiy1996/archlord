#ifndef _AS_DROP_DROP_ITEM_PROCESS_H_
#define _AS_DROP_DROP_ITEM_PROCESS_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module.h"

#define AS_DROP_ITEM_PROCESS_MODULE_NAME "AgsmDropItemProcess"

BEGIN_DECLS

struct as_drop_item_process_module * as_drop_item_process_create_module();

boolean as_drop_item_process_create_gold(struct as_drop_item_process_module * mod);

END_DECLS

#endif /* _AS_DROP_DROP_ITEM_PROCESS_H_ */
