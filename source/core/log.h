#ifndef _LOG_LOG_H_
#define _LOG_LOG_H_

#include "core/macros.h"
#include "core/types.h"

BEGIN_DECLS

#define TRACE(fmt, ...) log_msg(LOG_LEVEL_TRACE,\
	__FILE__, __LINE__, (fmt), __VA_ARGS__)
#define INFO(fmt, ...) log_msg(LOG_LEVEL_INFO,\
	__FILE__, __LINE__, (fmt), __VA_ARGS__)
#define WARN(fmt, ...) log_msg(LOG_LEVEL_WARN,\
	__FILE__, __LINE__, (fmt), __VA_ARGS__)
#define ERROR(fmt, ...) log_msg(LOG_LEVEL_ERROR,\
	__FILE__, __LINE__, (fmt), __VA_ARGS__)

enum LogLevel {
	LOG_LEVEL_TRACE,
	LOG_LEVEL_INFO,
	LOG_LEVEL_WARN,
	LOG_LEVEL_ERROR,
};

typedef void (*log_callback_t)(
	enum LogLevel level,
	const char * file,
	uint32_t line,
	const char * message);

boolean log_init();

void log_msg(
	enum LogLevel level,
	const char * file, 
	uint32_t line,
	const char * fmt,
	...);

void log_set_callback(log_callback_t cb);

END_DECLS

#endif /* _LOG_LOG_H_ */