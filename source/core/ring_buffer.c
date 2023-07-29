#include "core/ring_buffer.h"
#include "core/malloc.h"
#include <string.h>

struct ring_buffer * rb_create_prealloc(void * mem, size_t size)
{
	struct ring_buffer * rb = mem;
	rb->head = (uint8_t *)((uintptr_t)rb + sizeof(*rb));
	rb->tail = rb->head + size;
	rb->rcursor = rb->head;
	rb->wcursor = rb->head;
	rb->size = size;
	rb->usage = 0;
	return rb;
}

struct ring_buffer * rb_create(size_t size)
{
	struct ring_buffer * rb = alloc(sizeof(*rb) + size);
	rb->head = (uint8_t *)((uintptr_t)rb + sizeof(*rb));
	rb->tail = rb->head + size;
	rb->rcursor = rb->head;
	rb->wcursor = rb->head;
	rb->size = size;
	rb->usage = 0;
	return rb;
}

void rb_reset(struct ring_buffer * rb)
{
	rb->rcursor = rb->wcursor = rb->head;
	rb->usage = 0;
}

void rb_destroy(struct ring_buffer * rb)
{
	dealloc(rb);
}

size_t rb_write(
	struct ring_buffer * rb,
	const void * data,
	size_t size)
{
	size_t c = size;
	size_t r = rb_avail_write(rb);
	size_t to_end = (size_t)(rb->tail - rb->wcursor);
	if (c > r)
		c = r;
	if (c >= to_end) {
		memcpy(rb->wcursor, data, to_end);
		memcpy(rb->head, (void *)((uintptr_t)data + to_end),
			c - to_end);
		rb->wcursor = rb->head + (c - to_end);
		rb->usage += c;
	}
	else {
		memcpy(rb->wcursor, data, c);
		rb->wcursor += c;
		rb->usage += c;
	}
	return c;
}

size_t rb_read(
	struct ring_buffer * rb,
	void * buf,
	size_t maxcount)
{
	size_t c = maxcount;
	size_t r = rb_avail_read(rb);
	size_t to_end = (size_t)(rb->tail - rb->rcursor);
	if (c > r)
		c = r;
	if (!c)
		return 0;
	if (c >= to_end) {
		memcpy(buf, rb->rcursor, to_end);
		memcpy((void *)((uintptr_t)buf + to_end), rb->head,
			c - to_end);
		rb->rcursor = rb->head + (c - to_end);
		rb->usage -= c;
	}
	else {
		memcpy(buf, rb->rcursor, c);
		rb->rcursor += c;
		rb->usage -= c;
	}
	return c;
}

size_t rb_read_tmp(
	struct ring_buffer * rb,
	void * buf,
	size_t maxcount)
{
	size_t c = maxcount;
	size_t r = rb_avail_read(rb);
	size_t to_end = (size_t)(rb->tail - rb->rcursor);
	if (c > r)
		c = r;
	if (!c)
		return 0;
	if (c >= to_end) {
		memcpy(buf, rb->rcursor, to_end);
		memcpy((void *)((uintptr_t)buf + to_end), rb->head,
			c - to_end);
	}
	else {
		memcpy(buf, rb->rcursor, c);
	}
	return c;
}

boolean rb_forward_read(struct ring_buffer * rb, size_t count)
{
	if (rb_avail_read(rb) < count)
		return FALSE;
	size_t to_end = (size_t)(rb->tail - rb->rcursor);
	if (count >= to_end) {
		rb->rcursor = rb->head + (count - to_end);
		rb->usage -= count;
	}
	else {
		rb->rcursor += count;
		rb->usage -= count;
	}
	return TRUE;
}

size_t rb_consume_other(
	struct ring_buffer * dst,
	struct ring_buffer * src)
{
	size_t avail_r = rb_avail_read(src);
	size_t avail_w = rb_avail_write(dst);
	size_t c = MIN(avail_r, avail_w);
	size_t to_end_w = (size_t)(dst->tail - dst->wcursor);
	if (!c)
		return 0;
	if (c >= to_end_w) {
		rb_read(src, dst->wcursor, to_end_w);
		rb_read(src, dst->head, c - to_end_w);
		dst->wcursor = dst->head + (c - to_end_w);
		dst->usage += c;
	}
	else {
		rb_read(src, dst->wcursor, c);
		dst->wcursor += c;
		dst->usage += c;
	}
	return c;
}
