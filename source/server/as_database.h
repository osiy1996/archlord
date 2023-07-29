#ifndef _AS_DATABASE_H_
#define _AS_DATABASE_H_

#include "core/macros.h"
#include "core/types.h"

#include "task/task.h"

#include "vendor/PostgreSQL/libpq-fe.h"

#include "public/ap_module.h"

#define AS_DATABASE_MODULE_NAME "AgsmDatabase"

#define AS_DATABASE_ENCODE(CODEC, ID, DATA)\
	as_database_encode((CODEC), (ID), &(DATA), sizeof(DATA))

#define AS_DATABASE_DECODE(CODEC, DATA)\
	as_database_read_decoded((CODEC), &(DATA), sizeof(DATA))

/**
 * \brief Maximum number of attached streams.
 */
#define AS_DATABASE_MAX_STREAM_COUNT 8

/**
 * \brief Encode database id.
 * \param[in] MODULE_ID Unique module database id.
 * \param[in] FIELD_ID  Module-local field id.
 *
 * \return Encoded database id.
 */
#define AS_DATABASE_MAKE_ID(MODULE_ID, FIELD_ID) \
	(((uint32_t)(MODULE_ID) << 16) | ((FIELD_ID) & 0xFFFF))

/**
 * \brief Retrieve module id from encoded database id.
 * \param[in] ENCODED_ID Encoded database id.
 * 
 * \return Unique module database id.
 */
#define AS_DATABASE_GET_MODULE_ID(ENCODED_ID) \
	(((ENCODED_ID) >> 16) & 0xFFFF)

/**
 * \brief Retrieve module-local field id from 
 *        encoded database id.
 * \param[in] ENCODED_ID Encoded database id.
 * 
 * \return Module-local field id.
 */
#define AS_DATABASE_GET_FIELD_ID(ENCODED_ID) \
	((ENCODED_ID) & 0xFFFF)

BEGIN_DECLS

enum as_database_callback_id {
	AS_DATABASE_CB_CONNECT,
};

struct as_database_cb_connect {
	PGconn * conn;
};

struct as_database_task_data {
	PGconn * conn;
	void * data;
};

struct as_database_codec {
	void * data;
	void * cursor;
	size_t length;
	struct as_database_codec * next;
};

struct as_database_module * as_database_create_module();

boolean as_database_connect(struct as_database_module * mod);

void as_database_set_blocking(struct as_database_module * mod, boolean is_blocking);

PGconn * as_database_get_conn(struct as_database_module * mod);

void as_database_add_callback(
	struct as_database_module * mod,
	enum as_database_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

void * as_database_add_task(
	struct as_database_module * mod,
	task_work_t work_cb, 
	task_post_t post_cb,
	size_t data_size);

void as_database_free_task(struct as_database_module * mod, struct task_descriptor * task);

void as_database_process(struct as_database_module * mod);

struct as_database_codec * as_database_get_encoder(struct as_database_module * mod);

boolean as_database_encode(
	struct as_database_codec * codec,
	uint32_t id,
	const void * data,
	size_t size);

boolean as_database_encode_timestamp(
	struct as_database_codec * codec,
	uint32_t id,
	time_t timestamp);

boolean as_database_write_encoded(
	struct as_database_codec * codec,
	const void * data,
	size_t size);

size_t as_database_get_encoded_length(
	struct as_database_codec * codec);

struct as_database_codec * as_database_get_decoder(
	struct as_database_module * mod,
	const void * data,
	size_t size);

boolean as_database_decode(
	struct as_database_codec * codec,
	uint32_t * id);

boolean as_database_read_decoded(
	struct as_database_codec * codec,
	void * data,
	size_t size);

boolean as_database_read_timestamp(
	struct as_database_codec * codec,
	time_t * timestamp);

/**
 * Advance database codec.
 * \param [in,out] codec  Database codec pointer.
 * \param [in]     length Number of bytes to advance.
 *
 * \return TRUE if successful. Otherwise FALSE.
 */
boolean as_database_advance_codec(
	struct as_database_codec * codec,
	size_t length);

void as_database_free_codec(struct as_database_module * mod, struct as_database_codec * codec);

END_DECLS

#endif /* _AS_DATABASE_H_ */
