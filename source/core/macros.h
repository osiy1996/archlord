#ifndef _CORE_MACROS_H_
#define _CORE_MACROS_H_

#ifdef __cplusplus
#define BEGIN_DECLS extern "C" {
#define END_DECLS }
#else
#define BEGIN_DECLS
#define END_DECLS
#endif

#ifndef NULL
#ifdef __cplusplus
#define NULL (0L)
#else /* !__cplusplus */
#define NULL ((void *)0)
#endif /* !__cplusplus */
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE (!FALSE)
#endif

#undef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#undef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

/* Count of elements in an array.
 * Size of the array must be known at compile-time.
 * Usage with dynamic arrays will result in undefined behaviour.
 */
#define COUNT_OF(arr) (sizeof(arr) / sizeof((arr)[0]))

#define CLAMP(value, min, max) (((value) <= (min)) ? (min) :\
	((value) >= (max)) ? (max) : (value))

#endif /* _CORE_MACROS_H_ */
