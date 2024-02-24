/** \file file_system.h
 * File system abstraction.
 * Offers a variety of portable, file system related utilities.
 */
#ifndef _CORE_FILE_SYSTEM_H_
#define _CORE_FILE_SYSTEM_H_

#include <stdarg.h>
#include "core/macros.h"
#include "core/types.h"

BEGIN_DECLS

typedef void * file;
typedef boolean(*enum_dir_callback_t)(
	char * current_dir,
	size_t maxcount,
	const char * name,
	size_t size,
	void * user_data);

enum file_access_type {
	FILE_ACCESS_READ,
	FILE_ACCESS_WRITE,
	FILE_ACCESS_APPEND,
};

/**
 * Creates a platform-dependent file path.
 *
 * Returns the number of characters that would have been 
 * written if `maxcount` had been sufficiently large, 
 * not counting the terminating null character.
 *
 * If an encoding error occurs, a negative number is returned.
 *
 * Notice that only when this returned value is non-negative 
 * and less than n, the string has been completely written.
 */
int make_path(
	char * dst,
	size_t maxcount,
	const char * fmt,
	...);

int make_pathv(
	char * dst,
	size_t maxcount,
	const char * fmt,
	va_list ap);

size_t get_path_length(const char * fmt, ...);

size_t get_path_lengthv(const char * fmt, va_list ap);

/**
 * Strips file name from path.
 *
 * Both file directory and file 
 * extension is stripped.
 *
 * `path` and `dst` must be different strings.
 *
 * Returns the length of stripped file name (not the 
 * number of bytes that were written to `dst`, which 
 * may be smaller due to insufficient buffer size).
 */
size_t strip_file_name(
	const char * path,
	char * dst,
	size_t maxcount);

boolean get_file_size(const char * file_path, size_t * size);

file open_file(
	const char * file_path,
	enum file_access_type access_type);

void close_file(file file);

boolean seek_file(file file, size_t offset, boolean from_start);

boolean read_file(file file, void * buffer, size_t count);

boolean read_line(file file, char * dst, size_t maxcount);

const char * read_line_buffer(
	const char * buf, 
	char * dst, 
	size_t maxcount);

boolean write_line(file file, const char * str);

char * write_line_buffer(
	char * dst, 
	size_t maxcount, 
	const char * str);

char * write_line_bufferv(
	char * dst, 
	size_t maxcount, 
	const char * fmt, ...);

boolean load_file(
	const char * file_path,
	void * data,
	size_t size);

boolean write_file(file file, const void * buffer, size_t count);

boolean print_file(file  file, const char * fmt, ...);

boolean make_file(
	const char * path,
	const void * buffer,
	size_t count);

boolean copy_file(
	const char * src_path, 
	const char * dst_path, 
	boolean fail_if_exists);

boolean enum_dir(
	char * directory_path,
	size_t maxcount,
	boolean recursive,
	enum_dir_callback_t cb,
	void * user_data);

boolean replace_file(
	const char * replaced, 
	const char * replacement);

boolean remove_file(const char * path);

END_DECLS

#endif /* _CORE_FILE_SYSTEM_H_ */