#ifndef _AS_EVENT_GUILD_H_
#define _AS_EVENT_GUILD_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_event_manager.h"
#include "public/ap_module.h"

#define AS_EVENT_GUILD_MODULE_NAME "AgsmEventGuild"

BEGIN_DECLS

struct as_event_guild_module * as_event_guild_create_module();

END_DECLS

#endif /* _AS_EVENT_GUILD_H_ */
