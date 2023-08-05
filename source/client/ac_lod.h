#ifndef _AC_LOD_H_
#define _AC_LOD_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module_instance.h"

#define AC_LOD_MODULE_NAME "AgcmLOD"

#define AC_LOD_MAX_NUM 5

BEGIN_DECLS

enum ac_lod_stream_read_result {
	AC_LOD_STREAM_READ_RESULT_ERROR,
	AC_LOD_STREAM_READ_RESULT_PASS,
	AC_LOD_STREAM_READ_RESULT_READ,
};

struct ac_lod_data {
	int index;
	int bill_count;
	int bill_info[AC_LOD_MAX_NUM];
	uint32_t max_lod_level;
	uint32_t lod_distance[AC_LOD_MAX_NUM];
	uint32_t boundary;
	uint32_t max_distance_ratio;
	struct ac_lod * lod;
};

struct ac_lod_list {
	struct ac_lod_data data;
	struct ac_lod_list * next;
};

struct ac_lod {
	int count;
	int distance_type;
	struct ac_lod_list * list;
};

struct ac_lod_module * ac_lod_create_module();

enum ac_lod_stream_read_result ac_lod_stream_read(
	struct ac_lod_module * mod,
	struct ap_module_stream * stream,
	struct ac_lod * lod);

boolean ac_lod_stream_write(
	struct ac_lod_module * mod,
	struct ap_module_stream * stream,
	struct ac_lod * lod);

struct ac_lod_data * ac_lod_get_data(struct ac_lod * lod, int index, boolean add);

END_DECLS

#endif /* _AC_LOD_H_ */
