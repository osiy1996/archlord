#include "core/vector.h"
#include "core/malloc.h"
#include <string.h>

void * vec_new(size_t element_size)
{
	return vec_reserve(NULL, element_size, 0);
}

void * vec_new_reserved(size_t element_size, uint32_t size)
{
	return vec_reserve(NULL, element_size, size);
}

void * vec_reserve(void * v, size_t element_size, uint32_t size)
{
	struct vec_header * h;
	if (!v) {
		h = alloc(sizeof(*h) + element_size * size);
		h->element_size = element_size;
		h->size = size;
		h->count = 0;
		v = (void *)((uintptr_t)h + sizeof(*h));
		return v;
	}
	h = vec__get_header(v);
	if (h->size < size) {
		h = reallocate(h, sizeof(*h) + element_size * size);
		h->size = size;
		v = (void *)((uintptr_t)h + sizeof(*h));
	}
	return v;
}

void vec_copy(void ** dst, const void * src)
{
	void * d = *dst;
	const struct vec_header * sh = vec__get_const_header(src);
	struct vec_header * dh = vec__get_header(d);
	if (dh->size < sh->size) {
		d = vec_reserve(d, sh->element_size, sh->size);
		dh = vec__get_header(d);
		*dst = d;
	}
	memcpy(d, src, sh->element_size * sh->count);
	dh->count = sh->count;
}

void vec_push_back(void ** v, const void * e)
{
	void * vec = *v;
	struct vec_header * h = vec__get_header(vec);
	if (h->count == h->size) {
		uint32_t sz = !h->size ? 8 : h->size * 2;
		vec = vec_reserve(vec, h->element_size, sz);
		h = vec__get_header(vec);
		*v = vec;
	}
	memcpy((void *)((uintptr_t)vec + (uintptr_t)h->count++ * h->element_size), e, 
		h->element_size);
}

void * vec_add_empty(void ** v)
{
	void * vec = *v;
	struct vec_header * h = vec__get_header(vec);
	void * p;
	if (h->count == h->size) {
		uint32_t sz = !h->size ? 8 : h->size * 2;
		vec = vec_reserve(vec, h->element_size, sz);
		h = vec__get_header(vec);
		*v = vec;
	}
	p = (void *)((uintptr_t)vec + (uintptr_t)h->count++ * h->element_size);
	memset(p, 0, h->element_size);
	return p;
}

void * vec_back(void * v)
{
	struct vec_header * h = vec__get_header(v);
	if (!h->count)
		return NULL;
	return (void *)((uintptr_t)v +
		(uintptr_t)h->element_size * ((uintptr_t)h->count - 1));
}

void * vec_at(void * v, uint32_t index)
{
	struct vec_header * h = vec__get_header(v);
	return (void *)((uintptr_t)v + (uintptr_t)index * h->element_size);
}

uint32_t vec_index(void * v, const void * e)
{
	return vec__element_index(vec__get_header(v), v, e);
}

void vec_erase(void * v, const void * e)
{
	struct vec_header * h = vec__get_header(v);
	uint32_t i = vec__element_index(h, v, e);
	memmove((void *)e, (void *)((uintptr_t)e + h->element_size),
		(size_t)(--h->count - i) * h->element_size);
}

uint32_t vec_erase_iterator(void * v, uint32_t index)
{
	struct vec_header * h = vec__get_header(v);
	void * e = (void *)((uintptr_t)v + index * h->element_size);
	memmove((void *)e, (void *)((uintptr_t)e + h->element_size),
		(size_t)(--h->count - index) * h->element_size);
	return index;
}

void vec_erase_chunk(void * v, uint32_t begin, uint32_t end)
{
	struct vec_header * h = vec__get_header(v);
	memmove(
		(void *)((uintptr_t)v + begin * (uintptr_t)h->element_size),
		(void *)((uintptr_t)v + end * (uintptr_t)h->element_size),
		(size_t)((h->count - end) + 1) * h->element_size);
	h->count -= end - begin;
}

void vec_free(void * v)
{
	if (!v)
		return;
	dealloc(vec__get_header(v));
}

boolean vec_set_count(void * v, uint32_t count)
{
	struct vec_header * h = vec__get_header(v);
	if (h->size < count)
		return FALSE;
	h->count = count;
	return TRUE;
}
