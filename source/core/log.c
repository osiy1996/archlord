#include "core/log.h"
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static log_callback_t g_Callback;

static size_t fmt_time(char * str, size_t maxlen, time_t t)
{
	const struct tm * tm = localtime(&t);
	return snprintf(str, maxlen, 
		"%04d-%02d-%02dT%02d:%02d:%02d", 
		tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, 
		tm->tm_hour, tm->tm_min, tm->tm_sec);
}

static const char * level_label(enum LogLevel level)
{
	switch(level){
	case LOG_LEVEL_TRACE:
		return "TRACE";
	case LOG_LEVEL_INFO:
		return "INFO";
	case LOG_LEVEL_WARN:
		return "WARN";
	case LOG_LEVEL_ERROR:
		return "ERROR";
	default:
		return "UNDEFINED";
	}
}

boolean log_init()
{
	return TRUE;
}

void log_msg(
	enum LogLevel level,
	const char * file, 
	uint32_t line,
	const char * fmt,
	...)
{
	char msg[1024];
	char timestr[128];
	va_list ap;
	FILE * out;
	va_start(ap, fmt);
	vsnprintf(msg, 1024, fmt, ap);
	va_end(ap);
	fmt_time(timestr, sizeof(timestr), time(NULL));
	switch (level) {
	default:
	case LOG_LEVEL_TRACE:
	case LOG_LEVEL_INFO:
		out = stdout;
		break;
	case LOG_LEVEL_WARN:
	case LOG_LEVEL_ERROR:
		out = stderr;
		break;
	}
	fprintf(out, "[%s| %-6s] %s\n",
		timestr, level_label(level), msg);
	if (g_Callback)
		g_Callback(level, file, line, msg);
}

void log_set_callback(log_callback_t cb)
{
	g_Callback = cb;
}
