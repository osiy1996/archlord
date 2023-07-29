#ifndef _UTIL_RING_BUFFER_H_
#define _UTIL_RING_BUFFER_H_

#include "core/macros.h"
#include "core/types.h"

BEGIN_DECLS

struct ring_buffer {
	uint8_t * head;
	uint8_t * tail;
	uint8_t * rcursor;
	uint8_t * wcursor;
	size_t size;
	size_t usage;
};

/*
 * Returns the required size for a ring buffer.
 */
static inline size_t rb_req_size(size_t size)
{
	return (sizeof(struct ring_buffer) + size);
}

/*
 * Creates a ring buffer from pre-allocated memory.
 */
struct ring_buffer * rb_create_prealloc(void * mem, size_t size);

/*
 * Creates a ring buffer with given size.
 */
struct ring_buffer * rb_create(size_t size);

/*
 * Resets the ring buffer cursors.
 */
void rb_reset(struct ring_buffer * rb);

/*
 * Deallocates ring buffer.
 */
void rb_destroy(struct ring_buffer * rb);

/*
 * Returns the writable byte count.
 */
static inline size_t rb_avail_write(struct ring_buffer * rb)
{
	return (rb->size - rb->usage);
}

/*
 * Attempts to write to ring buffer.
 * Returns the number of bytes written.
 */
size_t rb_write(
	struct ring_buffer * rb,
	const void * data,
	size_t size);

/*
 * Returns the readable byte count.
 */
static inline size_t rb_avail_read(struct ring_buffer * rb)
{
	return rb->usage;
}

/*
 * Attempts to read `maxcount` bytes from ring buffer.
 * Returns the number of bytes that were read.
 */
size_t rb_read(
	struct ring_buffer * rb,
	void * buf,
	size_t maxcount);

/*
 * Attempts to read `maxcount` bytes from ring buffer.
 * Returns the number of bytes that were read.
 *
 * Unlike ``rb_read``, this function does not modify 
 * read cursor.
 */
size_t rb_read_tmp(
	struct ring_buffer * rb,
	void * buf,
	size_t maxcount);

/*
 * Attempts to forward read cursor `count` bytes.
 *
 * If the number of bytes available for read 
 * is less than `count`, returns FALSE immediately 
 * without changing read cursor.
 */
boolean rb_forward_read(struct ring_buffer * rb, size_t count);

/*
 * `dst` is filled with what is available in `src`.
 * Returns the number of bytes consumed.
 */
size_t rb_consume_other(
	struct ring_buffer * dst,
	struct ring_buffer * src);

END_DECLS

#endif /* _UTIL_RING_BUFFER_H_ */