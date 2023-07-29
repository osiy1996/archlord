#ifndef _AS_EVENT_ITEM_CONVERT_PROCESS_H_
#define _AS_EVENT_ITEM_CONVERT_PROCESS_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_character.h"
#include "public/ap_event_manager.h"
#include "public/ap_module.h"

#define AS_EVENT_ITEM_CONVERT_PROCESS_MODULE_NAME "AgsmEventItemConvertProcess"

BEGIN_DECLS

struct as_event_item_convert_process_module * as_event_item_convert_process_create_module();

END_DECLS

#endif /* _AS_EVENT_ITEM_CONVERT_PROCESS_H_ */
