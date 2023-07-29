#ifndef _AP_TIMER_H_
#define _AP_TIMER_H_

#include "core/macros.h"
#include "core/types.h"

BEGIN_DECLS

struct ap_timer_module * ap_timer_create_module();

void ap_timer_close_module();

void ap_timer_shutdown();

END_DECLS

#endif /* _AP_TIMER_H_ */
