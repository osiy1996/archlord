#ifndef _AS_CHARACTER_PROCESS_H_
#define _AS_CHARACTER_PROCESS_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module.h"

#define AS_CHARACTER_PROCESS_MODULE_NAME "AgsmCharacterProcess"

BEGIN_DECLS

struct as_character_process_module * as_character_process_create_module();

void as_character_process_iterate_all(
	struct as_character_process_module * mod, 
	float dt);

END_DECLS

#endif /* _AS_CHARACTER_PROCESS_H_ */
