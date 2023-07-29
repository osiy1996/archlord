#include "core/bin_stream.h"
#include "core/malloc.h"
#include "core/file_system.h"
#include <string.h>

struct bin_stream * create_bstream(enum bin_stream_mode mode)
{
	struct bin_stream * stream = alloc(sizeof(*stream));
	memset(stream, 0, sizeof(*stream));
	stream->mode = mode;
	return stream;
}

void bstream_from_buffer(
	const void * data,
	size_t size,
	boolean inherit_memory,
	struct bin_stream ** stream)
{
	struct bin_stream * s = create_bstream(BIN_STREAM_READ);
	s->head = (void *)data;
	s->cursor = s->head;
	s->end = (void *)((uintptr_t)data + size);
	s->size = size;
	s->is_mem_inherited = inherit_memory;
	*stream = s;
}

boolean bstream_from_file(
	const char * file_path,
	struct bin_stream ** stream)
{
	size_t size;
	void * data;
	if (!get_file_size(file_path, &size))
		return FALSE;
	data = alloc(size);
	if (!load_file(file_path, data, size)) {
		dealloc(data);
		return FALSE;
	}
	bstream_from_buffer(data, size, TRUE, stream);
	return TRUE;
}

boolean bstream_from_filev(
	const char * pathfmt,
	struct bin_stream ** stream,
	...)
{
	char path[1024];
	va_list ap;
	va_start(ap, stream);
	make_pathv(path, sizeof(path), pathfmt, ap);
	va_end(ap);
	return bstream_from_file(path, stream);
}

void bstream_for_write(
	void * data,
	size_t size,
	struct bin_stream ** stream)
{
	struct bin_stream * s = create_bstream(BIN_STREAM_WRITE);
	if (!data) {
		data = alloc(size);
		s->is_mem_inherited = TRUE;
	}
	s->head = data;
	s->cursor = data;
	s->end = (void *)((uintptr_t)data + size);
	s->size = size;
	*stream = s;
}

boolean bstream_advance(struct bin_stream * stream, uint32_t n)
{
	if (bstream_remain(stream) < n)
		return FALSE;
	stream->cursor = (void *)((uintptr_t)stream->cursor + n);
	return TRUE;
}

boolean bstream_seek(struct bin_stream * stream, size_t offset)
{
	if (offset > stream->size)
		return FALSE;
	stream->cursor = (void *)((uintptr_t)stream->head + offset);
	return TRUE;
}

size_t bstream_offset(struct bin_stream * stream)
{
	return (size_t)(
		(uintptr_t)stream->cursor - (uintptr_t)stream->head);
}

size_t bstream_remain(struct bin_stream * stream)
{
	if (stream->mode == BIN_STREAM_NOT_OPEN)
		return 0;
	return (size_t)(
		(uintptr_t)stream->end - (uintptr_t)stream->cursor);
}

boolean bstream_read(
	struct bin_stream * stream,
	void * dst,
	uint32_t n)
{
	if (stream->mode != BIN_STREAM_READ ||
		bstream_remain(stream) < n)
		return FALSE;
	memcpy(dst, stream->cursor, n);
	return bstream_advance(stream, n);
}

boolean bstream_read_i8(struct bin_stream * stream, int8_t * n)
{
	return bstream_read(stream, n, sizeof(*n));
}

boolean bstream_read_i16(struct bin_stream * stream, int16_t * n)
{
	return bstream_read(stream, n, sizeof(*n));
}

boolean bstream_read_i32(struct bin_stream * stream, int32_t * n)
{
	return bstream_read(stream, n, sizeof(*n));
}

boolean bstream_read_u8(struct bin_stream * stream, uint8_t * n)
{
	return bstream_read(stream, n, sizeof(*n));
}

boolean bstream_read_u16(struct bin_stream * stream, uint16_t * n)
{
	return bstream_read(stream, n, sizeof(*n));
}

boolean bstream_read_u32(struct bin_stream * stream, uint32_t * n)
{
	return bstream_read(stream, n, sizeof(*n));
}

boolean bstream_read_f32(struct bin_stream * stream, float * n)
{
	return bstream_read(stream, n, sizeof(*n));
}

boolean bstream_read_f64(struct bin_stream * stream, double * n)
{
	return bstream_read(stream, n, sizeof(*n));
}

boolean bstream_write(
	struct bin_stream * stream,
	const void * src,
	uint32_t n)
{
	if (stream->mode != BIN_STREAM_WRITE ||
		bstream_remain(stream) < n)
		return FALSE;
	memcpy(stream->cursor, src, n);
	return bstream_advance(stream, n);
}

uint32_t bstream_fill(
	struct bin_stream * stream,
	uint8_t value,
	uint32_t count)
{
	if (stream->mode != BIN_STREAM_WRITE ||
		bstream_remain(stream) < count)
		return FALSE;
	memset(stream->cursor, value, count);
	return bstream_advance(stream, count);
}

boolean bstream_write_i8(struct bin_stream * stream, int8_t n)
{
	return bstream_write(stream, &n, sizeof(n));
}

boolean bstream_write_i16(struct bin_stream * stream, int16_t n)
{
	return bstream_write(stream, &n, sizeof(n));
}

boolean bstream_write_i32(struct bin_stream * stream, int32_t n)
{
	return bstream_write(stream, &n, sizeof(n));
}

boolean bstream_write_u8(struct bin_stream * stream, uint8_t n)
{
	return bstream_write(stream, &n, sizeof(n));
}

boolean bstream_write_u16(struct bin_stream * stream, uint16_t n)
{
	return bstream_write(stream, &n, sizeof(n));
}

boolean bstream_write_u32(struct bin_stream * stream, uint32_t n)
{
	return bstream_write(stream, &n, sizeof(n));
}

boolean bstream_write_f32(struct bin_stream * stream, float n)
{
	return bstream_write(stream, &n, sizeof(n));
}

boolean bstream_write_f64(struct bin_stream * stream, double n)
{
	return bstream_write(stream, &n, sizeof(n));
}

boolean bstream_write_str(
	struct bin_stream * stream,
	const char * str,
	uint32_t size)
{
	size_t len = strlen(str);
	if (!size)
		return bstream_write(stream, str, (uint32_t)len + 1);
	if (bstream_remain(stream) < size)
		return FALSE;
	if (len >= size)
		len = (size_t)size - 1;
	memcpy(stream->cursor, str, len);
	memset((void *)((uintptr_t)stream->cursor + len), 0,
		size - len);
	bstream_advance(stream, size);
	return TRUE;
}

void bstream_reset(struct bin_stream * stream)
{
	stream->cursor = stream->head;
}

void bstream_close(struct bin_stream * stream)
{
	if (stream->mode == BIN_STREAM_NOT_OPEN)
		return;
	if (stream->is_mem_inherited)
		dealloc(stream->head);
	stream->mode = BIN_STREAM_NOT_OPEN;
}

void bstream_destroy(struct bin_stream * stream)
{
	if (stream->mode != BIN_STREAM_NOT_OPEN)
		bstream_close(stream);
	dealloc(stream);
}
