#include "core/vector.h"
#include "core/malloc.h"
#include <string.h>

struct vheader {
	size_t ele_size;
	uint32_t size;
	uint32_t count;
};

static const struct vheader * getconstheader(const void * v)
{
	return (const struct vheader *)(
		(uintptr_t)v - sizeof(struct vheader));
}

static struct vheader * getheader(void * v)
{
	return (struct vheader *)(
		(uintptr_t)v - sizeof(struct vheader));
}

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
	struct vheader * h;
	if (!v) {
		h = alloc(sizeof(*h) + element_size * size);
		h->ele_size = element_size;
		h->size = size;
		h->count = 0;
		v = (void *)((uintptr_t)h + sizeof(*h));
		return v;
	}
	h = getheader(v);
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
	const struct vheader * sh = getconstheader(src);
	struct vheader * dh = getheader(d);
	if (dh->size < sh->size) {
		d = vec_reserve(d, sh->ele_size, sh->size);
		dh = getheader(d);
		*dst = d;
	}
	memcpy(d, src, sh->ele_size * sh->count);
	dh->count = sh->count;
}

boolean vec_is_empty(const void * v)
{
	return (getconstheader(v)->count == 0);
}

uint32_t vec_count(const void * v)
{
	return getconstheader(v)->count;
}

uint32_t vec_size(const void * v)
{
	return getconstheader(v)->size;
}

void vec_push_back(void ** v, const void * e)
{
	void * vec = *v;
	struct vheader * h = getheader(vec);
	if (h->count == h->size) {
		uint32_t sz = !h->size ? 8 : h->size * 2;
		vec = vec_reserve(vec, h->ele_size, sz);
		h = getheader(vec);
		*v = vec;
	}
	memcpy((void *)((uintptr_t)vec + (uintptr_t)h->count++ * h->ele_size), e, 
		h->ele_size);
}

void * vec_add_empty(void ** v)
{
	void * vec = *v;
	struct vheader * h = getheader(vec);
	void * p;
	if (h->count == h->size) {
		uint32_t sz = !h->size ? 8 : h->size * 2;
		vec = vec_reserve(vec, h->ele_size, sz);
		h = getheader(vec);
		*v = vec;
	}
	p = (void *)((uintptr_t)vec + (uintptr_t)h->count++ * h->ele_size);
	memset(p, 0, h->ele_size);
	return p;
}

void * vec_back(void * v)
{
	struct vheader * h = getheader(v);
	if (!h->count)
		return NULL;
	return (void *)((uintptr_t)v +
		(uintptr_t)h->ele_size * ((uintptr_t)h->count - 1));
}

void * vec_at(void * v, uint32_t index)
{
	struct vheader * h = getheader(v);
	return (void *)((uintptr_t)v + (uintptr_t)index * h->ele_size);
}

static uint32_t eindex(
	const struct vheader * h,
	const void * v,
	const void * e)
{
	return (uint32_t)(((uintptr_t)e - (uintptr_t)v) / h->ele_size);
}

uint32_t vec_index(void * v, const void * e)
{
	return eindex(getheader(v), v, e);
}

void vec_erase(void * v, const void * e)
{
	struct vheader * h = getheader(v);
	uint32_t i = eindex(h, v, e);
	memmove((void *)e, (void *)((uintptr_t)e + h->ele_size),
		(size_t)(--h->count - i) * h->ele_size);
}

uint32_t vec_erase_iterator(void * v, uint32_t index)
{
	struct vheader * h = getheader(v);
	void * e = (void *)((uintptr_t)v + index * h->ele_size);
	memmove((void *)e, (void *)((uintptr_t)e + h->ele_size),
		(size_t)(--h->count - index) * h->ele_size);
	return index;
}

void vec_erase_chunk(void * v, uint32_t begin, uint32_t end)
{
	struct vheader * h = getheader(v);
	memmove(
		(void *)((uintptr_t)v + begin * (uintptr_t)h->ele_size),
		(void *)((uintptr_t)v + end * (uintptr_t)h->ele_size),
		(size_t)((h->count - end) + 1) * h->ele_size);
	h->count -= end - begin;
}

void vec_clear(void * v)
{
	getheader(v)->count = 0;
}

void vec_free(void * v)
{
	if (!v)
		return;
	dealloc(getheader(v));
}

boolean vec_set_count(void * v, uint32_t count)
{
	struct vheader * h = getheader(v);
	if (h->size < count)
		return FALSE;
	h->count = count;
	return TRUE;
}
