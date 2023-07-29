#include "client/ac_render.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include <assert.h>

void ac_render_reserve_crt_set(
	struct ac_render_crt * crt,
	uint32_t count)
{
	struct ac_render_crt_data d = { 0 };
	uint32_t i;
	if (!(count > crt->set_count))
		return;
	d.render_type = alloc(count * sizeof(*d.render_type));
	d.cust_data = alloc(count * sizeof(*d.cust_data));
	memcpy(d.render_type, crt->render_type.render_type,
		crt->set_count * sizeof(*d.render_type));
	memcpy(d.cust_data, crt->render_type.cust_data,
		crt->set_count * sizeof(*d.cust_data));
	for (i = crt->set_count; i < count; i++) {
		d.render_type[i] = 0;
		d.cust_data[i] = AC_RENDER_CRT_DATA_CUST_DATA_NONE;
	}
	dealloc(crt->render_type.render_type);
	dealloc(crt->render_type.cust_data);
	crt->render_type = d;
	crt->set_count = count;
}

boolean ac_render_stream_read_crt(
	struct ap_module_stream * stream,
	struct ac_render_crt * crt)
{
	const char * value_name = ap_module_stream_get_value_name(stream);
	if (strncmp(value_name, AC_RENDER_CRT_STREAM_NUM,
			strlen(AC_RENDER_CRT_STREAM_NUM)) == 0) {
		uint32_t count;
		ap_module_stream_get_u32(stream, &count);
		ac_render_reserve_crt_set(crt, count);
	}
	else if (strncmp(value_name, AC_RENDER_CRT_STREAM_RENDERTYPE,
			strlen(AC_RENDER_CRT_STREAM_RENDERTYPE)) == 0) {
		const char * value = ap_module_stream_get_value(stream);
		uint32_t index;
		int32_t render_type;
		int32_t cust_data;
		if (sscanf(value, "%u:%d:%d", &index, &render_type, 
				&cust_data) != 3) {
			ERROR("sscanf() failed.");
			return FALSE;
		}
		if (index >= crt->set_count) {
			ERROR("Invalid index value.");
			return FALSE;
		}
		crt->render_type.render_type[index] = render_type;
		crt->render_type.cust_data[index] = cust_data;
	}
	else {
		/* Evaluate unrecognized value as an error. */
		return FALSE;
	}
	return TRUE;
}
