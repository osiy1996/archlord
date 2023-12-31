#include "server/as_login.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "utility/au_md5.h"

#include "public/ap_admin.h"
#include "public/ap_character.h"
#include "public/ap_config.h"
#include "public/ap_item.h"
#include "public/ap_item_convert.h"
#include "public/ap_login.h"
#include "public/ap_return_to_login.h"
#include "public/ap_skill.h"
#include "public/ap_ui_status.h"

#include "server/as_account.h"
#include "server/as_event_binding.h"
#include "server/as_item.h"
#include "server/as_server.h"

#include "vendor/PostgreSQL/openssl/evp.h"
#include "vendor/pcg/pcg_basic.h"

#include <assert.h>

#define REQ_VERSION_MAJOR 13
#define REQ_VERSION_MINOR 0
#define ENCRYPT_STRING "12345678"
#define ENCRYPT_STRING_LENGTH 8

#define MAXRETURNTOLOGINAUTH 128

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

struct return_to_login_auth {
	char account_id[AP_LOGIN_MAX_ID_LENGTH + 1];
	uint32_t auth_key;
};

struct character_preset {
	struct ap_character * character;
	uint32_t item_tid[8];
	uint32_t item_stack_count[8];
	uint32_t item_count;
};

struct as_login_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_config_module * ap_config;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct ap_login_module * ap_login;
	struct ap_return_to_login_module * ap_return_to_login;
	struct ap_skill_module * ap_skill;
	struct ap_ui_status_module * ap_ui_status;
	struct as_account_module * as_account;
	struct as_character_module * as_character;
	struct as_event_binding_module * as_event_binding;
	struct as_item_module * as_item;
	struct as_server_module * as_server;
	size_t conn_ad_offset;
	struct character_preset presets[9];
	struct ap_admin token_admin;
	pcg32_random_t token_rng;
	struct return_to_login_auth return_to_login_auth[MAXRETURNTOLOGINAUTH];
	uint32_t return_to_login_auth_count;
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

static void handlesuccessfullogin(
	struct as_login_module * mod,
	struct as_server_conn * conn,
	struct as_login_conn_ad * conn_login,
	struct as_account * account)
{
	INFO("Login: AccountID = %s", account->account_id);
	conn_login->stage = AS_LOGIN_STAGE_LOGGED_IN;
	conn_login->account = as_account_copy_detached(mod->as_account, account);
	as_account_reference(account);
	ap_login_make_sign_on_packet(mod->ap_login, account->account_id, NULL);
	as_server_send_packet(mod->as_server, conn);
	if (conn_login->in_return_to_login_process) {
		ap_return_to_login_make_end_process_packet(mod->ap_return_to_login);
		as_server_send_packet(mod->as_server, conn);
	}
	init_login_from_db(mod, conn, conn_login);
	dcsameaccount(mod, conn, account);
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
	handlesuccessfullogin(mod, conn, conn_login, account);
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

static void buildfrompreset(
	struct as_login_module * mod, 
	struct character_preset * preset, 
	struct ap_character * character)
{
	struct ap_skill_template * temp = ap_skill_get_template(mod->ap_skill, 222);
	uint32_t i;
	struct ap_ui_status_character * uiattachment;
	if (temp) {
		struct ap_skill_character * attachment = ap_skill_get_character(
			mod->ap_skill, character);
		struct ap_skill * skill = ap_skill_new(mod->ap_skill);
		uint32_t index;
		skill->tid = temp->id;
		skill->factor.dirt.skill_level = 1;
		skill->factor.dirt.skill_point = 1;
		skill->temp = temp;
		skill->status = AP_SKILL_STATUS_NOT_CAST;
		skill->is_temporary = FALSE;
		index = attachment->skill_count++;
		attachment->skill_id[index] = skill->id;
		attachment->skill[index] = skill;
	}
	for (i = 0; i < preset->item_count; i++) {
		struct ap_item * item = as_item_try_generate(mod->as_item, character, 
			AP_ITEM_STATUS_INVENTORY, preset->item_tid[i]);
		if (item)
			ap_item_set_stack_count(item, preset->item_stack_count[i]);
	}
	uiattachment = ap_ui_status_get_character(mod->ap_ui_status, character);
	uiattachment->items[0].base.type = AP_BASE_TYPE_SKILL;
	uiattachment->items[0].base.id = 0;
	uiattachment->items[0].tid = 222;
	uiattachment->hp_potion_tid = 190;
	uiattachment->mp_potion_tid = 193;
	uiattachment->option_view_helmet = TRUE;
	uiattachment->auto_use_hp_gauge = 50;
	uiattachment->auto_use_mp_gauge = 50;
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
			struct ap_item_character * attachment = ap_item_get_character(mod->ap_item, c);
			uint32_t j;
			ap_character_make_add_packet(mod->ap_character, c);
			as_server_send_packet(mod->as_server, conn);
			for (j = 0; j  < attachment->equipment->grid_count; j++) {
				struct ap_item * item = ap_grid_get_object_by_index(
					attachment->equipment, j);
				if (item) {
					ap_item_make_add_packet(mod->ap_item, item);
					as_server_send_packet(mod->as_server, conn);
					ap_item_convert_make_add_packet(mod->ap_item_convert, item);
					as_server_send_packet(mod->as_server, conn);
				}
			}
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
		struct character_preset * preset = NULL;
		struct ap_character * c = NULL;
		struct as_account * cached;
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
		for (i = 0; i < COUNT_OF(mod->presets); i++) {
			if (mod->presets[i].character->tid == d->char_info->tid) {
				preset = &mod->presets[i];
				break;
			}
		}
		if (!preset)
			return FALSE;
		assert(ad->character_count == ad->account->character_count);
		if (ad->character_count >= AS_ACCOUNT_MAX_CHARACTER)
			return FALSE;
		cached = as_account_load_from_cache(mod->as_account, 
			ad->account->account_id, FALSE);
		assert(cached != NULL);
		if (!cached || cached->character_count >= AS_ACCOUNT_MAX_CHARACTER)
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
		cdb->tid = preset->character->tid;
		cdb->level = 1;
		cdb->hp = 100;
		cdb->mp = 100;
		cdb->hair = d->char_info->hair_index;
		cdb->face = d->char_info->face_index;
		cdb->bound_region_id = preset->character->bound_region_id;
		cdb->position = preset->character->pos;
		c = as_character_from_db(mod->as_character, cdb);
		ad->characters[ad->character_count++] = c;
		buildfrompreset(mod, preset, c);
		/* Refresh database state before making a copy on cached account. */
		as_character_reflect(mod->as_character, c);
		cached->characters[cached->character_count++] = as_character_copy_database(
			mod->as_character, cdb);
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
		for (i = 0; i < COUNT_OF(mod->presets); i++) {
			ap_character_make_add_packet(mod->ap_character, mod->presets[i].character);
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

static boolean consumereturntologinauth(
	struct as_login_module * mod, 
	const char * account_id,
	uint32_t auth_key)
{
	uint32_t i;
	for (i = 0; i < mod->return_to_login_auth_count; i++) {
		if (strcmp(mod->return_to_login_auth[i].account_id, account_id) == 0) {
			if (mod->return_to_login_auth[i].auth_key == auth_key) {
				memcpy(&mod->return_to_login_auth[i],
					&mod->return_to_login_auth[--mod->return_to_login_auth_count],
					sizeof(mod->return_to_login_auth[0]));
				return TRUE;
			}
			else {
				return FALSE;
			}
		}
	}
	return FALSE;
}

static boolean cbreturntologinreceive(
	struct as_login_module * mod, 
	struct ap_return_to_login_cb_receive * cb)
{
	struct as_server_conn * conn;
	if (cb->domain != AP_RETURN_TO_LOGIN_DOMAIN_LOGIN)
		return TRUE;
	conn = cb->user_data;
	switch (cb->type) {
	case AP_RETURN_TO_LOGIN_PACKET_RECONNECT_LOGIN_SERVER: {
		struct as_login_conn_ad * attachment;
		if (!consumereturntologinauth(mod, cb->account_id, cb->auth_key)) {
			ap_login_make_login_result_packet(mod->ap_login,
				AP_LOGIN_RESULT_INVALID_ACCOUNT, NULL);
			as_server_send_packet(mod->as_server, conn);
			return FALSE;
		}
		attachment = as_login_get_attached_conn_data(mod, conn);
		attachment->in_return_to_login_process = TRUE;
		attachment->stage = AS_LOGIN_STAGE_AWAIT_LOGIN;
		ap_login_make_encrypt_code_packet(mod->ap_login, ENCRYPT_STRING);
		as_server_send_packet(mod->as_server, conn);
		return TRUE;
	}
	}
	return FALSE;
}

static boolean onregister(
	struct as_login_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item_convert, AP_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_login, AP_LOGIN_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_return_to_login, AP_RETURN_TO_LOGIN_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_skill, AP_SKILL_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_ui_status, AP_UI_STATUS_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_account, AS_ACCOUNT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_event_binding, AS_EVENT_BINDING_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_item, AS_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_server, AS_SERVER_MODULE_NAME);
	ap_login_add_callback(mod->ap_login, AP_LOGIN_CB_RECEIVE, mod, cb_receive);
	mod->conn_ad_offset = as_server_attach_data(mod->as_server,
		AS_SERVER_MDI_CONNECTION, sizeof(struct as_login_conn_ad),
		mod, connctor, NULL);
	as_server_add_callback(mod->as_server, AS_SERVER_CB_DISCONNECT, mod, cbdisconnect);
	ap_return_to_login_add_callback(mod->ap_return_to_login, AP_RETURN_TO_LOGIN_CB_RECEIVE, mod, cbreturntologinreceive);
	return TRUE;
}

static void onclose(struct as_login_module * mod)
{
	uint32_t i;
	for (i = 0; i < COUNT_OF(mod->presets); i++) {
		if (mod->presets[i].character) {
			ap_character_free(mod->ap_character, mod->presets[i].character);
			mod->presets[i].character = NULL;
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
	uint32_t weapons[9] = { 11, 1, 12, 4, 6, 9, 4920, 2727, 2711 };
	uint32_t extras[9] = { 0, 194, 0, 0, 238, 0, 5558, 0, 0 };
	uint32_t extrascount[9] = { 0, 500, 0, 0, 500, 0, 1, 0, 0 };
	uint32_t basic[3] = { 190, 193, 6393 };
	uint32_t basiccount[3] = { 5, 3, 5 };
	assert(COUNT_OF(tids) == COUNT_OF(mod->presets));
	for (i = 0; i < COUNT_OF(tids); i++) {
		struct character_preset * preset = &mod->presets[i];
		struct ap_character_template * temp = 
			ap_character_get_template(mod->ap_character, tids[i]);
		struct ap_character * c;
		uint32_t j;
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
		preset->character = c;
		preset->item_tid[preset->item_count] = weapons[i];
		preset->item_stack_count[preset->item_count++] = 1;
		if (extras[i]) {
			assert(extrascount[i] != 0);
			assert(preset->item_count < COUNT_OF(preset->item_tid));
			preset->item_tid[preset->item_count] = extras[i];
			preset->item_stack_count[preset->item_count++] = extrascount[i];
		}
		for (j = 0; j < COUNT_OF(basic); j++) {
			assert(basic[j] != 0);
			assert(basiccount[j] != 0);
			assert(preset->item_count < COUNT_OF(preset->item_tid));
			preset->item_tid[preset->item_count] = basic[j];
			preset->item_stack_count[preset->item_count++] = basiccount[j];
		}
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

void as_login_set_return_to_login_auth_key(
	struct as_login_module * mod,
	const char * account_id,
	uint32_t auth_key)
{
	uint32_t i;
	for (i = 0; i < mod->return_to_login_auth_count; i++) {
		if (strcmp(mod->return_to_login_auth[i].account_id, account_id) == 0) {
			mod->return_to_login_auth[i].auth_key = auth_key;
			return;
		}
	}
	if (mod->return_to_login_auth_count < MAXRETURNTOLOGINAUTH) {
		uint32_t index = mod->return_to_login_auth_count++;
		struct return_to_login_auth * auth = &mod->return_to_login_auth[index];
		strlcpy(auth->account_id, account_id, sizeof(auth->account_id));
		auth->auth_key = auth_key;
	}
}
