#include <assert.h>

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "utility/au_md5.h"

#include "public/ap_admin.h"
#include "public/ap_character.h"
#include "public/ap_config.h"
#include "public/ap_login.h"

#include "server/as_account.h"
#include "server/as_event_binding.h"
#include "server/as_login.h"
#include "server/as_server.h"

#include "vendor/PostgreSQL/openssl/evp.h"
#include "vendor/pcg/pcg_basic.h"

#define REQ_VERSION_MAJOR 13
#define REQ_VERSION_MINOR 0
#define ENCRYPT_STRING "12345678"
#define ENCRYPT_STRING_LENGTH 8

struct deferred_login_data {
	uint64_t conn_id;
	char password[AP_LOGIN_MAX_PW_LENGTH + 1];
};

struct game_token {
	uint64_t conn_id;
	char character[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	uint32_t token;
	time_t expire;
};

struct as_login_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_config_module * ap_config;
	struct ap_login_module * ap_login;
	struct as_account_module * as_account;
	struct as_character_module * as_character;
	struct as_event_binding_module * as_event_binding;
	struct as_server_module * as_server;
	size_t conn_ad_offset;
	struct ap_character * char_create[9];
	struct ap_admin token_admin;
	pcg32_random_t token_rng;
};

static boolean setspawnpos(
	struct as_login_module * mod,
	struct ap_character * c)
{
	const struct au_pos * pos;
	switch (c->temp->factor.char_type.race) {
	case AU_RACE_TYPE_HUMAN:
		c->bound_region_id = 9; /* Anchorville */
		break;
	case AU_RACE_TYPE_ORC:
		c->bound_region_id = 5; /* Golundo */
		break;
	case AU_RACE_TYPE_MOONELF:
		c->bound_region_id = 26; /* Norine */
		break;
	default:
		return FALSE;
	}
	pos = as_event_binding_find_by_type(mod->as_event_binding, c->bound_region_id, 
		AP_EVENT_BINDING_TYPE_NEW_CHARACTER, NULL);
	if (!pos) {
		ERROR("Failed to find a spawn position for new character.");
		return FALSE;
	}
	c->pos = *pos;
	return TRUE;
}

static void init_login_from_db(
	struct as_login_module * mod,
	struct as_server_conn * conn,
	struct as_login_conn_ad * conn_login)
{
	/* Here, we will create and initialize characters with 
	 * what we have loaded from the database.
	 *
	 * After login, server does not keep track of which 
	 * stage of login the client is in 
	 * (character selection, creation, etc.), as such, 
	 * states required for all login actions need to be 
	 * initialized here. */
	struct as_account * account = conn_login->account;
	uint32_t i;
	for (i = 0; i < account->character_count; i++) {
		struct as_character_db * db = account->characters[i];
		conn_login->characters[conn_login->character_count++] =
			as_character_from_db(mod->as_character, db);
	}
}

static void dcsameaccount(
	struct as_login_module * mod,
	struct as_server_conn * conn,
	struct as_account * account)
{
	size_t index = 0;
	struct as_server_conn * c = NULL;
	while (as_server_iterate_conn(mod->as_server, AS_SERVER_LOGIN, &index, &c)) {
		struct as_login_conn_ad * ad = 
			as_login_get_attached_conn_data(mod, c);
		if (conn != c && strcmp(account->account_id, ad->account->account_id) == 0) {
			as_server_disconnect(mod->as_server, c);
			index = 0;
		}
	}
}

static void handle_login(
	struct as_login_module * mod,
	struct as_server_conn * conn,
	struct as_login_conn_ad * conn_login,
	struct as_account * account,
	const char * password)
{
	uint8_t hash[AS_ACCOUNT_PW_HASH_SIZE];
	/* Login stage must be verified by the calling function. */
	assert(conn_login->stage == AS_LOGIN_STAGE_AWAIT_LOGIN);
	if (!as_account_hash_password(password, 
			account->pw_salt, sizeof(account->pw_salt),
			hash, sizeof(hash)) ||
		memcmp(account->pw_hash, hash, sizeof(hash)) != 0) {
		ap_login_make_login_result_packet(mod->ap_login,
			AP_LOGIN_RESULT_INVALID_ACCOUNT, NULL);
		as_server_send_packet(mod->as_server, conn);
		return;
	}
	INFO("Login: AccountID = %s", account->account_id);
	conn_login->stage = AS_LOGIN_STAGE_LOGGED_IN;
	conn_login->account = as_account_copy_detached(mod->as_account, account);
	as_account_reference(account);
	ap_login_make_sign_on_packet(mod->ap_login, account->account_id, NULL);
	as_server_send_packet(mod->as_server, conn);
	init_login_from_db(mod, conn, conn_login);
	dcsameaccount(mod, conn, account);
}

static void deferred_login(
	struct as_login_module * mod,
	const char * account_id,
	struct as_account * account,
	struct deferred_login_data * d)
{
	struct as_server_conn * conn = 
		as_server_get_conn(mod->as_server, AS_SERVER_LOGIN, d->conn_id);
	struct as_login_conn_ad * ad = 
		ap_module_get_attached_data(conn, mod->conn_ad_offset);
	if (!conn) {
		/* Client disconnected, no need to handle login. */
		return;
	}
	if (!account) {
		/* Account does not exist or we failed to load it. */
		ap_login_make_login_result_packet(mod->ap_login,
			AP_LOGIN_RESULT_INVALID_ACCOUNT, NULL);
		as_server_send_packet(mod->as_server, conn);
		return;
	}
	if (ad->stage != AS_LOGIN_STAGE_AWAIT_LOGIN) {
		as_server_disconnect(mod->as_server, conn);
		return;
	}
	handle_login(mod, conn, ad, account, d->password);
}

static boolean cb_receive(struct as_login_module * mod, void * data)
{
	struct ap_login_cb_receive * d = data;
	struct as_server_conn * conn = d->user_data;
	struct as_login_conn_ad * ad;
	if (conn->server_type != AS_SERVER_LOGIN)
		return TRUE;
	ad = as_login_get_attached_conn_data(mod, conn);
	switch (d->type) {
	case AP_LOGIN_PACKET_ENCRYPT_CODE:
		if (ad->stage != AS_LOGIN_STAGE_AWAIT_VERSION ||
			!d->version_info ||
			d->version_info->major != REQ_VERSION_MAJOR ||
			d->version_info->minor != REQ_VERSION_MINOR) {
			return FALSE;
		}
		ad->stage = AS_LOGIN_STAGE_AWAIT_LOGIN;
		ap_login_make_encrypt_code_packet(mod->ap_login, ENCRYPT_STRING);
		as_server_send_packet(mod->as_server, conn);
		return TRUE;
	case AP_LOGIN_PACKET_SIGN_ON: {
		struct as_account * account;
		if (ad->stage != AS_LOGIN_STAGE_AWAIT_LOGIN)
			return FALSE;
		au_md5_crypt(d->account_id, strlen(d->account_id),
			ENCRYPT_STRING, ENCRYPT_STRING_LENGTH);
		au_md5_crypt(d->password, strlen(d->password),
			ENCRYPT_STRING, ENCRYPT_STRING_LENGTH);
		if (!as_account_validate_account_id(d->account_id))
			return FALSE;
		account = as_account_load_from_cache(mod->as_account, d->account_id, FALSE);
		if (account) {
			handle_login(mod, conn, ad, account, d->password);
		}
		else {
			struct deferred_login_data * task = 
				as_account_load_deferred(mod->as_account, d->account_id,
					mod, deferred_login, sizeof(*task));
			memset(task, 0, sizeof(*task));
			task->conn_id = conn->id;
			strcpy(task->password, d->password);
		}
		return TRUE;
	}
	case AP_LOGIN_PACKET_UNION_INFO: {
		/* This is server selection packet, not sure why 
		 * it was named something else instead. */
		uint32_t i;
		if (ad->stage != AS_LOGIN_STAGE_LOGGED_IN)
			return FALSE;
		ap_login_make_union_info_packet(mod->ap_login, 0);
		as_server_send_packet(mod->as_server, conn);
		for (i = 0; i < ad->account->character_count; i++) {
			const struct as_character_db * c = 
				ad->account->characters[i];
			ap_login_make_char_name_packet(mod->ap_login, c->name, c->slot,
				ad->account->character_count);
			as_server_send_packet(mod->as_server, conn);
		}
		ap_login_make_packet(mod->ap_login, AP_LOGIN_PACKET_CHARACTER_NAME_FINISH);
		as_server_send_packet(mod->as_server, conn);
		return TRUE;
	}
	case AP_LOGIN_PACKET_CHARACTER_INFO: {
		uint32_t i;
		if (ad->stage != AS_LOGIN_STAGE_LOGGED_IN)
			return FALSE;
		for (i = 0; i < ad->character_count; i++) {
			struct ap_character * c = ad->characters[i];
			ap_character_make_add_packet(mod->ap_character, c);
			as_server_send_packet(mod->as_server, conn);
		}
		ap_login_make_packet(mod->ap_login, AP_LOGIN_PACKET_CHARACTER_INFO_FINISH);
		as_server_send_packet(mod->as_server, conn);
		return TRUE;
	}
	case AP_LOGIN_PACKET_ENTER_GAME: {
		struct ap_character * c = NULL;
		uint32_t i;
		struct game_token * t;
		char gameaddress[32];
		if (ad->stage != AS_LOGIN_STAGE_LOGGED_IN || !d->char_info)
			return FALSE;
		for (i = 0; i < ad->character_count; i++) {
			if (strcmp(ad->characters[i]->name, 
					d->char_info->char_name) == 0) {
				c = ad->characters[i];
				break;
			}
		}
		if (!c)
			return FALSE;
		t = ap_admin_get_object_by_name(&mod->token_admin, c->name);
		if (!t) {
			t = ap_admin_add_object_by_name(&mod->token_admin,
				c->name);
			if (!t) {
				assert(0);
				return FALSE;
			}
			strlcpy(t->character, c->name, sizeof(t->character));
		}
		t->conn_id = conn->id;
		t->token = 1000 + pcg32_boundedrand_r(&mod->token_rng, 
			UINT32_MAX - 1000);
		t->expire = time(NULL) + 60;
		ap_character_make_client_packet(mod->ap_character,
			AC_CHARACTER_PACKET_AUTH_KEY,
			NULL, NULL, NULL, NULL, NULL, &t->token);
		as_server_send_packet(mod->as_server, conn);
		snprintf(gameaddress, sizeof(gameaddress), "%s:%s",
			ap_config_get(mod->ap_config, "GameServerIP"),
			ap_config_get(mod->ap_config, "GameServerPort"));
		ap_login_make_enter_game_packet(mod->ap_login, c->id, c->name, gameaddress);
		as_server_send_packet(mod->as_server, conn);
		ad->stage = AS_LOGIN_STAGE_ENTERING_GAME;
		return TRUE;
	}
	case AP_LOGIN_PACKET_RETURN_TO_SELECT_WORLD:
		return TRUE;
	case AP_LOGIN_PACKET_NEW_CHARACTER_NAME: {
		uint8_t slotmask = 0;
		uint8_t slot;
		uint32_t i;
		struct as_character_db * cdb;
		struct ap_character * preset = NULL;
		struct ap_character * c = NULL;
		if (ad->stage != AS_LOGIN_STAGE_LOGGED_IN || !d->char_info)
			return FALSE;
		if (!as_character_is_name_valid(mod->as_character, d->char_info->char_name)) {
			ap_login_make_login_result_packet(mod->ap_login, 
				AP_LOGIN_RESULT_UNMAKABLE_CHAR_NAME, 
				d->char_info->char_name);
			as_server_send_packet(mod->as_server, conn);
			return TRUE;
		}
		if (as_character_is_name_reserved(mod->as_character, d->char_info->char_name)) {
			ap_login_make_login_result_packet(mod->ap_login, 
				AP_LOGIN_RESULT_CHAR_NAME_ALREADY_EXIST, 
				d->char_info->char_name);
			as_server_send_packet(mod->as_server, conn);
			return TRUE;
		}
		for (i = 0; i < COUNT_OF(mod->char_create); i++) {
			if (mod->char_create[i]->tid == d->char_info->tid) {
				preset = mod->char_create[i];
				break;
			}
		}
		if (!preset)
			return FALSE;
		assert(ad->character_count == ad->account->character_count);
		if (ad->character_count >= AS_ACCOUNT_MAX_CHARACTER)
			return FALSE;
		for (i = 0; i < ad->account->character_count; i++) {
			cdb = ad->account->characters[i];
			assert(cdb->slot < 3);
			assert((slotmask & (1u << cdb->slot)) == 0);
			slotmask |= 1u << cdb->slot;
		}
		if (!(slotmask & 1u))
			slot = 0;
		else if (!(slotmask & 2u))
			slot = 1;
		else if (!(slotmask & 4u))
			slot = 2;
		else
			return FALSE;
		if (!as_character_reserve_name(mod->as_character, d->char_info->char_name,
				ad->account->account_id)) {
			return FALSE;
		}
		cdb = as_character_new_db(mod->as_character);
		ad->account->characters[ad->account->character_count++] = cdb;
		strlcpy(cdb->name, d->char_info->char_name, sizeof(cdb->name));
		cdb->creation_date = time(NULL);
		cdb->slot = slot;
		cdb->tid = preset->tid;
		cdb->level = 1;
		cdb->hp = 100;
		cdb->mp = 100;
		cdb->hair = d->char_info->hair_index;
		cdb->face = d->char_info->face_index;
		cdb->bound_region_id = preset->bound_region_id;
		cdb->position = preset->pos;
		/** \todo Add skills and setup quickbelt. */
		c = as_character_from_db(mod->as_character, cdb);
		ad->characters[ad->character_count++] = c;
		ap_login_make_new_char_name_packet(mod->ap_login, cdb->name, cdb->slot,
			ad->account->character_count);
		as_server_send_packet(mod->as_server, conn);
		ap_character_make_add_packet(mod->ap_character, c);
		as_server_send_packet(mod->as_server, conn);
		ap_login_make_packet(mod->ap_login, AP_LOGIN_PACKET_NEW_CHARACTER_INFO_FINISH);
		as_server_send_packet(mod->as_server, conn);
		return TRUE;
	}
	case AP_LOGIN_PACKET_RACE_BASE: {
		uint32_t i;
		for (i = 0; i < COUNT_OF(mod->char_create); i++) {
			ap_character_make_add_packet(mod->ap_character, mod->char_create[i]);
			as_server_send_packet(mod->as_server, conn);
		}
		ap_login_make_packet(mod->ap_login, AP_LOGIN_PACKET_RACE_BASE);
		as_server_send_packet(mod->as_server, conn);
		return TRUE;
	}
	default:
		return FALSE;
	}
}

static boolean cbdisconnect(
	struct as_login_module * mod,
	struct as_server_conn * conn)
{
	struct as_login_conn_ad * ad;
	uint32_t i;
	if (conn->server_type != AS_SERVER_LOGIN)
		return TRUE;
	ad = as_login_get_attached_conn_data(mod, conn);
	ad->stage = AS_LOGIN_STAGE_DISCONNECTED;
	for (i = 0; i < ad->character_count; i++) {
		ap_character_free(mod->ap_character, ad->characters[i]);
		ad->characters[i] = NULL;
	}
	ad->character_count = 0;
	if (ad->account) {
		/* When account is loaded at login (either from cached or databased), 
		 * its refcount is increased to retain it in cache as user enters the game.
		 * 
		 * It is expected at this point the account should remain in cache. */
		struct as_account * cached = as_account_load_from_cache(mod->as_account,
			ad->account->account_id, FALSE);
		assert(cached != NULL);
		as_account_release(cached);
		ad->account = NULL;
	}
	return TRUE;
}

static boolean connctor(struct as_login_module * mod, void * data)
{
	struct as_server_conn * conn = data;
	struct as_login_conn_ad * ad;
	ad = ap_module_get_attached_data(conn, mod->conn_ad_offset);
	ad->stage = AS_LOGIN_STAGE_AWAIT_VERSION;
	return TRUE;
}

static boolean onregister(
	struct as_login_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_login, AP_LOGIN_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_account, AS_ACCOUNT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_event_binding, AS_EVENT_BINDING_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_server, AS_SERVER_MODULE_NAME);
	ap_login_add_callback(mod->ap_login, AP_LOGIN_CB_RECEIVE, mod, cb_receive);
	mod->conn_ad_offset = as_server_attach_data(mod->as_server,
		AS_SERVER_MDI_CONNECTION, sizeof(struct as_login_conn_ad),
		mod, connctor, NULL);
	as_server_add_callback(mod->as_server, AS_SERVER_CB_DISCONNECT, mod, cbdisconnect);
	return TRUE;
}

static void onclose(struct as_login_module * mod)
{
	uint32_t i;
	for (i = 0; i < COUNT_OF(mod->char_create); i++) {
		if (mod->char_create[i]) {
			ap_character_free(mod->ap_character, mod->char_create[i]);
			mod->char_create[i] = NULL;
		}
	}
}

static void onshutdown(struct as_login_module * mod)
{
	ap_admin_destroy(&mod->token_admin);
}

struct as_login_module * as_login_create_module()
{
	struct as_login_module * mod = ap_module_instance_new(AS_LOGIN_MODULE_NAME,
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	ap_admin_init(&mod->token_admin, 
		sizeof(struct game_token), 128);
	pcg32_srandom_r(&mod->token_rng, (uint64_t)time(NULL),
		(uint64_t)time);
	return mod;
}

boolean as_login_init_presets(struct as_login_module * mod)
{
	uint32_t i;
	uint32_t tids[9] = { 96, 1, 6, 4, 8, 3, 377, 460, 9 };
	for (i = 0; i < COUNT_OF(tids); i++) {
		struct ap_character_template * temp = 
			ap_character_get_template(mod->ap_character, tids[i]);
		struct ap_character * c;
		if (!temp) {
			ERROR("Invalid character preset (%u).", tids[i]);
			return FALSE;
		}
		c = ap_character_new(mod->ap_character);
		ap_character_set_template(mod->ap_character, c, temp);
		sprintf(c->name, "CC%u", tids[i]);
		if (!setspawnpos(mod, c)) {
			ERROR("Failed to set spawn position (%u).", tids[i]);
			ap_character_free(mod->ap_character, c);
			return FALSE;
		}
		c->login_status = AP_CHARACTER_STATUS_IN_GAME_WORLD;
		mod->char_create[i] = c;
	}
	return TRUE;
}

void as_login_add_callback(
	struct as_login_module * mod,
	enum as_login_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

struct as_login_conn_ad * as_login_get_attached_conn_data(
	struct as_login_module * mod,
	struct as_server_conn * conn)
{
	return ap_module_get_attached_data(conn, mod->conn_ad_offset);
}

boolean as_login_confirm_auth_key(
	struct as_login_module * mod,
	const char * character_name,
	uint32_t auth_key)
{
	struct game_token * t = ap_admin_get_object_by_name(
		&mod->token_admin, character_name);
	struct as_server_conn * conn;
	if (!t)
		return FALSE;
	if (time(NULL) >= t->expire) {
		ap_admin_remove_object_by_name(&mod->token_admin,
			character_name);
		return FALSE;
	}
	if (t->token != auth_key)
		return FALSE;
	conn = as_server_get_conn(mod->as_server, AS_SERVER_LOGIN, t->conn_id);
	if (conn)
		as_server_disconnect_in(mod->as_server, conn, 5000);
	ap_admin_remove_object_by_name(&mod->token_admin,
		character_name);
	return TRUE;
}
