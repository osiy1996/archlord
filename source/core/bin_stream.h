#ifndef _CORE_BIN_STREAM_H_
#define _CORE_BIN_STREAM_H_

#include "core/macros.h"
#include "core/types.h"
#include <stdarg.h>

BEGIN_DECLS

enum bin_stream_mode {
	BIN_STREAM_NOT_OPEN,
	BIN_STREAM_READ,
	BIN_STREAM_WRITE,
};

struct bin_stream {
	enum bin_stream_mode mode;
	void * head;
	void * cursor;
	void * end;
	size_t size;
	boolean is_mem_inherited;
};

void bstream_from_buffer(
	const void * data,
	size_t size,
	boolean inherit_memory,
	struct bin_stream ** stream);

boolean bstream_from_file(
	const char * file_path,
	struct bin_stream ** stream);

boolean bstream_from_filev(
	const char * pathfmt,
	struct bin_stream ** stream,
	...);

void bstream_for_write(
	void * data,
	size_t size,
	struct bin_stream ** stream);

boolean bstream_advance(struct bin_stream * stream, uint32_t n);

boolean bstream_seek(struct bin_stream * stream, size_t offset);

size_t bstream_offset(struct bin_stream * stream);

size_t bstream_remain(struct bin_stream * stream);

boolean bstream_read(
	struct bin_stream * stream,
	void * dst,
	uint32_t n);

boolean bstream_read_i8(struct bin_stream * stream, int8_t * n);

boolean bstream_read_i16(struct bin_stream * stream, int16_t * n);

boolean bstream_read_i32(struct bin_stream * stream, int32_t * n);

boolean bstream_read_u8(struct bin_stream * stream, uint8_t * n);

boolean bstream_read_u16(struct bin_stream * stream, uint16_t * n);

boolean bstream_read_u32(struct bin_stream * stream, uint32_t * n);

boolean bstream_read_f32(struct bin_stream * stream, float * n);

boolean bstream_read_f64(struct bin_stream * stream, double * n);

boolean bstream_write(
	struct bin_stream * stream,
	const void * src,
	uint32_t n);

uint32_t bstream_fill(
	struct bin_stream * stream,
	uint8_t value,
	uint32_t count);

boolean bstream_write_i8(struct bin_stream * stream, int8_t n);

boolean bstream_write_i16(struct bin_stream * stream, int16_t n);

boolean bstream_write_i32(struct bin_stream * stream, int32_t n);

boolean bstream_write_u8(struct bin_stream * stream, uint8_t n);

boolean bstream_write_u16(struct bin_stream * stream, uint16_t n);

boolean bstream_write_u32(struct bin_stream * stream, uint32_t n);

boolean bstream_write_f32(struct bin_stream * stream, float n);

boolean bstream_write_f64(struct bin_stream * stream, double n);

boolean bstream_write_str(
	struct bin_stream * stream,
	const char * str,
	uint32_t size);

void bstream_reset(struct bin_stream * stream);

void bstream_close(struct bin_stream * stream);

void bstream_destroy(struct bin_stream * stream);

END_DECLS

#endif /* _CORE_BIN_STREAM_H_ */