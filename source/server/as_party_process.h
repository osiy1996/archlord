#ifndef _AS_PARTY_PROCESS_H_
#define _AS_PARTY_PROCESS_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module.h"

#define AS_PARTY_PROCESS_MODULE_NAME "AgsmPartyProcess"

BEGIN_DECLS

struct as_party_process_module * as_party_process_create_module();

END_DECLS

#endif /* _AS_PARTY_PROCESS_H_ */
