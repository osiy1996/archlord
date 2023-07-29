#include "core/file_system.h"
#include "core/string.h"
#include "core/malloc.h"
#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <Windows.h>

int make_path(
	char * dst,
	size_t maxcount,
	const char * fmt,
	...)
{
	va_list ap;
	int r;
	va_start(ap, fmt);
	r = make_pathv(dst, maxcount, fmt, ap);
	va_end(ap);
	return r;
}

int make_pathv(
	char * dst,
	size_t maxcount,
	const char * fmt,
	va_list ap)
{
	int i;
	int r = vsnprintf(dst, maxcount, fmt, ap);
	int w = r;
	if (w >= (int)maxcount)
		w = (int)(maxcount - 1);
	for (i = 0; i < w; i++) {
		if (dst[i] == '/')
			dst[i] = '\\';
	}
	return r;
}

size_t get_path_length(const char * fmt, ...)
{
	va_list ap;
	size_t r;
	va_start(ap, fmt);
	r = get_path_lengthv(fmt, ap);
	va_end(ap);
	return r;
}

size_t get_path_lengthv(const char * fmt, va_list ap)
{
	return vsnprintf(NULL, 0, fmt, ap);
}

size_t strip_file_name(
	const char * path,
	char * dst,
	size_t maxcount)
{
	size_t len = strlen(path);
	const char * from = path;
	const char * s = path + (len - 1);
	const char * ext = NULL;
	size_t r;
	if (!len || path == dst)
		return 0;
	while (s >= path) {
		char c = *s--;
		if (!c)
			break;
		if (!ext && c == '.')
			ext = s + 1;
		if (c == '\\' || c == '/') {
			from = s + 2;
			break;
		}
	}
	if (ext) {
		r = (size_t)(ext - from);
		maxcount = MIN(maxcount, r + 1);
	}
	else {
		r = strlen(from);
	}
	strlcpy(dst, from, maxcount);
	return r;
}

boolean get_file_size(const char * file_path, size_t * size)
{
	LARGE_INTEGER n;
	HANDLE file = CreateFileA(file_path, GENERIC_READ, 
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return FALSE;
	if (!GetFileSizeEx(file, &n)) {
		CloseHandle(file);
		return FALSE;
	}
	CloseHandle(file);
	*size = (size_t)(n.QuadPart);
	return TRUE;
}

file open_file(
	const char * file_path,
	enum file_access_type access_type)
{
	const char * m;
	switch (access_type) {
	case FILE_ACCESS_READ:
		m = "rb";
		break;
	case FILE_ACCESS_WRITE:
		m = "wb";
		break;
	case FILE_ACCESS_APPEND:
		m = "ab";
		break;
	default:
		return NULL;
	}
	return fopen(file_path, m);
}

void close_file(file file)
{
	if (file)
		fclose((FILE *)file);
}

boolean seek_file(file file, size_t offset, boolean from_start)
{
	if (offset > MAXLONG)
		return 0;
	return (fseek((FILE *)file, (long)offset,
		from_start ? SEEK_SET : SEEK_CUR) == 0);
}

boolean read_file(file file, void * buffer, size_t count)
{
	return (fread(buffer, count, 1, (FILE *)file) != 0);
}

boolean read_line(file file, char * dst, size_t maxcount)
{
	FILE * f = file;
	char c = '\0';
	if (!maxcount)
		return FALSE;
	while (maxcount > 1) {
		c = fgetc(f);
		if (c == '\r') {
			c = fgetc(f);
			if (c == '\n') {
				*dst = '\0';
				return TRUE;
			}
			if (maxcount < 3)
				return FALSE;
			*dst++ = '\r';
			*dst++ = c;
			maxcount -= 2;
		}
		else if (c == '\n') {
			*dst = '\0';
			return TRUE;
		}
		else if (c == EOF || c == '\0') {
			break;
		}
		else {
			*dst++ = c;
			maxcount--;
		}
	}
	*dst = '\0';
	return (c != EOF);
}

const char * read_line_buffer(
	const char * buf, 
	char * dst, 
	size_t maxcount)
{
	char c = '\0';
	if (!maxcount)
		return NULL;
	if (!buf[0])
		return NULL;
	while (maxcount > 1) {
		c = *buf++;
		if (c == '\r') {
			c = *buf++;
			if (c == '\n') {
				*dst = '\0';
				return buf;
			}
			if (maxcount < 3)
				return NULL;
			*dst++ = '\r';
			*dst++ = c;
			maxcount -= 2;
		}
		else if (c == '\n') {
			*dst = '\0';
			return buf;
		}
		else if (c == '\0') {
			break;
		}
		else {
			*dst++ = c;
			maxcount--;
		}
	}
	*dst = '\0';
	return buf;
}

boolean write_line(file file, const char * str)
{
	const uint8_t new_line[2] = { '\r', '\n' };
	return (
		fwrite(str, strlen(str), 1, file) == 1 &&
		fwrite(new_line, 2, 1, file) == 1);
}

char * write_line_buffer(
	char * dst, 
	size_t maxcount, 
	const char * str)
{
	if (maxcount < 2)
		return NULL;
	while (TRUE) {
		char c = *str++;
		if (!c)
			break;
		if (maxcount < 3)
			return NULL;
		*dst++ = c;
	}
	*dst++ = '\r';
	*dst++ = '\n';
	return dst;
}

char * write_line_bufferv(
	char * dst, 
	size_t maxcount, 
	const char * fmt, ...)
{
	va_list ap;
	int n;
	char * buf;
	char * r;
	va_start(ap, fmt);
	n = vsnprintf(NULL, 0, fmt, ap);
	if (n < 0) {
		va_end(ap);
		return NULL;
	}
	buf = alloc(++n);
	n = vsnprintf(buf, n, fmt, ap);
	if (n < 0) {
		va_end(ap);
		dealloc(buf);
		return NULL;
	}
	va_end(ap);
	r = write_line_buffer(dst, maxcount, buf);
	dealloc(buf);
	return r;
}

boolean load_file(
	const char * file_path,
	void * data,
	size_t size)
{
	file file;
	boolean r;
	file = open_file(file_path, FILE_ACCESS_READ);
	if (!file)
		return FALSE;
	r = read_file(file, data, size);
	close_file(file);
	return r;
}

boolean write_file(file file, const void * buffer, size_t count)
{
	return (fwrite(buffer, count, 1, file) == 1);
}

boolean print_file(file  file, const char * fmt, ...)
{
	va_list ap;
	int n;
	char * buf;
	va_start(ap, fmt);
	n = vsnprintf(NULL, 0, fmt, ap);
	if (n < 0) {
		va_end(ap);
		return FALSE;
	}
	buf = alloc(++n);
	n = vsnprintf(buf, n, fmt, ap);
	if (n < 0) {
		va_end(ap);
		dealloc(buf);
		return FALSE;
	}
	va_end(ap);
	n = fputs(buf, (FILE *)file);
	dealloc(buf);
	return (n != EOF);
}

boolean make_file(
	const char * path,
	const void * buffer,
	size_t count)
{
	file f = open_file(path, FILE_ACCESS_WRITE);
	boolean r;
	if (!f)
		return FALSE;
	r = write_file(f, buffer, count);
	close_file(f);
	return r;
}

boolean enum_dir(
	char * directory_path,
	size_t maxcount,
	boolean recursive,
	enum_dir_callback_t cb,
	void * user_data)
{
	WIN32_FIND_DATA fd;
	LARGE_INTEGER li = { 0 };
	HANDLE file;
	size_t len = strlen(directory_path);
	size_t file_size = 0;
	char dir[MAX_PATH + 1];
	if (len > (MAX_PATH - 3))
		return FALSE;
	make_path(dir, sizeof(dir), "%s\\*", directory_path);
	file = FindFirstFileA(dir, &fd);
	if (file == INVALID_HANDLE_VALUE)
		return FALSE;
	do {
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
			fd.cFileName[0] == '.')
			continue;
		li.LowPart = fd.nFileSizeLow;
		li.HighPart = fd.nFileSizeHigh;
		file_size = (size_t)li.QuadPart;
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			if (!cb(directory_path, maxcount, fd.cFileName, 
					file_size, user_data)) {
				return FALSE;
			}
		}
		else if (recursive) {
			char prev[MAX_PATH + 1];
			int r;
			if (strlcpy(prev, directory_path,
				sizeof(prev)) >= sizeof(prev))
				return FALSE;
			r = make_path(directory_path, maxcount, "%s/%s", prev,
				fd.cFileName);
			if (r < 0 || r >= (int)maxcount)
				return FALSE;
			if (!enum_dir(directory_path, maxcount, TRUE, cb,
				user_data))
				return FALSE;
			if (strlcpy(directory_path, prev,
				maxcount) >= maxcount)
				return FALSE;
		}
	} while (FindNextFileA(file, &fd) != 0);
	FindClose(file);
	return TRUE;
}

boolean replace_file(
	const char * replaced, 
	const char * replacement)
{
	return (ReplaceFileA(replaced, replacement, NULL, 0, 
		NULL, NULL) != 0);
}
