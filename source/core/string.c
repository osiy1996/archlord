#include "core/string.h"
#include <string.h>
#include <ctype.h>

/*
* Converts hex character to binary value.
*
* Returns false if hex character is invalid.
*/
static boolean tobin(char c, uint8_t * b)
{
	switch (c) {
	case '0':
		*b = 0;
		break;
	case '1':
		*b = 1;
		break;
	case '2':
		*b = 2;
		break;
	case '3':
		*b = 3;
		break;
	case '4':
		*b = 4;
		break;
	case '5':
		*b = 5;
		break;
	case '6':
		*b = 6;
		break;
	case '7':
		*b = 7;
		break;
	case '8':
		*b = 8;
		break;
	case '9':
		*b = 9;
		break;
	case 'A':
	case 'a':
		*b = 0xa;
		break;
	case 'B':
	case 'b':
		*b = 0xb;
		break;
	case 'C':
	case 'c':
		*b = 0xc;
		break;
	case 'D':
	case 'd':
		*b = 0xd;
		break;
	case 'E':
	case 'e':
		*b = 0xe;
		break;
	case 'F':
	case 'f':
		*b = 0xf;
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

size_t strtohex(const char * str, uint8_t * buffer, size_t maxlen)
{
	size_t len = strlen(str);
	size_t i;
	size_t c = 0;
	for (i = 0; i < len / 2 && maxlen--; i++) {
		char h = str[i * 2];
		char l = str[i * 2 + 1];
		uint8_t bh = 0;
		uint8_t bl = 0;
		buffer[c] = 0;
		if (tobin(h, &bh))
			buffer[c] |= (uint8_t)((uint32_t)bh << 4);
		if (tobin(l, &bl))
			buffer[c] |= bl & 0xF;
		c++;
	}
	return c;
}

size_t strlcpy(char * dst, const char * src, size_t maxlen)
{
	const size_t srclen = strlen(src);
	if (srclen + 1 < maxlen) {
		memcpy(dst, src, srclen + 1);
	}
	else if (maxlen != 0) {
		memcpy(dst, src, maxlen - 1);
		dst[maxlen - 1] = '\0';
	}
	return srclen;
}

size_t strlcat(char * dst, const char * src, size_t maxlen)
{
	const size_t dstlen = strlen(dst);
	const size_t srclen = strlen(src);
	if (dstlen + srclen + 1 < maxlen) {
		memcpy(dst + dstlen, src, srclen + 1);
	}
	else if (maxlen > dstlen) {
		memcpy(dst + dstlen, src, maxlen - dstlen - 1);
		dst[maxlen - 1] = '\0';
	}
	return srclen;
}

char * strlower(char * s)
{
	char * tmp = s;
	for (; *tmp; tmp++)
		*tmp = tolower((unsigned char)*tmp);
	return s;
}

char * strupper(char * s)
{
	char * tmp = s;
	for (; *tmp; tmp++)
		*tmp = toupper((unsigned char)*tmp);
	return s;
}

int strcasecmp(const char * s1, const char * s2)
{
	const unsigned char * p1 = (const unsigned char *) s1;
	const unsigned char * p2 = (const unsigned char *) s2;
	int result;
	if (p1 == p2)
		return 0;
	while ((result = tolower(*p1) - tolower(*p2++)) == 0) {
		if (*p1++ == '\0')
			break;
	}
	return result;
}

char * stristr(const char * string, const char * substring)
{
	char * pptr = (char *)substring;
	char * sptr;
	char * start = (char *)string;
	size_t  slen = strlen(string);
	size_t plen = strlen(substring);
	for (; slen >= plen; start++, slen--) {
		/* find start of pattern in string */
		while (toupper(*start) != toupper(*substring)) {
			start++;
			slen--;
			/* if pattern longer than string */
			if (slen < plen)
				return NULL;
		}
		sptr = start;
		pptr = (char *)substring;
		while (toupper(*sptr) == toupper(*pptr)) {
			sptr++;
			pptr++;
			/* if end of pattern then pattern was found */
			if ('\0' == *pptr)
				return start;
		}
	}
	return NULL;
}

boolean strisempty(const char * str)
{
	return (str[0] == '\0');
}