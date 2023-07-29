#ifndef _AU_MD5_H_
#define _AU_MD5_H_

#include "core/macros.h"
#include "core/types.h"

BEGIN_DECLS

boolean au_md5_crypt(
	void * data,
	size_t size,
	const uint8_t * key,
	uint32_t key_size);

END_DECLS

#endif /* _AU_MD5_H_ */
