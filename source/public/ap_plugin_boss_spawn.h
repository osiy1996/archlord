#ifndef _AP_PLG_BSP_H_
#define _AP_PLG_BSP_H_

#include "public/ap_module_instance.h"

#define AP_PLUGIN_BOSS_SPAWN_MODULE_NAME "Plugin_BossSpawn"

BEGIN_DECLS

struct ap_plugin_boss_spawn_module;

struct ap_plugin_boss_spawn_module * ap_plugin_boss_spawn_create_module();

END_DECLS

#endif /* _AP_PLG_BSP_H_ */
