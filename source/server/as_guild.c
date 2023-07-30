#include "server/as_guild.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_guild.h"
#include "public/ap_tick.h"

#include "server/as_account.h"
#include "server/as_character.h"
#include "server/as_database.h"
#include "server/as_player.h"

#include <assert.h>

#define STMT_INSERT "INSERT_GUILD"
#define STMT_SELECT_LIST "SELECT_GUILD_LIST"
#define STMT_SELECT "SELECT_GUILD"
#define STMT_UPDATE "UPDATE_GUILD"
#define STMT_DELETE "DELETE_GUILD"

#define DB_STREAM_MODULE_ID 2
#define MAKE_ID(ID) AS_DATABASE_MAKE_ID(DB_STREAM_MODULE_ID, ID)

#define COMMIT_PER_HOUR 2
#define COMMIT_INTERVAL_MS (3600000 / COMMIT_PER_HOUR)

enum deferred_op {
	DEFERRED_CREATE,
	DEFERRED_UPDATE,
	DEFERRED_DELETE
};

struct deferred_entry {
	char guild_id[AP_GUILD_MAX_ID_LENGTH + 1];
	enum deferred_op operation;
	struct as_database_codec * codec;

	struct deferred_entry * next;
};

struct deferred_task {
	struct as_guild_module * mod;
	uint64_t tick;
	struct deferred_entry * entries;
};

struct pending_delete {
	char guild_id[AP_GUILD_MAX_ID_LENGTH + 1];
};

struct as_guild_module {
	struct ap_module_instance instance;
	struct ap_guild_module * ap_guild;
	struct ap_tick_module * ap_tick;
	struct as_account_module * as_account;
	struct as_character_module * as_character;
	struct as_database_module * as_database;
	struct as_player_module * as_player;
	size_t guild_offset;
	size_t character_db_offset;
	struct deferred_entry * entry_freelist;
	struct pending_delete * pending_delete;
	struct pending_delete * committing_delete;
	boolean committing;
	uint64_t last_commit_tick;
};

static struct deferred_entry * getentry(
	struct as_guild_module * mod)
{
	struct deferred_entry * e = mod->entry_freelist;
	if (e)
		mod->entry_freelist = e->next;
	else
		e = alloc(sizeof(*e));
	memset(e, 0, sizeof(*e));
	return e;
}

static void freeentry(
	struct as_guild_module * mod,
	struct deferred_entry * e)
{
	if (e->codec) {
		as_database_free_codec(mod->as_database, e->codec);
		e->codec = NULL;
	}
	e->next = mod->entry_freelist;
	mod->entry_freelist = e;
}

static boolean createstatements(PGconn * conn)
{
	PGresult * res;
	res = PQprepare(conn, STMT_INSERT, 
		"INSERT INTO guilds VALUES ($1,$2);", 2, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	res = PQprepare(conn, STMT_SELECT_LIST, 
		"SELECT data FROM guilds;", 0, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	res = PQprepare(conn, STMT_SELECT, 
		"SELECT data FROM guilds WHERE guild_id=$1;", 1, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	res = PQprepare(conn, STMT_UPDATE, 
		"UPDATE guilds SET data=$2 WHERE guild_id=$1;", 2, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	res = PQprepare(conn, STMT_DELETE, 
		"DELETE FROM guilds WHERE guild_id=$1;", 1, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	return TRUE;
}

struct as_guild_db * decodeguild(
	struct as_database_codec * codec)
{
	struct as_guild_db * g = alloc(sizeof(*g));
	uint32_t id;
	boolean result = TRUE;
	boolean end = FALSE;
	memset(g, 0, sizeof(*g));
	while (!end && as_database_decode(codec, &id)) {
		switch (id) {
		case AS_GUILD_DB_END:
			end = TRUE;
			break;
		case AS_GUILD_DB_ID:
			result &= as_database_read_decoded(codec, 
				g->id, sizeof(g->id));
			break;
		case AS_GUILD_DB_PASSWORD:
			result &= as_database_read_decoded(codec, 
				g->password, sizeof(g->password));
			break;
		case AS_GUILD_DB_NOTICE:
			result &= as_database_read_decoded(codec, 
				g->notice, sizeof(g->notice));
			break;
		case AS_GUILD_DB_RANK:
			result &= AS_DATABASE_DECODE(codec, 
				g->rank);
			break;
		case AS_GUILD_DB_CREATION_DATE:
			result &= AS_DATABASE_DECODE(codec, 
				g->creation_date);
			break;
		case AS_GUILD_DB_MAX_MEMBER_COUNT:
			result &= AS_DATABASE_DECODE(codec, 
				g->max_member_count);
			break;
		case AS_GUILD_DB_UNION_ID:
			result &= AS_DATABASE_DECODE(codec, 
				g->union_id);
			break;
		case AS_GUILD_DB_GUILD_MARK_TID:
			result &= AS_DATABASE_DECODE(codec, 
				g->guild_mark_tid);
			break;
		case AS_GUILD_DB_GUILD_MARK_COLOR:
			result &= AS_DATABASE_DECODE(codec, 
				g->guild_mark_color);
			break;
		case AS_GUILD_DB_TOTAL_BATTLE_POINT:
			result &= AS_DATABASE_DECODE(codec, 
				g->total_battle_point);
			break;
		default:
			ERROR("Invalid decoded id (%d).", id);
			dealloc(g);
			assert(0);
			return NULL;
		}
	}
	if (!result) {
		dealloc(g);
		assert(0);
		return NULL;
	}
	return g;
}

static struct as_database_codec * encodeguild(
	struct as_guild_module * mod,
	const struct as_guild_db * guild)
{
	boolean result = TRUE;
	struct as_database_codec * codec = as_database_get_encoder(mod->as_database);
	result &= as_database_encode(codec, AS_GUILD_DB_ID, 
		guild->id, sizeof(guild->id));
	result &= as_database_encode(codec, AS_GUILD_DB_PASSWORD, 
		guild->password, sizeof(guild->password));
	result &= as_database_encode(codec, AS_GUILD_DB_NOTICE, 
		guild->notice, sizeof(guild->notice));
	result &= AS_DATABASE_ENCODE(codec, 
		AS_GUILD_DB_RANK, guild->rank);
	result &= as_database_encode_timestamp(codec, 
		AS_GUILD_DB_CREATION_DATE, guild->creation_date);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_GUILD_DB_MAX_MEMBER_COUNT, guild->max_member_count);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_GUILD_DB_UNION_ID, guild->union_id);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_GUILD_DB_GUILD_MARK_TID, guild->guild_mark_tid);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_GUILD_DB_GUILD_MARK_COLOR, guild->guild_mark_color);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_GUILD_DB_TOTAL_BATTLE_POINT, guild->total_battle_point);
	result &= as_database_encode(codec, 
		AS_GUILD_DB_END, NULL, 0);
	if (result)
		return codec;
	as_database_free_codec(mod->as_database, codec);
	return NULL;
}

static boolean cbdecodechar(
	struct as_guild_module * mod, 
	struct as_character_cb_decode * cb)
{
	boolean result = TRUE;
	struct as_guild_character_db * db = 
		as_guild_get_character_db(mod, cb->character);
	if (cb->module_id != DB_STREAM_MODULE_ID)
		return TRUE;
	switch (cb->field_id) {
	case AS_GUILD_CHARACTER_DB_GUILD_ID:
		result &= as_database_read_decoded(cb->codec, 
			db->guild_id, sizeof(db->guild_id));
		break;
	case AS_GUILD_CHARACTER_DB_RANK:
		result &= AS_DATABASE_DECODE(cb->codec, db->rank);
		break;
	case AS_GUILD_CHARACTER_DB_JOIN_DATE:
		result &= as_database_read_timestamp(cb->codec, 
			&db->join_date);
		break;
	default:
		assert(0);
		return TRUE;
	}
	return !result;
}

static boolean cbencodechar(
	struct as_guild_module * mod, 
	struct as_character_cb_encode * cb)
{
	boolean result = TRUE;
	struct as_guild_character_db * db = 
		as_guild_get_character_db(mod, cb->character);
	result &= as_database_encode(cb->codec, 
		MAKE_ID(AS_GUILD_CHARACTER_DB_GUILD_ID),
		db->guild_id, sizeof(db->guild_id));
	result &= AS_DATABASE_ENCODE(cb->codec, 
		MAKE_ID(AS_GUILD_CHARACTER_DB_RANK), db->rank);
	result &= as_database_encode_timestamp(cb->codec, 
		MAKE_ID(AS_GUILD_CHARACTER_DB_JOIN_DATE), db->join_date);
	return result;
}

static boolean cbcharload(
	struct as_guild_module * mod, 
	struct as_character_cb_load * cb)
{
	struct ap_guild_character * gc = 
		ap_guild_get_character(mod->ap_guild, cb->character);
	struct ap_guild * g = 
		ap_guild_get_character_guild(mod->ap_guild, cb->character->name);
	gc->guild = g;
	if (!g)
		return TRUE;
	gc->member = ap_guild_get_member(mod->ap_guild, g, cb->character->name);
	strlcpy(gc->guild_id, g->id, sizeof(gc->guild_id));
	gc->guild_mark_tid = g->guild_mark_tid;
	gc->guild_mark_color = g->guild_mark_color;
	gc->battle_rank = g->battle_rank;
	return TRUE;
}

static boolean cbreflectchar(
	struct as_guild_module * mod, 
	struct as_character_cb_reflect * cb)
{
	struct ap_guild_character * gchar = 
		ap_guild_get_character(mod->ap_guild, cb->character);
	struct as_guild_character_db * db = 
		as_guild_get_character_db(mod, cb->db);
	strlcpy(db->guild_id, gchar->guild_id, sizeof(db->guild_id));
	if (gchar->member) {
		db->rank = gchar->member->rank;
		db->join_date = gchar->member->join_date;
	}
	return TRUE;
}

static boolean cbcharcopy(
	struct as_guild_module * mod, 
	struct as_character_cb_copy * cb)
{
	struct as_guild_character_db * src = as_guild_get_character_db(mod, cb->src);
	struct as_guild_character_db * dst = as_guild_get_character_db(mod, cb->dst);
	memcpy(src, dst, sizeof(*src));
	return TRUE;
}

static boolean cbpreprocesschar(
	struct as_guild_module * mod,
	struct as_account_cb_preprocess_char * cb)
{
	struct as_guild_character_db * db = 
		as_guild_get_character_db(mod, cb->character);
	struct ap_guild * g;
	struct ap_guild_member * m;
	if (strisempty(db->guild_id))
		return TRUE;
	g = ap_guild_get(mod->ap_guild, db->guild_id);
	if (!g) {
		ERROR("Character guild does not exist (name = %s, guild_id = %s).",
			cb->character->name, db->guild_id);
		return FALSE;
	}
	m = ap_guild_add_member(mod->ap_guild, g, cb->character->name);
	if (!m) {
		ERROR("Failed to add guild member (name = %s, guild_id = %s).",
			cb->character->name, db->guild_id);
		return FALSE;
	}
	m->rank = db->rank;
	m->join_date = db->join_date;
	m->level = cb->character->level;
	m->tid = cb->character->tid;
	m->status = AP_GUILD_MEMBER_STATUS_OFFLINE;
	switch (m->rank) {
	case AP_GUILD_MEMBER_RANK_MASTER:
		if (!strisempty(g->master_name)) {
			assert(0);
			ERROR("Guild has more than one master (%s).", g->id);
			return FALSE;
		}
		strlcpy(g->master_name, m->name, sizeof(g->master_name));
		break;
	case AP_GUILD_MEMBER_RANK_SUBMASTER:
		if (!strisempty(g->sub_master_name)) {
			assert(0);
			ERROR("Guild has more than one sub-master (%s).", 
				g->id);
			return FALSE;
		}
		strlcpy(g->sub_master_name, m->name, 
			sizeof(g->sub_master_name));
		break;
	}
	return TRUE;
}

static boolean cbplayeradd(
	struct as_guild_module * mod, 
	struct as_player_cb_add * cb)
{
	struct ap_guild_character * gc = 
		ap_guild_get_character(mod->ap_guild, cb->character);
	struct ap_guild * g = gc->guild;
	if (g) {
		assert(gc->member != NULL);
		gc->member->status = AP_GUILD_MEMBER_STATUS_ONLINE;
		ap_guild_make_char_data_packet(mod->ap_guild, g, gc->member, 
			cb->character->id);
		as_guild_send_packet(mod, g);
	}
	return TRUE;
}

static boolean cbplayerremove(
	struct as_guild_module * mod, 
	struct as_player_cb_remove * cb)
{
	struct ap_guild_character * gc = 
		ap_guild_get_character(mod->ap_guild, cb->character);
	struct ap_guild * g = gc->guild;
	if (g) {
		assert(gc->member != NULL);
		gc->member->status = AP_GUILD_MEMBER_STATUS_OFFLINE;
		ap_guild_make_char_data_packet(mod->ap_guild, g, gc->member, 
			cb->character->id);
		as_guild_send_packet(mod, g);
	}
	return TRUE;
}

static boolean preprocessguilds(
	struct as_guild_module * mod, 
	PGconn * conn)
{
	PGresult * res;
	time_t ct;
	int row_count;
	int i;
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
		struct ap_guild * guild;
		struct as_guild_db * db;
		void * data = PQgetvalue(res, i, 0);
		int len = PQgetlength(res, i, 0);
		struct as_database_codec * codec;
		if (!data || len <= 0) {
			ERROR("Empty guild database blob.");
			PQclear(res);
			return FALSE;
		}
		codec = as_database_get_decoder(mod->as_database, data, (size_t)len);
		if (!codec) {
			ERROR("Failed to create codec.");
			PQclear(res);
			return FALSE;
		}
		db = decodeguild(codec);
		as_database_free_codec(mod->as_database, codec);
		if (!db) {
			ERROR("Failed to decode guild.");
			PQclear(res);
			return FALSE;
		}
		INFO("Preprocessing guild (%s).", db->id);
		guild = ap_guild_add(mod->ap_guild, db->id);
		if (!guild) {
			ERROR("Failed to add guild (%s).", db->id);
			dealloc(db);
			PQclear(res);
			return FALSE;
		}
		strlcpy(guild->password, db->password, 
			sizeof(guild->password));
		strlcpy(guild->notice, db->notice, sizeof(guild->notice));
		guild->rank = db->rank;
		guild->creation_date = db->creation_date;
		guild->max_member_count = db->max_member_count;
		guild->union_id = db->union_id;
		guild->guild_mark_tid = db->guild_mark_tid;
		guild->guild_mark_color = db->guild_mark_color;
		guild->total_battle_point = db->total_battle_point;
		as_guild_get(mod, guild)->db = db;
	}
	PQclear(res);
	res = PQexec(conn, "END");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	return TRUE;
}

static boolean cbdatabaseconnect(struct as_guild_module * mod, void * data)
{
	struct as_database_cb_connect * d = data;
	if (!createstatements(d->conn)) {
		ERROR("Failed to create guild database statements.");
		return FALSE;
	}
	if (!preprocessguilds(mod, d->conn)) {
		ERROR("Failed to preprocess guilds.");
		return FALSE;
	}
	return TRUE;
}

static boolean taskdeferred(
	struct as_database_task_data * task)
{
	struct deferred_task * d = task->data;
	PGresult * res;
	struct deferred_entry * e = d->entries;
	res = PQexec(task->conn, "BEGIN");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		PQclear(res);
		res = PQexec(task->conn, "ROLLBACK");
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	while (e) {
		const char * values[2] = { e->guild_id };
		int lengths[2] = { (int)strlen(e->guild_id) };
		const int formats[2] = { 0, 1 };
		switch (e->operation) {
		case DEFERRED_CREATE:
			values[1] = e->codec->data;
			lengths[1] = 
				(int)as_database_get_encoded_length(e->codec);
			res = PQexecPrepared(task->conn, STMT_INSERT, 
				2, values, lengths, formats, 0);
			break;
		case DEFERRED_UPDATE:
			values[1] = e->codec->data;
			lengths[1] = 
				(int)as_database_get_encoded_length(e->codec);
			res = PQexecPrepared(task->conn, STMT_UPDATE, 
				2, values, lengths, formats, 0);
			break;
		case DEFERRED_DELETE:
			res = PQexecPrepared(task->conn, STMT_DELETE, 
				1, values, lengths, formats, 0);
			break;
		default:
			res = PQexec(task->conn, "ROLLBACK");
			PQclear(res);
			return FALSE;
		}
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

static void taskdeferredpost(
	struct task_descriptor * task,
	struct as_database_task_data * tdata, 
	boolean result)
{
	struct deferred_task * d = tdata->data;
	struct as_guild_module * mod = d->mod;
	struct deferred_entry * e = d->entries;
	if (!result) {
		uint32_t i;
		uint32_t count = vec_count(mod->committing_delete);
		ERROR("Deferred database task failed.");
		for (i = 0; i < count; i++) {
			vec_push_back(&mod->pending_delete, 
				&mod->committing_delete[i]);
		}
		while (e) {
			struct deferred_entry * next = e->next;
			freeentry(mod, e);
			e = next;
		}
	}
	else {
		while (e) {
			struct deferred_entry * next = e->next;
			struct ap_guild * g = ap_guild_get(mod->ap_guild, e->guild_id);
			if (g) {
				struct as_guild * gs = as_guild_get(mod, g);
				if (gs->await_create) {
					assert(e->operation == DEFERRED_CREATE);
					gs->await_create = FALSE;
				}
			}
			freeentry(mod, e);
			e = next;
		}
	}
	as_database_free_task(mod->as_database, task);
	vec_clear(mod->committing_delete);
	mod->committing = FALSE;
}

static struct deferred_entry * newdeferreddelete(
	struct as_guild_module * mod,
	struct deferred_entry * list,
	const struct pending_delete * pending)
{
	struct deferred_entry * e = getentry(mod);
	strlcpy(e->guild_id, pending->guild_id, sizeof(e->guild_id));
	e->operation = DEFERRED_DELETE;
	e->codec = NULL;
	e->next = list;
	return e;
}

static struct deferred_entry * newdeferred(
	struct as_guild_module * mod,
	struct deferred_entry * list,
	struct ap_guild * guild)
{
	struct as_database_codec * codec;
	struct deferred_entry * e;
	struct as_guild * g = as_guild_get(mod, guild);
	struct as_guild_db * db = g->db;
	if (!db)
		return list;
	as_guild_reflect(mod, guild);
	codec = encodeguild(mod, db);
	if (!codec) {
		ERROR("Failed to encode guild (%s).", guild->id);
		return list;
	}
	e = getentry(mod);
	strlcpy(e->guild_id, guild->id, sizeof(e->guild_id));
	if (g->await_create)
		e->operation = DEFERRED_CREATE;
	else 
		e->operation = DEFERRED_UPDATE;
	e->codec = codec;
	e->next = list;
	return e;
}

static boolean onregister(
	struct as_guild_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_guild, AP_GUILD_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_account, AS_ACCOUNT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_database, AS_DATABASE_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	mod->character_db_offset = as_character_attach_data(mod->as_character,
		AS_CHARACTER_MDI_DATABASE, sizeof(struct as_guild_character_db), 
		mod, NULL, NULL);
	if (mod->character_db_offset == SIZE_MAX) {
		ERROR("Failed to attach character database data.");
		return FALSE;
	}
	mod->guild_offset = ap_guild_attach_data(mod->ap_guild,
		AP_GUILD_MDI_GUILD, sizeof(struct as_guild), mod, NULL, NULL);
	if (mod->guild_offset == SIZE_MAX) {
		ERROR("Failed to attach guild data.");
		return FALSE;
	}
	if (!as_character_set_database_stream(mod->as_character, DB_STREAM_MODULE_ID,
			mod, cbdecodechar, cbencodechar)) {
		ERROR("Failed to set character database stream.");
		return FALSE;
	}
	as_character_add_callback(mod->as_character, AS_CHARACTER_CB_LOAD, mod, cbcharload);
	as_character_add_callback(mod->as_character, AS_CHARACTER_CB_REFLECT, mod, cbreflectchar);
	as_character_add_callback(mod->as_character, AS_CHARACTER_CB_COPY, mod, cbcharcopy);
	as_account_add_callback(mod->as_account, AS_ACCOUNT_CB_PREPROCESS_CHARACTER, mod, cbpreprocesschar);
	as_player_add_callback(mod->as_player, AS_PLAYER_CB_ADD, mod, cbplayeradd);
	as_player_add_callback(mod->as_player, AS_PLAYER_CB_REMOVE, mod, cbplayerremove);
	as_database_add_callback(mod->as_database, AS_DATABASE_CB_CONNECT, 
		mod, cbdatabaseconnect);
	return TRUE;
}

static void onclose(struct as_guild_module * mod)
{
	as_guild_commit(mod, TRUE);
}

struct as_guild_module * as_guild_create_module()
{
	struct as_guild_module * mod = ap_module_instance_new(AS_GUILD_MODULE_NAME,
		sizeof(*mod), onregister, NULL, onclose, NULL);
	mod->pending_delete = vec_new(sizeof(struct pending_delete));
	mod->committing_delete = vec_new(sizeof(struct pending_delete));
	return mod;
}

void as_guild_add_callback(
	struct as_guild_module * mod,
	enum as_guild_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t as_guild_attach_data(
	struct as_guild_module * mod,
	enum as_guild_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

struct as_guild * as_guild_get(struct as_guild_module * mod, struct ap_guild * guild)
{
	return ap_module_get_attached_data(guild, mod->guild_offset);
}

struct as_guild_character_db * as_guild_get_character_db(
	struct as_guild_module * mod,
	struct as_character_db * character)
{
	return ap_module_get_attached_data(character, 
		mod->character_db_offset);
}

void as_guild_reflect(struct as_guild_module * mod, struct ap_guild * guild)
{
	struct as_guild * g = as_guild_get(mod, guild);
	struct as_guild_db * db = g->db;
	if (!db)
		return;
	strlcpy(db->id, guild->id, sizeof(db->id));
	strlcpy(db->password, guild->password, sizeof(db->password));
	strlcpy(db->notice, guild->notice, sizeof(db->notice));
	db->rank = guild->rank;
	db->creation_date = guild->creation_date;
	db->max_member_count = guild->max_member_count;
	db->union_id = guild->union_id;
	db->guild_mark_tid = guild->guild_mark_tid;
	db->guild_mark_color = guild->guild_mark_color;
	db->total_battle_point = guild->total_battle_point;
}

void as_guild_commit(struct as_guild_module * mod, boolean force)
{
	size_t index = 0;
	uint64_t tick = ap_tick_get(mod->ap_tick);
	struct deferred_entry * list = NULL;
	struct deferred_entry * cur = NULL;
	struct deferred_entry * last = NULL;
	struct ap_guild * g;
	uint32_t i;
	uint32_t count;
	if (force) {
		as_database_process(mod->as_database);
		task_wait_all();
	}
	if (mod->committing) {
		/* We force commits when server is shutting down,
		 * in which case we will have waited for previous 
		 * commits to have been completed. */
		assert(force == FALSE);
		return;
	}
	if (!force && 
		tick < mod->last_commit_tick + COMMIT_INTERVAL_MS) {
		return;
	}
	g = ap_guild_iterate(mod->ap_guild, &index);
	while (g) {
		list = newdeferred(mod, list, g);
		g = ap_guild_iterate(mod->ap_guild, &index);
	}
	/* We put pending deletes after create/update operations 
	 * but because the linked-list works as first-in, last-out, 
	 * they will be executed first. */
	count = vec_count(mod->pending_delete);
	for (i = 0; i < count; i++) {
		list = newdeferreddelete(mod, list, 
			&mod->pending_delete[i]);
	}
	vec_copy(&mod->committing_delete, mod->pending_delete);
	vec_clear(mod->pending_delete);
	if (list) {
		struct deferred_task * task;
		task = as_database_add_task(mod->as_database, taskdeferred, 
			taskdeferredpost, sizeof(*task));
		memset(task, 0, sizeof(*task));
		task->mod = mod;
		task->tick = tick;
		task->entries = list;
		if (force) {
			as_database_process(mod->as_database);
			task_wait_all();
		}
	}
	mod->committing = TRUE;
	mod->last_commit_tick = tick;
}

void as_guild_send_packet(struct as_guild_module * mod, struct ap_guild * guild)
{
	struct ap_admin * admin = &guild->member_admin;
	size_t index = 0;
	struct ap_guild_member * m = NULL;
	while (ap_admin_iterate_name(admin, &index, &m)) {
		struct ap_character * c;
		if (m->rank == AP_GUILD_MEMBER_RANK_JOIN_REQUEST)
			continue;
		c = as_player_get_by_name(mod->as_player, m->name);
		if (c)
			as_player_send_packet(mod->as_player, c);
	}
}

void as_guild_send_packet_with_exception(
	struct as_guild_module * mod,
	struct ap_guild * guild,
	struct ap_character * character)
{
	struct ap_admin * admin = &guild->member_admin;
	size_t index = 0;
	struct ap_guild_member * m = NULL;
	while (ap_admin_iterate_name(admin, &index, &m)) {
		struct ap_character * c;
		if (m->rank == AP_GUILD_MEMBER_RANK_JOIN_REQUEST)
			continue;
		c = as_player_get_by_name(mod->as_player, m->name);
		if (c && c != character)
			as_player_send_packet(mod->as_player, c);
	}
}

void as_guild_broadcast_members(
	struct as_guild_module * mod,
	struct ap_guild * guild,
	struct ap_character * character)
{
	size_t i = 0;
	struct ap_guild_member * m = ap_guild_iterate_member(mod->ap_guild, guild, &i);
	while (m) {
		ap_guild_make_join_packet(mod->ap_guild, guild, m);
		as_player_send_packet(mod->as_player, character);
		m = ap_guild_iterate_member(mod->ap_guild, guild, &i);
	}
}
