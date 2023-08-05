#include "client/ac_lod.h"

#include "core/log.h"
#include "core/malloc.h"

#include <assert.h>
#include <stdlib.h>

#define STREAM_LOD_MAX_DISTANCE "LOD_MAX_DISTANCE"
#define STREAM_LOD_DISTANCE_TYPE "LOD_TYPE_DISTANCE"
#define STREAM_LOD_LEVEL "LOD_LEVEL"
#define STREAM_LOD_DISTANCE "LOD_DISTANCE"
#define STREAM_LOD_BOUNDARY "LOD_BOUNDARY"
#define STREAM_LOD_BILLBOARD_NUM "LOD_BILLBOARD_NUM"
#define STREAM_LOD_BILLBOARD_INFO "LOD_BILLBOARD_INFO"
#define STREAM_LOD_BILLBOARD_INFO_LENGTH 18

struct ac_lod_module {
	struct ap_module_instance instance;
};

static boolean onregister(
	struct ac_lod_module * mod,
	struct ap_module_registry * registry)
{
	return TRUE;
}

static void onshutdown(struct ac_lod_module * mod)
{
}

struct ac_lod_module * ac_lod_create_module()
{
	struct ac_lod_module * mod = ap_module_instance_new(AC_LOD_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	return mod;
}

enum ac_lod_stream_read_result ac_lod_stream_read(
	struct ac_lod_module * mod,
	struct ap_module_stream * stream,
	struct ac_lod * lod)
{
	const char * value_name = ap_module_stream_get_value_name(stream);
	const char * value = ap_module_stream_get_value(stream);
	int index = -1;
	int tmp1 = 0;
	int tmp2 = 0;
	struct ac_lod_data * data;
	if (strcmp(value_name, STREAM_LOD_LEVEL) == 0) {
		if (sscanf(value, "%d:%d", &index, &tmp1) != 2) {
			ERROR("Failed to read lod level.");
			return AC_LOD_STREAM_READ_RESULT_ERROR;
		}
		data = ac_lod_get_data(lod, index, TRUE);
		data->max_lod_level = tmp1;
		return AC_LOD_STREAM_READ_RESULT_READ;
	}
	else if (strcmp(value_name, STREAM_LOD_BILLBOARD_NUM) == 0) {
		if (sscanf(value, "%d:%d", &index, &tmp1) != 2) {
			ERROR("Failed to read billboard count.");
			return AC_LOD_STREAM_READ_RESULT_ERROR;
		}
		data = ac_lod_get_data(lod, index, TRUE);
		data->bill_count = tmp1;
		return AC_LOD_STREAM_READ_RESULT_READ;
	}
	else if (strncmp(value_name, STREAM_LOD_BILLBOARD_INFO, STREAM_LOD_BILLBOARD_INFO_LENGTH) == 0) {
		if (sscanf(value, "%d:%d:%d", &index, &tmp1, &tmp2) != 3) {
			ERROR("Failed to read billboard info.");
			return AC_LOD_STREAM_READ_RESULT_ERROR;
		}
		if (tmp2 >= AC_LOD_MAX_NUM) {
			ERROR("Invalid billboard info index.");
			return AC_LOD_STREAM_READ_RESULT_ERROR;
		}
		data = ac_lod_get_data(lod, index, TRUE);
		data->bill_info[tmp1] = tmp2;
		return AC_LOD_STREAM_READ_RESULT_READ;
	}
	else if (strcmp(value_name, STREAM_LOD_BOUNDARY) == 0) {
		if (sscanf(value, "%d:%d", &index, &tmp1) != 2) {
			ERROR("Failed to read lod boundary.");
			return AC_LOD_STREAM_READ_RESULT_ERROR;
		}
		data = ac_lod_get_data(lod, index, TRUE);
		data->boundary = tmp1;
		return AC_LOD_STREAM_READ_RESULT_READ;
	}
	else if (strcmp(value_name, STREAM_LOD_DISTANCE_TYPE) == 0) {
		if (sscanf(value, "%d", &tmp1) != 1) {
			ERROR("Failed to read lod distance type.");
			return AC_LOD_STREAM_READ_RESULT_ERROR;
		}
		lod->distance_type = tmp1;
		return AC_LOD_STREAM_READ_RESULT_READ;
	}
	else if (strcmp(value_name, STREAM_LOD_MAX_DISTANCE) == 0) {
		if (sscanf(value, "%d:%d", &index, &tmp1) != 2) {
			ERROR("Failed to read lod max distance.");
			return AC_LOD_STREAM_READ_RESULT_ERROR;
		}
		data = ac_lod_get_data(lod, index, TRUE);
		data->max_distance_ratio = tmp1;
		return AC_LOD_STREAM_READ_RESULT_READ;
	}
	return AC_LOD_STREAM_READ_RESULT_PASS;
}

boolean ac_lod_stream_write(
	struct ac_lod_module * mod,
	struct ap_module_stream * stream,
	struct ac_lod * lod)
{
	boolean result = TRUE;
	struct ac_lod_list * list = lod->list;
	char buffer[256];
	while (list) {
		struct ac_lod_data * data = &list->data;
		if (data->max_lod_level) {
			snprintf(buffer, sizeof(buffer), "%d:%d", data->index, data->max_lod_level);
			result &= ap_module_stream_write_value(stream, STREAM_LOD_LEVEL, buffer);
			if (data->bill_count > 0) {
				int i;
				snprintf(buffer, sizeof(buffer), "%d:%d", data->index, data->bill_count);
				result &= ap_module_stream_write_value(stream, STREAM_LOD_BILLBOARD_NUM, buffer);
				for (i = 0; i < data->bill_count; i++) {
					if (data->bill_info[i]) {
						char valuename[128];
						snprintf(valuename, sizeof(valuename), "%s%d", STREAM_LOD_BILLBOARD_INFO, i);
						snprintf(buffer, sizeof(buffer), "%d:%d:%d", data->index, i, data->bill_info[i]);
						result &= ap_module_stream_write_value(stream, valuename, buffer);
					}
				}
			}
			snprintf(buffer, sizeof(buffer), "%d:%d", data->index, data->boundary);
			result &= ap_module_stream_write_value(stream, STREAM_LOD_BOUNDARY, buffer);
			snprintf(buffer, sizeof(buffer), "%d:%d", data->index, data->max_distance_ratio);
			result &= ap_module_stream_write_value(stream, STREAM_LOD_MAX_DISTANCE, buffer);
		}
		list = list->next;
	}
	if (lod->distance_type) {
		snprintf(buffer, sizeof(buffer), "%d", lod->distance_type);
		result &= ap_module_stream_write_value(stream, STREAM_LOD_DISTANCE_TYPE, buffer);
	}
	return result;
}

static struct ac_lod_data * getdata(struct ac_lod * lod, int index)
{
	struct ac_lod_list * list = lod->list;
	while (list) {
		if (list->data.index == index)
			return &list->data;
		list = list->next;
	}
	return NULL;
}

static struct ac_lod_data * adddata(struct ac_lod * lod)
{
	struct ac_lod_list * link = alloc(sizeof(*link));
	memset(link, 0, sizeof(*link));
	link->data.index = lod->count++;
	link->data.lod = lod;
	if (lod->list) {
		struct ac_lod_list * last = lod->list;
		while (last->next)
			last = last->next;
		last->next = link;
	}
	else {
		lod->list = link;
	}
	return &link->data;
}

struct ac_lod_data * ac_lod_get_data(struct ac_lod * lod, int index, boolean add)
{
	struct ac_lod_data * data = getdata(lod, index);
	if (!data && add) {
		data = adddata(lod);
		assert(data->index == index);
		data->index = index;
	}
	return data;
}
