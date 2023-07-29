#include "core/malloc.h"
#include "core/string_conv.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static char * new_str(size_t len)
{
	char * str = alloc(len);
	memset(str, 0, len);
	return str;
}

static wchar_t * new_wstr(size_t len)
{
	wchar_t * str = alloc(len * sizeof(*str));
	memset(str, 0, len * sizeof(*str));
	return str;
}

char * sys_wide_to_utf8(const wchar_t * wide)
{
    return sys_wide_to_mb(wide, CP_UTF8);
}

wchar_t * sys_utf8_to_wide(const char * utf8)
{
    return sys_mb_to_wide(utf8, CP_UTF8);
}

char * sys_wide_to_native_mb(const wchar_t * wide)
{
    return sys_wide_to_mb(wide, CP_ACP);
}

wchar_t * sys_native_mb_to_wide(const char * native_mb)
{
    return sys_mb_to_wide(native_mb, CP_ACP);
}

wchar_t * sys_mb_to_wide(const char * mb, uint32_t code_page)
{
    int mb_length = (int)strlen(mb);
    int charcount;
    wchar_t * w;
    if (mb_length == 0)
        return new_wstr(1);
    /* Compute the length of the buffer. */
    charcount = MultiByteToWideChar(code_page, 0,
        mb, mb_length, NULL, 0);
    if (charcount == 0)
        return new_wstr(1);
    w = new_wstr(charcount);
    MultiByteToWideChar(code_page, 0, mb, mb_length,
        &w[0], charcount);
    return w;
}

char * sys_wide_to_mb(const wchar_t * wide, uint32_t code_page)
{
    int wide_length = (int)lstrlenW(wide);
    int charcount;
    char * mb;
    if (wide_length == 0)
        return new_str(1);
    /* Compute the length of the buffer we'll need. */
    charcount = WideCharToMultiByte(code_page, 0, wide,
        wide_length, NULL, 0, NULL, NULL);
    if (charcount == 0)
        return new_str(1);
    mb = new_str(charcount);
    WideCharToMultiByte(code_page, 0, wide, wide_length,
        &mb[0], charcount, NULL, NULL);
    return mb;
}
