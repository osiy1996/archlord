#include "server/as_account.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_admin.h"
#include "public/ap_module.h"
#include "public/ap_tick.h"

#include "server/as_database.h"

#include <assert.h>
#include <time.h>

#define STMT_INSERT "INSERT_ACCOUNT"
#define STMT_SELECT_LIST "SELECT_ACCOUNT_LIST"
#define STMT_SELECT "SELECT_ACCOUNT"
#define STMT_UPDATE "UPDATE_ACCOUNT"

#define MAX_USER_DATA_SIZE ((size_t)1u << 14)

struct load_task {
	struct as_account_module * mod;
	ap_module_t callback_module;
	char account_id[AP_LOGIN_MAX_ID_LENGTH + 1];
	struct as_account * account;
};

struct load_callback {
	as_account_deferred_t cb;
	void * user_data;

	struct load_callback * next;
};

struct update_entry {
	char account_id[AP_LOGIN_MAX_ID_LENGTH + 1];
	struct as_database_codec * codec;
	boolean linked;

	struct update_entry * next;
};

struct update_task {
	struct as_account_module * mod;
	uint64_t tick;
	struct update_entry * entries;
};

struct as_account_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_tick_module * ap_tick;
	struct as_character_module * as_character;
	struct as_database_module * as_database;
	uint32_t stream_module_id[AS_DATABASE_MAX_STREAM_COUNT];
	uint32_t stream_count;
	struct ap_admin account_admin;
	struct ap_admin account_id_admin;
	struct ap_admin load_admin;
	struct load_callback * callback_freelist;
	struct update_entry * entry_freelist;
};

static struct load_callback * getcallback(
	struct as_account_module * mod)
{
	struct load_callback * c = mod->callback_freelist;
	if (c) {
		mod->callback_freelist = c->next;
	}
	else {
		c = alloc(sizeof(*c) + MAX_USER_DATA_SIZE);
		c->user_data = (void *)((uintptr_t)c + sizeof(*c));
	}
	c->cb = NULL;
	c->next = NULL;
	return c;
}

static void freecallback(
	struct as_account_module * mod,
	struct load_callback * c)
{
	c->next = mod->callback_freelist;
	mod->callback_freelist = c;
}

static struct update_entry * getentry(
	struct as_account_module * mod)
{
	struct update_entry * e = mod->entry_freelist;
	if (e)
		mod->entry_freelist = e->next;
	else
		e = alloc(sizeof(*e));
	memset(e, 0, sizeof(*e));
	return e;
}

static void freeentry(
	struct as_account_module * mod,
	struct update_entry * e)
{
	if (e->codec) {
		as_database_free_codec(mod->as_database, e->codec);
		e->codec = NULL;
	}
	e->next = mod->entry_freelist;
	mod->entry_freelist = e;
}

static boolean create_statements(PGconn * conn)
{
	PGresult * res;
	res = PQprepare(conn, STMT_INSERT, 
		"INSERT INTO accounts VALUES ($1,$2);", 2, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	res = PQprepare(conn, STMT_SELECT_LIST, 
		"SELECT data FROM accounts;", 0, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	res = PQprepare(conn, STMT_SELECT, 
		"SELECT data FROM accounts WHERE account_id=$1;", 1, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	res = PQprepare(conn, STMT_UPDATE, 
		"UPDATE accounts SET data=$2 WHERE account_id=$1;", 2, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	return TRUE;
}

static boolean decode_account(
	struct as_account_module * mod,
	struct as_account * account,
	const void * data,
	size_t length)
{
	struct as_account_cb_decode cb = { 0 };
	uint32_t id;
	boolean result = TRUE;
	struct as_database_codec * codec = as_database_get_decoder(mod->as_database, data, length);
	if (!codec)
		return FALSE;
	cb.account = account;
	cb.codec = codec;
	while (as_database_decode(codec, &id)) {
		switch (id) {
		case AS_ACCOUNT_DB_ACCOUNT_ID:
			result &= as_database_read_decoded(codec, 
				account->account_id, sizeof(account->account_id));
			break;
		case AS_ACCOUNT_DB_PW_SALT:
			result &= as_database_read_decoded(codec, 
				account->pw_salt, sizeof(account->pw_salt));
			break;
		case AS_ACCOUNT_DB_PW_HASH:
			result &= as_database_read_decoded(codec, 
				account->pw_hash, sizeof(account->pw_hash));
			break;
		case AS_ACCOUNT_DB_EMAIL:
			result &= as_database_read_decoded(codec, 
				account->email, sizeof(account->email));
			break;
		case AS_ACCOUNT_DB_CREATION_DATE:
			result &= AS_DATABASE_DECODE(codec, 
				account->creation_date);
			break;
		case AS_ACCOUNT_DB_FLAGS:
			result &= AS_DATABASE_DECODE(codec, 
				account->flags);
			break;
		case AS_ACCOUNT_DB_EXTRA_BANK_SLOTS:
			result &= AS_DATABASE_DECODE(codec, 
				account->extra_bank_slots);
			break;
		case AS_ACCOUNT_DB_BANK_GOLD:
			result &= AS_DATABASE_DECODE(codec, 
				account->bank_gold);
			break;
		case AS_ACCOUNT_DB_CHANTRA_COINS:
			result &= AS_DATABASE_DECODE(codec, 
				account->chantra_coins);
			break;
		case AS_ACCOUNT_DB_CHARACTER: {
			uint32_t index;
			if (account->character_count >= AS_ACCOUNT_MAX_CHARACTER) {
				ERROR("Exceed max. number of characters per account (%s).",
					account->account_id);
				assert(0);
				as_database_free_codec(mod->as_database, codec);
				return FALSE;
			}
			index = account->character_count;
			account->characters[index] = as_character_decode(mod->as_character, codec);
			if (!account->characters[index]) {
				ERROR("Failed to decode character in account (%s).",
					account->account_id);
				assert(0);
				as_database_free_codec(mod->as_database, codec);
				return FALSE;
			}
			account->character_count++;
			break;
		}
		default:
			cb.module_id = AS_DATABASE_GET_MODULE_ID(id);
			cb.field_id = AS_DATABASE_GET_FIELD_ID(id);
			if (ap_module_enum_callback(mod, AS_ACCOUNT_CB_DECODE, &cb)) {
				ERROR("Invalid decoded id (%d).", id);
				assert(0);
				as_database_free_codec(mod->as_database, codec);
				return FALSE;
			}
			break;
		}
	}
	as_database_free_codec(mod->as_database, codec);
	return result;
}

static struct as_database_codec * encode_account(
	struct as_account_module * mod,
	const struct as_account * account)
{
	struct as_account_cb_encode cb = { 0 };
	boolean result = TRUE;
	struct as_database_codec * codec = as_database_get_encoder(mod->as_database);
	uint32_t i;
	if (!codec)
		return NULL;
	result &= as_database_encode(codec, AS_ACCOUNT_DB_ACCOUNT_ID, 
		account->account_id, sizeof(account->account_id));
	result &= as_database_encode(codec, AS_ACCOUNT_DB_PW_SALT, 
		account->pw_salt, sizeof(account->pw_salt));
	result &= as_database_encode(codec, AS_ACCOUNT_DB_PW_HASH, 
		account->pw_hash, sizeof(account->pw_hash));
	result &= as_database_encode(codec, AS_ACCOUNT_DB_EMAIL, 
		account->email, sizeof(account->email));
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ACCOUNT_DB_CREATION_DATE, account->creation_date);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ACCOUNT_DB_FLAGS, account->flags);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ACCOUNT_DB_EXTRA_BANK_SLOTS, account->extra_bank_slots);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ACCOUNT_DB_BANK_GOLD, account->bank_gold);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ACCOUNT_DB_CHANTRA_COINS, account->chantra_coins);
	for (i = 0; i < account->character_count; i++) {
		result &= as_database_encode(codec, AS_ACCOUNT_DB_CHARACTER, NULL, 0);
		result &= as_character_encode(mod->as_character, codec, account->characters[i]);
	}
	cb.account = account;
	cb.codec = codec;
	result &= ap_module_enum_callback(mod, AS_ACCOUNT_CB_ENCODE, &cb);
	if (result)
		return codec;
	as_database_free_codec(mod->as_database, codec);
	return NULL;
}

static boolean cbdatabaseconnect(struct as_account_module * mod, void * data)
{
	struct as_database_cb_connect * d = data;
	if (!create_statements(d->conn)) {
		ERROR("Failed to create account database statements.");
		return FALSE;
	}
	return TRUE;
}

static struct as_account * getcached(
	struct as_account_module * mod,
	const char * account_id)
{
	struct as_account ** object = 
		ap_admin_get_object_by_name(&mod->account_admin, 
			account_id);
	if (!object)
		return NULL;
	return *object;
}

static void executecallbacks(
	struct as_account_module * mod,
	struct load_task * t)
{
	struct load_callback ** obj = ap_admin_get_object_by_name(
		&mod->load_admin, t->account_id);
	struct load_callback * c;
	if (!obj) {
		ERROR("[UNREACHABLE] No callbacks for deferred load (%s).",
			t->account_id);
		assert(0);
		return;
	}
	c = *obj;
	while (c) {
		struct load_callback * next = c->next;
		c->cb(t->callback_module, t->account_id, t->account, c->user_data);
		freecallback(mod, c);
		c = next;
	}
	ap_admin_remove_object_by_name(&mod->load_admin, t->account_id);
}

static boolean task_load(struct as_database_task_data * task)
{
	struct load_task * d = task->data;
	d->account = as_account_load_from_db(d->mod, task->conn, 
		d->account_id);
	return (d->account != NULL);
}

static void task_load_post(
	struct task_descriptor * task,
	struct as_database_task_data * tdata, 
	boolean result)
{
	struct load_task * d = tdata->data;
	struct as_account * cached;
	if (!d->account) {
		/* Account does not exist or we failed to load it. */
		executecallbacks(d->mod, d);
		as_database_free_task(d->mod->as_database, task);
		return;
	}
	d->account->last_commit = ap_tick_get(d->mod->ap_tick);
	cached = as_account_load_from_cache(d->mod, d->account_id, FALSE);
	if (cached) {
		ERROR("[UNREACHABLE] Account is already in cache (%s).",
			d->account_id);
		assert(0);
		as_account_free(d->mod, d->account);
		d->account = cached;
	}
	else if (!as_account_cache(d->mod, d->account, FALSE)) {
		/* This code should be unreachable. */
		assert(0);
		as_account_free(d->mod, d->account);
		d->account = NULL;
		executecallbacks(d->mod, d);
		as_database_free_task(d->mod->as_database, task);
		return;
	}
	executecallbacks(d->mod, d);
	as_database_free_task(d->mod->as_database, task);
}

static boolean task_update(
	struct as_database_task_data * task)
{
	struct update_task * d = task->data;
	PGresult * res;
	struct update_entry * e = d->entries;
	res = PQexec(task->conn, "BEGIN");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		res = PQexec(task->conn, "ROLLBACK");
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	while (e) {
		const char * values[2] = { e->account_id, e->codec->data };
		int lengths[2] = { 
			(int)strlen(e->account_id), 
			(int)as_database_get_encoded_length(e->codec) };
		const int formats[2] = { 0, 1 };
		res = PQexecPrepared(task->conn, STMT_UPDATE, 
			2, values, lengths, formats, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			PQclear(res);
			res = PQexec(task->conn, "ROLLBACK");
			PQclear(res);
			return FALSE;
		}
		PQclear(res);
		e = e->next;
	}
	res = PQexec(task->conn, "END");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		res = PQexec(task->conn, "ROLLBACK");
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	return TRUE;
}

static void syncaftersuccess(
	struct as_account_module * mod,
	struct as_account * account, 
	struct update_task * t,
	struct update_entry * e)
{
	uint32_t rc;
	account->committing = FALSE;
	account->last_commit = t->tick;
	assert(account->refcount != 0);
	rc = account->refcount--;
	INFO("Updated account (%s).", account->account_id);
	if (rc <= 1 && account->unloading) {
		struct as_account ** object = 
			ap_admin_get_object_by_name(&mod->account_admin, 
				account->account_id);
		struct as_account * cached;
		if (!object) {
			assert(0);
			return;
		}
		cached = *object;
		assert(cached == account);
		if (!ap_admin_remove_object_by_name(&mod->account_admin,
				account->account_id)) {
			assert(0);
			return;
		}
		as_account_free(mod, account);
	}
}

static void syncafterfail(
	struct as_account * account, 
	struct update_entry * e)
{
	account->committing = FALSE;
	account->unloading = FALSE;
	if (e->linked)
		account->commit_linked;
	as_account_release(account);
}

static void task_update_post(
	struct task_descriptor * task,
	struct as_database_task_data * tdata, 
	boolean result)
{
	struct update_task * d = tdata->data;
	struct as_account_module * mod = d->mod;
	struct update_entry * e = d->entries;
	if (result) {
		while (e) {
			struct update_entry * next = e->next;
			struct as_account * account = 
				getcached(mod, e->account_id);
			if (account) {
				syncaftersuccess(mod, account, d, e);
			}
			else {
				/* Unreachable. We do not completely release 
				 * accounts until they are commited to DB. */
				ERROR("[UNREACHABLE] Failed to retrieve account after commit (%s).",
					e->account_id);
				assert(0);
			}
			freeentry(mod, e);
			e = next;
		}
	}
	else {
		while (e) {
			struct update_entry * next = e->next;
			struct as_account * account = 
				getcached(mod, e->account_id);
			if (account) {
				syncafterfail(account, e);
			}
			else {
				/* Unreachable. We do not completely release 
				 * accounts until they are commited to DB. */
				ERROR("[UNREACHABLE] Failed to retrieve account in commit (%s).",
					e->account_id);
				assert(0);
			}
			freeentry(mod, e);
			e = next;
		}
	}
	as_database_free_task(mod->as_database, task);
}

static struct update_entry * process_commit(
	struct as_account_module * mod,
	struct update_entry * list,
	struct as_account * account,
	uint64_t tick,
	boolean force)
{
	struct as_database_codec * codec;
	struct update_entry * e;
	uint32_t rc;
	boolean commit_now;
	boolean commit_relaxed;
	struct as_account_cb_pre_commit cb = { account };
	if (account->committing) {
		/* We force commits when server is shutting down,
		 * in which case we will have waited for previous 
		 * commits to have been completed. */
		assert(force == FALSE);
		return list;
	}
	rc = account->refcount;
	commit_now = force || account->commit_linked;
	/* What this translates to is that either commit interval 
	 * has arrived or account can be released, at the same time 
	 * commit list should be empty. */
	commit_relaxed = 
		(tick >= account->last_commit + 300000 || !rc) && 
			!list;
	if (!commit_now && !commit_relaxed)
		return list;
	ap_module_enum_callback(mod, AS_ACCOUNT_CB_PRE_COMMIT, &cb);
	codec = encode_account(mod, account);
	if (!codec) {
		ERROR("Failed to encode account (%s).", 
			account->account_id);
		return list;
	}
	e = getentry(mod);
	strlcpy(e->account_id, account->account_id, 
		sizeof(e->account_id));
	e->codec = codec;
	e->linked = account->commit_linked;
	e->next = list;
	account->commit_linked = FALSE;
	account->committing = TRUE;
	if (!rc)
		account->unloading = TRUE;
	as_account_reference(account);
	return e;
}

static boolean onregister(
	struct as_account_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_database, AS_DATABASE_MODULE_NAME);
	as_database_add_callback(mod->as_database, AS_DATABASE_CB_CONNECT, 
		mod, cbdatabaseconnect);
	return TRUE;
}

static void onclose(struct as_account_module * mod)
{
	as_account_commit(mod, TRUE);
}

static void onshutdown(struct as_account_module * mod)
{
	const char * account_id;
	size_t index = 0;
	struct as_account ** object = NULL;
	if (!mod)
		return;
	account_id = ap_admin_iterate_name(&mod->account_admin, 
		&index, (void **)&object);
	while (account_id) {
		struct as_account * account = *object;
		if (account->refcount) {
			WARN("Account refcount not 0 at shutdown (%s).",
				account_id);
		}
		account_id = ap_admin_iterate_name(&mod->account_admin, 
			&index, (void **)&object);
	}
	ap_admin_destroy(&mod->load_admin);
	ap_admin_destroy(&mod->account_admin);
	ap_admin_destroy(&mod->account_id_admin);
}

struct as_account_module * as_account_create_module()
{
	struct as_account_module * mod = ap_module_instance_new(AS_ACCOUNT_MODULE_NAME,
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	ap_module_set_module_data(mod, AS_ACCOUNT_MDI_ACCOUNT, sizeof(struct as_account),
		NULL, NULL);
	ap_admin_init(&mod->account_admin, 
		sizeof(struct as_account *), 1024);
	ap_admin_init(&mod->account_id_admin, 
		sizeof(boolean), 1024);
	ap_admin_init(&mod->load_admin, 
		sizeof(struct load_callback *), 16);
	return mod;
}

struct as_account * as_account_new(struct as_account_module * mod)
{
	struct as_account * account = ap_module_create_module_data(mod, 
		AS_ACCOUNT_MDI_ACCOUNT);
	return account;
}

void as_account_free(struct as_account_module * mod, struct as_account * account)
{
	uint32_t i;
	for (i = 0; i < account->character_count; i++) {
		as_character_free_db(mod->as_character, account->characters[i]);
		account->characters[i] = NULL;
	}
	account->character_count = 0;
	ap_module_destroy_module_data(mod, AS_ACCOUNT_MDI_ACCOUNT, account);
}

boolean as_account_validate_account_id(const char * account_id)
{
	size_t len = strlen(account_id);
	size_t i;
	if (len < 4 || len > AP_LOGIN_MAX_ID_LENGTH)
		return FALSE;
	for (i = 0; i < len; i++) {
		char c = account_id[i];
		boolean is_numeric;
		boolean is_alph;
		is_numeric = (c >= '0' && c <= '9');
		is_alph = (c >= 'a' && c <= 'z');
		if (!is_numeric && !is_alph)
			return FALSE;
	}
	return TRUE;
}

void as_account_add_callback(
	struct as_account_module * mod,
	enum as_account_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t as_account_attach_data(
	struct as_account_module * mod,
	enum as_account_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

boolean as_account_set_database_stream(
	struct as_account_module * mod,
	uint32_t module_id,
	ap_module_t callback_module,
	ap_module_default_t cb_decode,
	ap_module_default_t cb_encode)
{
	uint32_t i;
	assert(cb_decode != NULL && cb_encode != NULL);
	if (mod->stream_count >= AS_DATABASE_MAX_STREAM_COUNT)
		return FALSE;
	for (i = 0; i < mod->stream_count; i++) {
		if (mod->stream_module_id[i] == module_id) {
			ERROR("Module database id is not unique (%u).",
				module_id);
			return FALSE;
		}
	}
	as_account_add_callback(mod, AS_ACCOUNT_CB_DECODE, callback_module, cb_decode);
	as_account_add_callback(mod, AS_ACCOUNT_CB_ENCODE, callback_module, cb_encode);
	mod->stream_module_id[mod->stream_count++] = module_id;
	return TRUE;
}

boolean as_account_preprocess(struct as_account_module * mod)
{
	PGresult * res;
	time_t ct;
	int row_count;
	int i;
	PGconn * conn = as_database_get_conn(mod->as_database);
	if (!conn) {
		ERROR("Failed to retrieve database connection.");
		return FALSE;
	}
	res = PQexec(conn, "BEGIN");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	res = PQexecPrepared(conn, STMT_SELECT_LIST, 0, 0, 0, 0, 1);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		PQclear(res);
		return FALSE;
	}
	ct = time(0);
	row_count = PQntuples(res);
	for (i = 0; i < row_count; i++) {
		struct as_account * account = as_account_new(mod);
		void * data = PQgetvalue(res, i, 0);
		int len = PQgetlength(res, i, 0);
		uint32_t j;
		if (!data || 
			len <= 0 || 
			!decode_account(mod, account, data, (size_t)len)) {
			ERROR("Failed to decode account.");
			as_account_free(mod, account);
			return FALSE;
		}
		INFO("Preprocessing account (%s).", account->account_id);
		if (!as_account_cache_id(mod, account->account_id)) {
			ERROR("Failed to cache account id (%s).", 
				account->account_id);
			PQclear(res);
			as_account_free(mod, account);
			return FALSE;
		}
		for (j = 0; j < account->character_count; j++) {
			struct as_character_db * c = account->characters[j];
			struct as_account_cb_preprocess_char cb = {
				account, c };
			if (!as_character_reserve_name(mod->as_character, c->name, 
					account->account_id)) {
				ERROR("Failed to reserve character name (%s).",
					c->name);
				PQclear(res);
				as_account_free(mod, account);
				return FALSE;
			}
			if (!ap_character_get_template(mod->ap_character, c->tid)) {
				ERROR("Character with invalid tid (name = %s, tid = %u).",
					c->name, c->tid);
				PQclear(res);
				as_account_free(mod, account);
				return FALSE;
			}
			if (!ap_module_enum_callback(mod, AS_ACCOUNT_CB_PREPROCESS_CHARACTER, 
					&cb)) {
				ERROR("Failed to preprocess database character (name = %s).",
					c->name);
				PQclear(res);
				as_account_free(mod, account);
				return FALSE;
			}
		}
		as_account_free(mod, account);
	}
	PQclear(res);
	res = PQexec(conn, "END");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	if (!ap_module_enum_callback(mod, AS_ACCOUNT_CB_PREPROCESS_COMPLETE, NULL)) {
		ERROR("Account complete preprocess callback failed.");
		return FALSE;
	}
	return TRUE;
}

struct as_account * as_account_load_from_db(
	struct as_account_module * mod,
	PGconn * conn, 
	const char * account_id)
{
	PGresult * res;
	const char * values[1] = { account_id };
	const int lengths[1] = { (int)strlen(account_id) };
	const int formats[1] = { 0 };
	void * data = 0;
	struct as_account * account = NULL;
	int len;
	res = PQexecPrepared(conn, STMT_SELECT, 
		1, values, lengths, formats, 1);
	if (PQresultStatus(res) != PGRES_TUPLES_OK || 
		PQntuples(res) != 1) {
		PQclear(res);
		return NULL;
	}
	data = (void *)PQgetvalue(res, 0, 0);
	len = PQgetlength(res, 0, 0);
	if (!data || len <= 0) {
		PQclear(res);
		return NULL;
	}
	account = as_account_new(mod);
	if (!decode_account(mod, account, data, (size_t)len)) {
		PQclear(res);
		dealloc(account);
		return NULL;
	}
	PQclear(res);
	account->refcount = 0;
	return account;
}

struct as_account * as_account_load_from_cache(
	struct as_account_module * mod,
	const char * account_id,
	boolean is_reference_held)
{
	struct as_account ** object = 
		ap_admin_get_object_by_name(&mod->account_admin, 
			account_id);
	if (!object)
		return NULL;
	if (is_reference_held)
		(*object)->refcount++;
	(*object)->unloading = FALSE;
	return *object;
}

void * as_account_load_deferred(
	struct as_account_module * mod,
	const char * account_id,
	ap_module_t callback_module,
	as_account_deferred_t callback,
	size_t user_data_size)
{
	struct load_callback ** obj = ap_admin_get_object_by_name(
		&mod->load_admin, account_id);
	assert(user_data_size <= MAX_USER_DATA_SIZE);
	if (obj) {
		/* There's already a task to load this account. */
		struct load_callback * nc = getcallback(mod);
		nc->cb = callback;
		nc->next = *obj;
		*obj = nc;
		return nc->user_data;
	}
	else {
		struct load_task * task = as_database_add_task(mod->as_database, task_load, 
			task_load_post, sizeof(*task));
		struct load_callback * c = getcallback(mod);
		c->cb = callback;
		obj = ap_admin_add_object_by_name(&mod->load_admin, 
			account_id);
		*obj = c;
		memset(task, 0, sizeof(*task));
		task->mod = mod;
		task->callback_module = callback_module;
		strlcpy(task->account_id, account_id, 
			sizeof(task->account_id));
		return c->user_data;
	}
}

boolean as_account_create_in_db(
	struct as_account_module * mod,
	PGconn  * conn,
	struct as_account * account)
{
	struct as_database_codec * codec = encode_account(mod, account);
	PGresult * res;
	const char * values[2] = { account->account_id, NULL };
	int lengths[2] = { (int)strlen(account->account_id), 0 };
	const int formats[2] = { 0, 1 };
	if (!codec) {
		ERROR("Failed to encode account.");
		return FALSE;
	}
	res = PQexec(conn, "BEGIN");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		res = PQexec(conn, "ROLLBACK");
		PQclear(res);
		as_database_free_codec(mod->as_database, codec);
		return FALSE;
	}
	PQclear(res);
	values[1] = (const char *)codec->data;
	lengths[1] = (int)as_database_get_encoded_length(codec);
	res = PQexecPrepared(conn, STMT_INSERT, 
		2, values, lengths, formats, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		res = PQexec(conn, "ROLLBACK");
		PQclear(res);
		as_database_free_codec(mod->as_database, codec);
		return FALSE;
	}
	PQclear(res);
	as_database_free_codec(mod->as_database, codec);
	res = PQexec(conn, "END");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		res = PQexec(conn, "ROLLBACK");
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	return TRUE;
}

boolean as_account_cache_id(struct as_account_module * mod, const char * account_id)
{
	return (ap_admin_add_object_by_name(&mod->account_id_admin,
		account_id) != NULL);
}

boolean as_account_cache(
	struct as_account_module * mod,
	struct as_account * account, 
	boolean reference)
{
	struct as_account ** object = 
		ap_admin_add_object_by_name(&mod->account_admin,
			account->account_id);
	if (!object)
		return FALSE;
	assert(account->refcount == 0);
	if (reference)
		account->refcount = 1;
	*object = account;
	return TRUE;
}

void as_account_reference(struct as_account * account)
{
	account->refcount++;
}

struct as_account * as_account_copy_detached(
	struct as_account_module * mod,
	struct as_account * account)
{
	struct as_account_cb_copy cb = { 0 };
	struct as_account * copy = as_account_new(mod);
	uint32_t i;
	memcpy(copy->account_id, account->account_id, sizeof(copy->account_id));
	memcpy(copy->pw_salt, account->pw_salt, sizeof(copy->pw_salt));
	memcpy(copy->pw_hash, account->pw_hash, sizeof(copy->pw_hash));
	memcpy(copy->email, account->email, sizeof(copy->email));
	copy->creation_date = account->creation_date;
	copy->flags = account->flags;
	copy->extra_bank_slots = account->extra_bank_slots;
	copy->bank_gold = account->bank_gold;
	copy->chantra_coins = account->chantra_coins;
	copy->character_count = account->character_count;
	for (i = 0; i < account->character_count; i++) {
		copy->characters[i] = as_character_copy_database(mod->as_character, 
			account->characters[i]);
	}
	cb.src = account;
	cb.dst = copy;
	ap_module_enum_callback(mod, AS_ACCOUNT_CB_COPY, &cb);
	return copy;
}

void as_account_release(struct as_account * account)
{
	assert(account->refcount != 0);
	account->refcount--;
}

void as_account_commit(struct as_account_module * mod, boolean force)
{
	size_t index = 0;
	struct as_account ** object = NULL;
	uint64_t tick = ap_tick_get(mod->ap_tick);
	struct update_entry * list = NULL;
	if (force) {
		as_database_process(mod->as_database);
		task_wait_all();
	}
	while (ap_admin_iterate_name(&mod->account_admin, &index, 
			(void **)&object)) {
		struct as_account * account = *object;
		list = process_commit(mod, list, account, tick, force);
	}
	if (list) {
		struct update_task * task;
		task = as_database_add_task(mod->as_database, task_update, 
			task_update_post, sizeof(*task));
		memset(task, 0, sizeof(*task));
		task->mod = mod;
		task->tick = tick;
		task->entries = list;
		if (force) {
			as_database_process(mod->as_database);
			task_wait_all();
		}
	}
}
