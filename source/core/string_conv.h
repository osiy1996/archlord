#ifndef _CORE_STRING_CONV_H_
#define _CORE_STRING_CONV_H_

#include "core/macros.h"
#include "core/types.h"

//
//  Code Page Default Values.
//  Please Use Unicode, either UTF-16 (as in WCHAR) or UTF-8 (code page CP_ACP)
//
#define CONV_CP_ACP         0     // default to ANSI code page
#define CONV_CP_OEMCP       1     // default to OEM  code page
#define CONV_CP_MACCP       2     // default to MAC  code page
#define CONV_CP_THREAD_ACP  3     // current thread's ANSI code page
#define CONV_CP_SYMBOL      42    // SYMBOL translations
#define CONV_CP_UTF7        65000 // UTF-7 translation
#define CONV_CP_UTF8        65001 // UTF-8 translation

BEGIN_DECLS

char * sys_wide_to_utf8(const wchar_t * wide);

wchar_t * sys_utf8_to_wide(const char * utf8);

char * sys_wide_to_native_mb(const wchar_t * wide);

wchar_t * sys_native_mb_to_wide(const char * native_mb);

wchar_t * sys_mb_to_wide(const char * mb, uint32_t code_page);

char * sys_wide_to_mb(const wchar_t * wide, uint32_t code_page);

END_DECLS

#endif /* _CORE_STRING_CONV_H_ */
