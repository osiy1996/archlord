#include <assert.h>
#include <stdlib.h>

#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_module.h"

void ap_module_init(ap_module_t module_, const char * name)
{
	struct ap_module * mod = module_;
	memset(mod, 0, sizeof(*mod));
	strlcpy(mod->name, name, sizeof(mod->name));
}

void ap_module_set_module_data(
	ap_module_t module_,
	uint32_t index, 
	size_t size,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	struct ap_module * mod = module_;
	assert(index < AP_MODULE_MAX_MODULE_DATA_COUNT);
	mod->data[index].size = size;
	mod->data[index].ctor = constructor;
	mod->data[index].dtor = destructor;
}

size_t ap_module_attach_data(
	ap_module_t module_,
	uint32_t data_index,
	size_t size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	struct ap_module * mod = module_;
	struct ap_module_data * d = &mod->data[data_index];
	struct ap_module_attached_data * ad;
	assert(data_index < AP_MODULE_MAX_MODULE_DATA_COUNT);
	assert(d->attached_data_count < AP_MODULE_MAX_ATTACHED_DATA_COUNT);
	if (d->attached_data_count >= AP_MODULE_MAX_ATTACHED_DATA_COUNT)
		return SIZE_MAX;
	ad = &d->attached_data[d->attached_data_count++];
	ad->offset = d->size;
	d->size += size;
	ad->module_ = callback_module;
	ad->ctor = constructor;
	ad->dtor = destructor;
	return ad->offset;
}

size_t ap_module_get_module_data_size(
	ap_module_t module_, 
	uint32_t data_index)
{
	struct ap_module * mod = module_;
	struct ap_module_data * d = &mod->data[data_index];
	assert(data_index < AP_MODULE_MAX_MODULE_DATA_COUNT);
	return d->size;
}

void * ap_module_create_module_data(
	ap_module_t module_, 
	uint32_t data_index)
{
	struct ap_module * mod = module_;
	struct ap_module_data * d = &mod->data[data_index];
	void * md;
	uint32_t i;
	assert(data_index < AP_MODULE_MAX_MODULE_DATA_COUNT);
	if (!d->size)
		return NULL;
	md = alloc(d->size);
	memset(md, 0, d->size);
	if (d->ctor)
		d->ctor(mod, md);
	for (i = 0; i < d->attached_data_count; i++) {
		struct ap_module_attached_data * ad = 
			&d->attached_data[i];
		if (ad->ctor)
			ad->ctor(ad->module_, md);
	}
	return md;
}

void ap_module_construct_module_data(
	ap_module_t module_, 
	uint32_t data_index,
	void * data)
{
	struct ap_module * mod = module_;
	struct ap_module_data * d = &mod->data[data_index];
	uint32_t i;
	assert(data_index < AP_MODULE_MAX_MODULE_DATA_COUNT);
	memset(data, 0, d->size);
	if (d->ctor)
		d->ctor(mod, data);
	for (i = 0; i < d->attached_data_count; i++) {
		struct ap_module_attached_data * ad = 
			&d->attached_data[i];
		if (ad->ctor)
			ad->ctor(ad->module_, data);
	}
}

void ap_module_destroy_module_data(
	ap_module_t module_, 
	uint32_t data_index,
	void * data)
{
	struct ap_module * mod = module_;
	struct ap_module_data * d = &mod->data[data_index];
	uint32_t i;
	assert(data_index < AP_MODULE_MAX_MODULE_DATA_COUNT);
	for (i = 0; i < d->attached_data_count; i++) {
		struct ap_module_attached_data * ad = 
			&d->attached_data[i];
		if (ad->dtor)
			ad->dtor(ad->module_, data);
	}
	if (d->dtor)
		d->dtor(mod, data);
	dealloc(data);
}

void ap_module_destruct_module_data(
	ap_module_t module_, 
	uint32_t data_index,
	void * data)
{
	struct ap_module * mod = module_;
	struct ap_module_data * d = &mod->data[data_index];
	uint32_t i;
	assert(data_index < AP_MODULE_MAX_MODULE_DATA_COUNT);
	for (i = 0; i < d->attached_data_count; i++) {
		struct ap_module_attached_data * ad = 
			&d->attached_data[i];
		if (ad->dtor)
			ad->dtor(ad->module_, data);
	}
	if (d->dtor)
		d->dtor(mod, data);
}

void ap_module_add_callback(
	ap_module_t module_,
	uint32_t id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	struct ap_module * mod = module_;
	assert(id < AP_MODULE_MAX_CALLBACK_ID);
	assert(mod->callback_count[id] < AP_MODULE_MAX_CALLBACK_COUNT);
	mod->callback_modules[id][mod->callback_count[id]] = callback_module;
	mod->callbacks[id][mod->callback_count[id]++] = callback;
}

boolean ap_module_enum_callback(
	ap_module_t module_,
	uint32_t id,
	void * data)
{
	struct ap_module * mod = module_;
	uint32_t i;
	boolean r = TRUE;
	if (id >= AP_MODULE_MAX_CALLBACK_ID)
		return FALSE;
	for (i = 0; i < mod->callback_count[id]; i++)
		r &= mod->callbacks[id][i](mod->callback_modules[id][i], data);
	return r;
}

struct ap_module_stream * ap_module_stream_create()
{
	struct ap_module_stream * s = alloc(sizeof(*s));
	memset(s, 0, sizeof(*s));
	s->ini_mgr = au_ini_mgr_create();
	s->value_id = UINT32_MAX;
	s->module_name = "ModuleData";
	s->enum_end = "EnumEnd";
	s->module_name_len = strlen(s->module_name);
	s->enum_end_len = strlen(s->enum_end);
	return s;
}

void ap_module_stream_destroy(struct ap_module_stream * stream)
{
	au_ini_mgr_destroy(stream->ini_mgr);
	dealloc(stream);
}

boolean ap_module_stream_open(
	struct ap_module_stream * stream,
	const char * file,
	uint32_t part_index,
	boolean decrypt)
{
	au_ini_mgr_set_path(stream->ini_mgr, file);
	return au_ini_mgr_read_file(stream->ini_mgr, part_index,
		decrypt);
}

boolean ap_module_stream_parse(
	struct ap_module_stream * stream,
	void * data,
	uint32_t data_size,
	boolean decrypt)
{
	return au_ini_mgr_from_memory(
		stream->ini_mgr, data, data_size, decrypt);
}

boolean ap_module_stream_write(
	struct ap_module_stream * stream,
	const char * file,
	uint32_t part_index,
	boolean encrypt)
{
	au_ini_mgr_set_path(stream->ini_mgr, file);
	return au_ini_mgr_write_file(stream->ini_mgr, part_index, 
		encrypt);
}

boolean ap_module_stream_write_to_buffer(
	struct ap_module_stream * stream,
	void * buffer,
	size_t buffer_size,
	uint32_t * written_count,
	boolean encrypt)
{
	return au_ini_mgr_write_to_memory(stream->ini_mgr, buffer, 
		buffer_size, written_count, encrypt);
}

boolean ap_module_stream_set_section_name(
	struct ap_module_stream * stream,
	const char * section_name)
{
	uint32_t c = au_ini_mgr_get_section_count(stream->ini_mgr);
	strlcpy(stream->section_name, section_name,
		sizeof(stream->section_name));
	stream->module_data_index = 0;
	stream->value_id = UINT32_MAX;
	for (uint32_t i = 0; i < c; i++) {
		if (strcmp(au_ini_mgr_get_section_name(stream->ini_mgr, i),
				stream->section_name) == 0) {
			stream->section_id = i;
			return TRUE;
		}
	}
	stream->section_id = au_ini_mgr_add_section(stream->ini_mgr,
		stream->section_name);
	return (stream->section_id != UINT32_MAX);
}

boolean ap_module_stream_write_f32(
	struct ap_module_stream * stream,
	const char * value_name,
	float value)
{
	return au_ini_mgr_set_f32(stream->ini_mgr,
		stream->section_name, value_name, value);
}

boolean ap_module_stream_write_i32(
	struct ap_module_stream * stream,
	const char * value_name,
	int32_t value)
{
	return au_ini_mgr_set_i32(stream->ini_mgr,
		stream->section_name, value_name, value);
}

boolean ap_module_stream_write_i64(
	struct ap_module_stream * stream,
	const char * value_name, 
	int64_t value)
{
	return au_ini_mgr_set_i64(stream->ini_mgr,
		stream->section_name, value_name, value);
}

boolean ap_module_stream_write_value(
	struct ap_module_stream * stream,
	const char * value_name, 
	const char * value)
{
	return au_ini_mgr_set_value(stream->ini_mgr,
		stream->section_name, value_name, value);
}

boolean ap_module_stream_read_next_value(struct ap_module_stream * stream)
{
	uint32_t key_count = au_ini_mgr_get_key_count(
		stream->ini_mgr, stream->section_id);
	if (stream->value_id + 1 >= key_count)
		return FALSE;
	stream->value_name = au_ini_mgr_get_key_name(stream->ini_mgr,
		stream->section_id, stream->value_id + 1);
	if (!stream->value_name)
		return FALSE;
	if (strncmp(stream->value_name, stream->module_name, 
			stream->module_name_len) == 0 ||
		strncmp(stream->value_name, stream->enum_end, 
			stream->enum_end_len) == 0) {
		return FALSE;
	}
	stream->value_id++;
	return TRUE;
}

boolean ap_module_stream_read_prev_value(struct ap_module_stream * stream)
{
	if (stream->value_id == 0)
		return FALSE;
	stream->value_name = au_ini_mgr_get_key_name(stream->ini_mgr,
		stream->section_id, stream->value_id - 1);
	if (!stream->value_name)
		return FALSE;
	if (strncmp(stream->value_name, stream->module_name, 
			stream->module_name_len) == 0 ||
		strncmp(stream->value_name, stream->enum_end, 
			stream->enum_end_len) == 0) {
		return FALSE;
	}
	stream->value_id--;
	return TRUE;
}

const char * ap_module_stream_get_value(struct ap_module_stream * stream)
{
	return au_ini_mgr_get_value(stream->ini_mgr, 
		stream->section_id, stream->value_id);
}

boolean ap_module_stream_get_f32(
	struct ap_module_stream * stream,
	float * value)
{
	const char * str = ap_module_stream_get_value(stream);
	if (!str)
		return FALSE;
	*value = strtof(str, NULL);
	return TRUE;
}

boolean ap_module_stream_get_i32(
	struct ap_module_stream * stream,
	int32_t * value)
{
	const char * str = ap_module_stream_get_value(stream);
	if (!str)
		return FALSE;
	*value = strtol(str, NULL, 10);
	return TRUE;
}

boolean ap_module_stream_get_i64(
	struct ap_module_stream * stream,
	int64_t * value)
{
	const char * str = ap_module_stream_get_value(stream);
	if (!str)
		return FALSE;
	*value = strtoll(str, NULL, 10);
	return TRUE;
}

boolean ap_module_stream_get_u32(
	struct ap_module_stream * stream,
	uint32_t * value)
{
	const char * str = ap_module_stream_get_value(stream);
	if (!str)
		return FALSE;
	*value = strtoul(str, NULL, 10);
	return TRUE;
}

boolean ap_module_stream_get_u64(
	struct ap_module_stream * stream,
	uint64_t * value)
{
	const char * str = ap_module_stream_get_value(stream);
	if (!str)
		return FALSE;
	*value = strtoull(str, NULL, 10);
	return TRUE;
}

boolean ap_module_stream_get_str(
	struct ap_module_stream * stream,
	char * value,
	size_t maxcount)
{
	const char * str = ap_module_stream_get_value(stream);
	if (!str)
		return FALSE;
	strlcpy(value, str, maxcount);
	return TRUE;
}

const char * ap_module_stream_get_value_name(struct ap_module_stream * stream)
{
	return stream->value_name;
}

boolean ap_module_stream_enum_write(
	ap_module_t module_,
	struct ap_module_stream * stream,
	uint16_t data_index,
	void * data)
{
	struct ap_module * mod = module_;
	char value_name[64];
	struct ap_module_stream_data * stream_data;
	if (data_index >= AP_MODULE_MAX_MODULE_DATA_COUNT)
		return FALSE;
	stream_data = mod->data[data_index].stream_data;
	while (stream_data) {
		size_t len = snprintf(value_name, sizeof(value_name), 
			"%s%u",
			stream->module_name, ++stream->module_data_index);
		assert(len < 64);
		if (!ap_module_stream_write_value(stream, value_name, 
				stream_data->module_name) ||
			(stream_data->write_cb && 
				!stream_data->write_cb(stream_data->callback_module, data, stream))) {
			return FALSE;
		}
		stream_data = stream_data->next;
	}
	if (!ap_module_stream_write_value(stream, stream->enum_end, "0"))
		return FALSE;
	return TRUE;
}

boolean ap_module_stream_enum_read(
	ap_module_t module_,
	struct ap_module_stream * stream,
	uint16_t data_index, 
	void * data)
{
	struct ap_module * mod = module_;
	uint32_t key_count = au_ini_mgr_get_key_count(
		stream->ini_mgr, stream->section_id);
	const char * value_name;
	const char * value;
	struct ap_module_stream_data * cur;
	if (data_index >= AP_MODULE_MAX_MODULE_DATA_COUNT)
		return FALSE;
	for (uint32_t i = stream->value_id + 1; i < key_count; i++) {
		value_name = au_ini_mgr_get_key_name(
			stream->ini_mgr, stream->section_id, i);
		if (!value_name)
			return FALSE;
		if (strncmp(value_name, stream->module_name, stream->module_name_len) == 0) {
			value = au_ini_mgr_get_value(stream->ini_mgr, 
				stream->section_id, i);
			cur = mod->data[data_index].stream_data;
			while (cur) {
				if (strcmp(value, cur->module_name) == 0) {
					stream->value_id = i;
					if (cur->read_cb && !cur->read_cb(cur->callback_module, data, stream))
						return FALSE;
					break;
				}
				cur = cur->next;
			}
		}
		else if (strncmp(value_name, stream->enum_end, stream->enum_end_len) == 0) {
			stream->value_id = i;
			return TRUE;
		}
	}
	return TRUE;
}

const char * ap_module_stream_read_section_name(
	struct ap_module_stream * stream,
	uint32_t section_id)
{
	if (section_id >= ap_module_stream_get_section_count(stream))
		return NULL;
	stream->section_id = section_id;
	strlcpy(stream->section_name,
		au_ini_mgr_get_section_name(stream->ini_mgr, section_id), 
		sizeof(stream->section_name));
	stream->value_id = UINT32_MAX;
	return stream->section_name;
}

uint32_t ap_module_stream_get_section_count(struct ap_module_stream * stream)
{
	return au_ini_mgr_get_section_count(stream->ini_mgr);
}

boolean ap_module_stream_set_mode(
	struct ap_module_stream * stream,
	enum au_ini_mgr_mode mode)
{
	return au_ini_mgr_set_mode(stream->ini_mgr, mode);
}

boolean ap_module_stream_set_type(
	struct ap_module_stream * stream,
	enum au_ini_mgr_type type)
{
	return au_ini_mgr_set_type(stream->ini_mgr, type);
}

struct ap_module_stream_data * ap_module_stream_alloc_data()
{
	struct ap_module_stream_data * d = alloc(sizeof(*d));
	memset(d, 0, sizeof(*d));
	return d;
}

void ap_module_stream_destroy_data(struct ap_module_stream_data * data)
{
	dealloc(data);
}

void ap_module_stream_add_callback(
	ap_module_t module_,
	uint32_t data_index,
	const char * module_name,
	ap_module_t callback_module,
	ap_module_stream_read_t read_cb,
	ap_module_stream_write_t write_cb)
{
	struct ap_module * mod = module_;
	struct ap_module_stream_data * d;
	struct ap_module_stream_data * cur;
	struct ap_module_stream_data * last = NULL;
	assert(data_index < AP_MODULE_MAX_MODULE_DATA_COUNT);
	d = ap_module_stream_alloc_data();
	strlcpy(d->module_name, module_name, sizeof(d->module_name));
	d->callback_module = callback_module;
	d->read_cb = read_cb;
	d->write_cb = write_cb;
	d->next = NULL;
	cur = mod->data[data_index].stream_data;
	while (cur) {
		last = cur;
		cur = cur->next;
	}
	if (last)
		last->next = d;
	else
		mod->data[data_index].stream_data = d;
}
