#ifndef _CORE_MALLOC_H_
#define _CORE_MALLOC_H_

#include "core/macros.h"
#include "core/types.h"

BEGIN_DECLS

void * alloc(size_t n);

void * reallocate(void * p, size_t n);

void dealloc(void * p);

END_DECLS

#endif /* _CORE_MALLOC_H_ */
