#ifndef _AC_RENDERWARE_H_
#define _AC_RENDERWARE_H_

#include "core/macros.h"
#include "core/types.h"
#include "core/bin_stream.h"

#include "vendor/RenderWare/rwcore.h"
#include "vendor/RenderWare/rpworld.h"

BEGIN_DECLS

boolean ac_renderware_read_chunk(
	struct bin_stream * stream,
	RwChunkHeaderInfo * chunk);

boolean ac_renderware_find_chunk(
	struct bin_stream * stream,
	uint32_t type,
	RwChunkHeaderInfo * chunk);

boolean ac_renderware_begin_chunk(
	struct bin_stream * stream,
	uint32_t type,
	size_t * offset);

boolean ac_renderware_end_chunk(
	struct bin_stream * stream,
	size_t offset);

END_DECLS

#endif /* _AC_RENDERWARE_H_ */