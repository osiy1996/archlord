#ifndef _UTIL_STRING_H_
#define _UTIL_STRING_H_

#include "core/macros.h"
#include "core/types.h"
#include <string.h>

BEGIN_DECLS

/*
 * Converts a string of hex characters to binary array.
 *
 * E.g. "a1B2c3D4" -> { 0xa1, 0xb2, 0xc3, 0xd4 }
 * Characters in string must be continuous, without spaces.
 *
 * Returns the number of bytes written to `buffer`.
 */
size_t strtohex(const char * str, uint8_t * buffer, size_t maxlen);

/*
 * Size-bounded string copy.
 * Guarantees that `dst` is zero-terminated.
 *
 * Returns length of `src`.
 */
size_t strlcpy(char * dst, const char * src, size_t maxlen);

/*
 * Size-bounded string contcatenate.
 * Guarantees that [dst] is zero-terminated.
 *
 * Returns length of `src`.
 */
size_t strlcat(char * dst, const char * src, size_t maxlen);

/*
 * Converts entire string to lower-case.
 *
 * Returns `s`.
 */
char * strlower(char * s);

/*
 * Converts entire string to upper-case.
 *
 * Returns `s`.
 */
char * strupper(char * s);

/*
 * Compares two strings, ignoring case.
 */
int strcasecmp(const char * s1, const char * s2);

/*
 * Finds a substring in a string, case insensitive.
 */
char * stristr(const char * string, const char * substring);

/*
 * Is string empty?
 */
boolean strisempty(const char * str);

END_DECLS

#endif /* _UTIL_STRING_H_ */