#ifndef _CORE_H_
#define _CORE_H_

#include "core/macros.h"
#include "core/types.h"

BEGIN_DECLS

boolean core_startup();

boolean core_should_shutdown();

void core_signal_shutdown();

END_DECLS

#endif /* _CORE_H_ */
