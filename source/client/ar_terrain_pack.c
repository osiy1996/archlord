#include "ar_terrain_internal.h"
#include <core/core.h>
#include <core/malloc.h>
#include <core/log.h>
#include <core/bin_stream.h>
#include <core/file_system.h>
#include <core/string.h>
#include <vendor/aplib/aplib.h>
#include <string.h>

struct pack * create_pack()
{
	struct pack * p;
	p = alloc(sizeof(*p));
	memset(p, 0, sizeof(*p));
	return p;
}

boolean unpack_terrain_file(
	const char * pathfmt,
	boolean decompress,
	struct pack ** pack,
	...)
{
	struct bin_stream * stream;
	char path[512];
	const char * header = "MagPack Ver 0.1a";
	char header_in_file[50] = "";
	va_list ap;
	size_t size;
	uint32_t packed_size[256];
	void * work_mem;
	struct pack * p;
	uint32_t i;
	memset(packed_size, 0, sizeof(packed_size));
	va_start(ap, pack);
	make_pathv(path, sizeof(path), pathfmt, ap);
	va_end(ap);
	if (!get_file_size(path, &size))
		return FALSE;
	if (!bstream_from_file(path, &stream))
		return FALSE;
	if (!bstream_read(stream, header_in_file, 50) ||
		strcmp(header_in_file, header) != 0) {
		bstream_destroy(stream);
		return FALSE;
	}
	p = create_pack();
	for (i = 0; i < 256; i++) {
		struct packed_file * s = &p->files[i];
		uint8_t name_len = 0;
		if (!bstream_read_u8(stream, &name_len) ||
			name_len >= COUNT_OF(s->name) ||
			!bstream_read(stream, s->name, name_len) ||
			!bstream_read_u32(stream, &packed_size[i])) {
			destroy_pack(p);
			bstream_destroy(stream);
			return FALSE;
		}
		s->name[name_len] = '\0';
		packed_size[i] ^= 0x00006969;
	}
	/* We skip a byte here, client loads until it comes 
	 * across a sector with '0' as its name_len. */
	bstream_advance(stream, 1);
	work_mem = alloc((size_t)1u << 24);
	for (i = 0; i < 256; i++) {
		struct packed_file * s = &p->files[i];
		if (!bstream_read_u32(stream, &s->decompressed_size)) {
			dealloc(work_mem);
			destroy_pack(p);
			bstream_destroy(stream);
			return FALSE;
		}
		s->decompressed_size ^= 0x00006969;
		if (decompress) {
			s->size = aP_depack_asm(
				(unsigned char *)stream->cursor,
				(unsigned char *)work_mem);
			if (s->size != s->decompressed_size) {
				dealloc(work_mem);
				destroy_pack(p);
				bstream_destroy(stream);
				return FALSE;
			}
			s->data = alloc(s->decompressed_size);
			memcpy(s->data, work_mem, s->size);
			if (!bstream_advance(stream, packed_size[i])) {
				dealloc(work_mem);
				destroy_pack(p);
				bstream_destroy(stream);
				return FALSE;
			}
		}
		else {
			s->data = alloc(packed_size[i]);
			s->size = packed_size[i];
			if (!bstream_read(stream, s->data, s->size)) {
				dealloc(work_mem);
				destroy_pack(p);
				bstream_destroy(stream);
				return FALSE;
			}
		}
	}
	dealloc(work_mem);
	bstream_destroy(stream);
	*pack = p;
	return TRUE;
}

boolean pack_terrain_file(
	const char * pathfmt,
	const struct pack * pack,
	...)
{
	struct bin_stream * stream;
	char path[512];
	const char * header = "MagPack Ver 0.1a";
	va_list ap;
	uint32_t i;
	va_start(ap, pack);
	make_pathv(path, sizeof(path), pathfmt, ap);
	strlcat(path, ".npack", sizeof(path));
	va_end(ap);
	bstream_for_write(NULL, (size_t)1u << 25, &stream);
	if (!bstream_write_str(stream, header, 50)) {
		bstream_destroy(stream);
		return FALSE;
	}
	for (i = 0; i < 256; i++) {
		const struct packed_file * s = &pack->files[i];
		uint32_t len = (uint32_t)strlen(s->name);
		if (!bstream_write_u8(stream, (uint8_t)len) ||
			!bstream_write_str(stream, s->name, len) ||
			!bstream_write_u32(stream, s->size)) {
			bstream_destroy(stream);
			return FALSE;
		}
	}
	/* Empty byte to indicate that we are 
	 * done writing headers. */
	if (!bstream_fill(stream, 0, 1)) {
		bstream_destroy(stream);
		return FALSE;
	}
	for (i = 0; i < 256; i++) {
		const struct packed_file * s = &pack->files[i];
		if (!bstream_write_u32(stream, s->size)) {
			bstream_destroy(stream);
			return FALSE;
		}
		if (!bstream_write(stream, s->data, s->size)) {
			bstream_destroy(stream);
			return FALSE;
		}
	}
	if (!make_file(path, stream->head, bstream_offset(stream))) {
		bstream_destroy(stream);
		return FALSE;
	}
	bstream_destroy(stream);
	return TRUE;
}

void destroy_pack(struct pack * pack)
{
	uint32_t i;
	for (i = 0; i < COUNT_OF(pack->files); i++)
		dealloc(pack->files[i].data);
	dealloc(pack);
}

boolean decompress_packed_file(
	struct packed_file * file,
	void ** work_mem,
	size_t * work_size)
{
	uint32_t sz;
	void * wm = *work_mem;
	if (*work_size < file->decompressed_size) {
		*work_size = file->decompressed_size;
		wm = reallocate(wm, *work_size);
		*work_mem = wm;
	}
	sz = aP_depack_asm(
		(unsigned char *)file->data,
		(unsigned char *)wm);
	if (sz != file->decompressed_size)
		return FALSE;
	file->size = sz;
	file->data = reallocate(file->data, file->decompressed_size);
	memcpy(file->data, wm, file->decompressed_size);
	return TRUE;
}
