#include "client/ac_renderware.h"

#include "core/log.h"

struct header {
	uint32_t type;
	uint32_t length;
	uint32_t library_id;
};

boolean ac_renderware_read_chunk(
	struct bin_stream * stream,
	RwChunkHeaderInfo * chunk)
{
	struct header h;
	if (!bstream_read(stream, &h, sizeof(h)))
		return FALSE;
	chunk->type = h.type;
	chunk->length = h.length;
	/* Check for old library ID. */
	if (!(h.library_id & 0xffff0000)) {
		chunk->version = h.library_id << 8;
		chunk->buildNum = 0;
	}
	else {
		/* Unpack new library ID. */
		chunk->version = RWLIBRARYIDUNPACKVERSION(h.library_id);
		chunk->buildNum = RWLIBRARYIDUNPACKBUILDNUM(h.library_id);
	}
	return TRUE;
}

boolean ac_renderware_find_chunk(
	struct bin_stream * stream,
	uint32_t type,
	RwChunkHeaderInfo * chunk)
{
	RwChunkHeaderInfo c;
	while (ac_renderware_read_chunk(stream, &c)) {
		if (c.type == type) {
			if (c.version < rwLIBRARYBASEVERSION) {
				ERROR("Version 0x%x is older (0x%x).",
					c.version, rwLIBRARYBASEVERSION);
				return FALSE;
			}
			if (c.version > rwLIBRARYCURRENTVERSION) {
				ERROR("Version 0x%x is newer (0x%x).",
					c.version, rwLIBRARYCURRENTVERSION);
				return FALSE;
			}
			if (chunk)
				*chunk = c;
			return TRUE;
		}
		if (!bstream_advance(stream, c.length))
			return FALSE;
	}
	return FALSE;
}

boolean ac_renderware_begin_chunk(
	struct bin_stream * stream,
	uint32_t type,
	size_t * offset)
{
	struct header h = { type, 0, 
		RWLIBRARYIDPACK(rwLIBRARYCURRENTVERSION, RWBUILDNUMBER) };
	*offset = bstream_offset(stream);
	return bstream_write(stream, &h, sizeof(h));
}

boolean ac_renderware_end_chunk(
	struct bin_stream * stream,
	size_t offset)
{
	size_t end = bstream_offset(stream);
	size_t len = end - offset;
	if (len < rwCHUNKHEADERSIZE || len > UINT32_MAX)
		return FALSE;
	len -= rwCHUNKHEADERSIZE;
	if (!bstream_seek(stream, offset + 4) ||
		!bstream_write_u32(stream, (uint32_t)len))
		return FALSE;
	return bstream_seek(stream, end);
}
