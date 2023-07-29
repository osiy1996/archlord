#ifndef _AS_CHAT_PROCESS_H_
#define _AS_CHAT_PROCESS_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module.h"

#define AS_CHAT_PROCESS_MODULE_NAME "AgsmChatProcess"

BEGIN_DECLS

struct as_chat_process_module * as_chat_process_create_module();

END_DECLS

#endif /* _AS_CHAT_PROCESS_H_ */
