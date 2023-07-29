#ifndef _AC_CONSOLE_H_
#define _AC_CONSOLE_H_

#include "core/macros.h"
#include "core/types.h"

#include <stdarg.h>

#define AC_CONSOLE_MODULE_NAME "AgcmConsole"

BEGIN_DECLS

struct ac_console_module * ac_console_create_module();

void ac_console_println(struct ac_console_module * mod, const char * fmt, ...);

void ac_console_vprintln(
	struct ac_console_module * mod, 
	const char * fmt, 
	va_list ap);

void ac_console_println_colored(
	struct ac_console_module * mod,
	const char * fmt,
	const float color[4], ...);

void ac_console_render(struct ac_console_module * mod);

void ac_console_render_icon(struct ac_console_module * mod);

END_DECLS

#endif /* _AC_CONSOLE_H_ */
