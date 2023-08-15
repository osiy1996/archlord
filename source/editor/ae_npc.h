#ifndef _AE_NPC_H_
#define _AE_NPC_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module_instance.h"

#include "vendor/bgfx/c99/bgfx.h"

#define AE_NPC_MODULE_NAME "AgemNPC"

BEGIN_DECLS

struct ac_camera;

struct ae_npc_module * ae_npc_create_module();

void ae_npc_render(struct ae_npc_module * mod);

END_DECLS

#endif /* _AE_NPC_H_ */

