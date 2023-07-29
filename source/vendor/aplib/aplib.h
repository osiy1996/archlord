#ifndef _AP_LIB_H_
#define _AP_LIB_H_

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned int __cdecl aP_pack(
	unsigned char *source,
	unsigned char *destination,
	unsigned int length,
	unsigned char *workmem,
	int (__cdecl *callback) (unsigned int, unsigned int));

extern unsigned int __cdecl aP_depack_asm(
	unsigned char *source,
	unsigned char *destination);


#ifdef __cplusplus
}
#endif

#endif /* _AP_LIB_H_ */