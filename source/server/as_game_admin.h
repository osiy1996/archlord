#ifndef _AS_GAME_ADMIN_H_
#define _AS_GAME_ADMIN_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module.h"

#define AS_GAME_ADMIN_MODULE_NAME "AgsmGameAdmin"

enum as_game_admin_callback_id {
	AS_GAME_ADMIN_CB_CONNECT,
	AS_GAME_ADMIN_CB_DISCONNECT,
	AS_GAME_ADMIN_CB_RECEIVE,
};

BEGIN_DECLS

struct as_game_admin_module * as_game_admin_create_module();

END_DECLS

#endif /* _AS_GAME_ADMIN_H_ */
