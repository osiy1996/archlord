#ifndef _AP_DUNGEON_WND_H_
#define _AP_DUNGEON_WND_H_

#include "public/ap_module_instance.h"

#define AP_DUNGEON_WND_MODULE_NAME "DungeonWnd"

BEGIN_DECLS

struct ap_dungeon_wnd_module;

struct ap_dungeon_wnd_object_data {
	boolean is_inside_dungeon;
};

struct ap_dungeon_wnd_module * ap_dungeon_wnd_create_module();

END_DECLS

#endif /* _AP_DUNGEON_WND_H_ */
