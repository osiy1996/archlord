#include "core/malloc.h"
#include <malloc.h>


void * alloc(size_t n)
{
	void * p;
	if (!n)
		return NULL;
	p = malloc(n);
	while (!p) {
		/* TODO: */
		p = malloc(n);
	}
	return p;
}

void * reallocate(void * p, size_t n)
{
	void * pnew;
	if (!n) {
		if (p)
			dealloc(p);
		return NULL;
	}
	pnew = realloc(p, n);
	while (!pnew) {
		/* TODO: */
		pnew = realloc(p, n);
	}
	return pnew;
}

void dealloc(void * p)
{
	free(p);
}
