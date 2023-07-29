#ifndef _AC_RENDER_H_
#define _AC_RENDER_H_

#include "core/macros.h"
#include "core/types.h"
#include "public/ap_module.h"
#include "client/ac_define.h"
#include "vendor/RenderWare/rwplcore.h"

/* 'CRT` stands for ClumpRenderType. */

#define AC_RENDER_CRT_DATA_CUST_DATA_NONE -1
#define AC_RENDER_CRT_STREAM_NUM "CRT_CLUMP_RENDER_TYPE_NUM"
#define AC_RENDER_CRT_STREAM_RENDERTYPE "CRT_CLUMP_RENDER_TYPE"
#define AC_RENDER_CRT_STREAM_CUSTOM_DATA1 "CRT_CUSTOM_DATA1"
#define AC_RENDER_CRT_STREAM_CUSTOM_DATA2 "CRT_CUSTOM_DATA2"

BEGIN_DECLS

struct ac_render_crt_data {
	int32_t * render_type;
	int32_t * cust_data;
};

struct ac_render_crt {
	uint32_t set_count;
	struct ac_render_crt_data render_type;
	uint32_t cb_count;
	int32_t * cust_data;
};

void ac_render_reserve_crt_set(
	struct ac_render_crt * crt,
	uint32_t count);

boolean ac_render_stream_read_crt(
	struct ap_module_stream * stream,
	struct ac_render_crt * crt);

END_DECLS

#endif /* _AC_RENDER_H_ */
