#include <assert.h>

#include "core/log.h"
#include "core/malloc.h"
#include "core/os.h"
#include "core/vector.h"

#include "public/ap_config.h"

#include "server/as_database.h"

#define CODEC_SIZE ((size_t)1u << 24)
#define TASK_BUFFER_SIZE ((size_t)1u << 20)

struct as_database_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	boolean is_blocking;
	PGconn * conn;
	struct task_descriptor * free_tasklist;
	struct task_descriptor * task_queue;
	uint32_t active_task_count;
	struct as_database_codec * free_codecs;
	struct as_database_codec * free_codecs_thread;
	uint64_t main_thread_id;
};

static struct as_database_codec * getcodec(struct as_database_module * mod)
{
	uint64_t threadid = get_current_thread_id();
	struct as_database_codec * e = NULL;
	if (threadid == mod->main_thread_id) {
		e = mod->free_codecs;
		if (e) {
			mod->free_codecs = e->next;
			e->cursor = e->data;
			e->next = NULL;
		}
	}
	else {
		e = mod->free_codecs_thread;
		if (e) {
			mod->free_codecs_thread = e->next;
			e->cursor = e->data;
			e->next = NULL;
		}
	}
	return e;
}

static boolean onregister(
	struct as_database_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	return TRUE;
}

static void onshutdown(struct as_database_module * mod)
{
	struct task_descriptor * task;
	struct as_database_codec * codec;
	if (!mod)
		return;
	if (mod->conn) {
		PQfinish(mod->conn);
		mod->conn = NULL;
	}
	task = mod->task_queue;
	while (task) {
		struct task_descriptor * next = task->next;
		dealloc(task);
		task = next;
	}
	task = mod->free_tasklist;
	while (task) {
		struct task_descriptor * next = task->next;
		dealloc(task);
		task = next;
	}
	codec = mod->free_codecs;
	while (codec) {
		struct as_database_codec * next = codec->next;
		dealloc(codec);
		codec = next;
	}
	codec = mod->free_codecs_thread;
	while (codec) {
		struct as_database_codec * next = codec->next;
		dealloc(codec);
		codec = next;
	}
}

struct as_database_module * as_database_create_module()
{
	struct as_database_module * mod = ap_module_instance_new(AS_DATABASE_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	mod->is_blocking = TRUE;
	mod->main_thread_id = get_current_thread_id();
	return mod;
}

boolean as_database_connect(struct as_database_module * mod)
{
	char conn_info[256];
	struct as_database_cb_connect cb = { 0 };
	const char * db = ap_config_get(mod->ap_config, "DBName");
	const char * user = ap_config_get(mod->ap_config, "DBUser");
	const char * pwd = ap_config_get(mod->ap_config, "DBPassword");
	if (!db || !user || !pwd) {
		ERROR("Failed to retrieve database configuration.");
		return FALSE;
	}
	snprintf(conn_info, sizeof(conn_info), 
		"dbname=%s user=%s password=%s", db, user, pwd);
	mod->conn = PQconnectdb(conn_info);
	if (PQstatus(mod->conn) != CONNECTION_OK)
		return FALSE;
	cb.conn = mod->conn;
	return ap_module_enum_callback(mod, AS_DATABASE_CB_CONNECT, &cb);
}

void as_database_set_blocking(struct as_database_module * mod, boolean is_blocking)
{
	mod->is_blocking = is_blocking;
}

PGconn * as_database_get_conn(struct as_database_module * mod)
{
	if (!mod->is_blocking) {
		ERROR("Database connection can only be accessed by tasks in non-blocking mode.");
		assert(0);
		return NULL;
	}
	return mod->conn;
}

void as_database_add_callback(
	struct as_database_module * mod,
	enum as_database_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

void * as_database_add_task(
	struct as_database_module * mod,
	task_work_t work_cb, 
	task_post_t post_cb,
	size_t data_size)
{
	struct task_descriptor * task = mod->free_tasklist;
	struct task_descriptor * last = NULL;
	struct task_descriptor * cur = NULL;
	struct as_database_task_data * tdata;
	size_t size = data_size + sizeof(struct as_database_task_data);
	if (mod->is_blocking) {
		ERROR("Database tasks are not accessible in blocking mode.");
		assert(0);
		return NULL;
	}
	if (size > TASK_BUFFER_SIZE)
		return NULL;
	if (task) {
		mod->free_tasklist = task->next;
	}
	else {
		task = alloc(sizeof(*task) + TASK_BUFFER_SIZE);
		memset(task, 0, sizeof(*task));
		task->data = (void *)((uintptr_t)task + sizeof(*task));
	}
	tdata = task->data;
	tdata->conn = mod->conn;
	tdata->data = (void *)((uintptr_t)tdata + sizeof(*tdata));
	task->work_cb = work_cb;
	task->post_cb = post_cb;
	task->next = NULL;
	cur = mod->task_queue;
	while (cur) {
		last = cur;
		cur = cur->next;
	}
	if (last)
		last->next = task;
	else
		mod->task_queue = task;
	return tdata->data;
}

void as_database_free_task(struct as_database_module * mod, struct task_descriptor * task)
{
	assert(mod->active_task_count > 0);
	task->next = mod->free_tasklist;
	mod->free_tasklist = task->next;
	mod->active_task_count--;
}

void as_database_process(struct as_database_module * mod)
{
	if (!mod->active_task_count && mod->task_queue) {
		uint32_t count = 0;
		struct task_descriptor * task = mod->task_queue;
		while (task) {
			count++;
			task = task->next;
		}
		task_add_list(mod->task_queue, TRUE);
		mod->task_queue = NULL;
		mod->active_task_count = count;
	}
}

struct as_database_codec * as_database_get_encoder(struct as_database_module * mod)
{
	struct as_database_codec * e = getcodec(mod);
	if (!e) {
		e = alloc(sizeof(*e) + CODEC_SIZE);
		memset(e, 0, sizeof(*e));
		e->data = (void *)((uintptr_t)e + sizeof(*e));
		e->cursor = e->data;
	}
	e->length = CODEC_SIZE;
	return e;
}

boolean as_database_encode(
	struct as_database_codec * codec,
	uint32_t id,
	const void * data,
	size_t size)
{
	uintptr_t len = (uintptr_t)codec->cursor - 
		(uintptr_t)codec->data;
	if (len + size + sizeof(id) > CODEC_SIZE) {
		WARN("Database codec buffer overflow (encode).");
		return FALSE;
	}
	memcpy(codec->cursor, &id, sizeof(id));
	memcpy((void *)((uintptr_t)codec->cursor + sizeof(id)),
		data, size);
	codec->cursor = (void *)((uintptr_t)codec->cursor + 
		sizeof(id) + size);
	return TRUE;
}

boolean as_database_encode_timestamp(
	struct as_database_codec * codec,
	uint32_t id,
	time_t timestamp)
{
	uint64_t u = (uint64_t)timestamp;
	return AS_DATABASE_ENCODE(codec, id, u);
}

boolean as_database_write_encoded(
	struct as_database_codec * codec,
	const void * data,
	size_t size)
{
	uintptr_t len = (uintptr_t)codec->cursor - 
		(uintptr_t)codec->data;
	if (len + size > CODEC_SIZE) {
		WARN("Database codec buffer overflow (encode).");
		return FALSE;
	}
	memcpy(codec->cursor, data, size);
	codec->cursor = (void *)((uintptr_t)codec->cursor + size);
	return TRUE;
}

size_t as_database_get_encoded_length(
	struct as_database_codec * codec)
{
	return (size_t)((uintptr_t)codec->cursor - 
		(uintptr_t)codec->data);
}

struct as_database_codec * as_database_get_decoder(
	struct as_database_module * mod,
	const void * data,
	size_t size)
{
	struct as_database_codec * e = getcodec(mod);
	if (size > CODEC_SIZE) {
		WARN("Data size exceeds codec limits.");
		return NULL;
	}
	if (!e) {
		e = alloc(sizeof(*e) + CODEC_SIZE);
		memset(e, 0, sizeof(*e));
		e->data = (void *)((uintptr_t)e + sizeof(*e));
		e->cursor = e->data;
	}
	memcpy(e->data, data, size);
	e->length = size;
	return e;
}

boolean as_database_decode(
	struct as_database_codec * codec,
	uint32_t * id)
{
	uintptr_t cur = (uintptr_t)codec->cursor - 
		(uintptr_t)codec->data;
	if (cur + sizeof(*id) > codec->length)
		return FALSE;
	memcpy(id, codec->cursor, sizeof(*id));
	codec->cursor = (void *)((uintptr_t)codec->cursor + 
		sizeof(*id));
	return TRUE;
}

boolean as_database_read_decoded(
	struct as_database_codec * codec,
	void * data,
	size_t size)
{
	uintptr_t cur = (uintptr_t)codec->cursor - 
		(uintptr_t)codec->data;
	if (cur + size > codec->length) {
		WARN("Database codec buffer overflow (decode data).");
		return FALSE;
	}
	memcpy(data, codec->cursor, size);
	codec->cursor = (void *)((uintptr_t)codec->cursor + size);
	return TRUE;
}

boolean as_database_read_timestamp(
	struct as_database_codec * codec,
	time_t * timestamp)
{
	uint64_t u;
	if (!AS_DATABASE_DECODE(codec, u))
		return FALSE;
	*timestamp = (time_t)u;
	return TRUE;
}

boolean as_database_advance_codec(
	struct as_database_codec * codec,
	size_t length)
{
	uintptr_t cur = (uintptr_t)codec->cursor - 
		(uintptr_t)codec->data;
	if (cur + length > codec->length) {
		WARN("Database codec buffer overflow (advance).");
		return FALSE;
	}
	codec->cursor = (void *)((uintptr_t)codec->cursor + length);
	return TRUE;
}

void as_database_free_codec(struct as_database_module * mod, struct as_database_codec * codec)
{
	uint64_t threadid = get_current_thread_id();
	if (threadid == mod->main_thread_id) {
		codec->next = mod->free_codecs;
		mod->free_codecs = codec;
	}
	else {
		codec->next = mod->free_codecs_thread;
		mod->free_codecs_thread = codec;
	}
}
