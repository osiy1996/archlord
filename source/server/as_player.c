#include <assert.h>

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_admin.h"
#include "public/ap_character.h"

#include "server/as_player.h"
#include "server/as_server.h"

struct as_player_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct as_account_module * as_account;
	struct as_character_module * as_character;
	struct as_server_module * as_server;
	size_t char_ad_offset;
	struct ap_admin player_admin;
	struct ap_admin session_admin;
};

static boolean charctor(struct as_player_module * mod, void * data)
{
	return TRUE;
}

static boolean chardtor(struct as_player_module * mod, void * data)
{
	return TRUE;
}

static boolean cbaccountprecommit(
	struct as_player_module * mod,
	struct as_account_cb_pre_commit * cb)
{
	uint32_t i;
	for (i = 0; i < cb->account->character_count; i++) {
		struct ap_character * c = 
			as_player_get_by_name(mod, cb->account->characters[i]->name);
		if (c)
			as_character_reflect(mod->as_character, c);
	}
	return TRUE;
}

static boolean onregister(
	struct as_player_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_account, AS_ACCOUNT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_server, AS_SERVER_MODULE_NAME);
	mod->char_ad_offset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR,
		sizeof(struct as_player_character),
		mod, charctor, chardtor);
	if (mod->char_ad_offset == SIZE_MAX) {
		ERROR("Failed to attach character data.");
		return FALSE;
	}
	as_account_add_callback(mod->as_account, AS_ACCOUNT_CB_PRE_COMMIT, 
		mod, cbaccountprecommit);
	return TRUE;
}

static void onclose(struct as_player_module * mod)
{
	size_t index = 0;
	struct as_player_session ** obj = NULL;
	while (ap_admin_iterate_id(&mod->session_admin, &index, (void **)&obj))
		ap_module_destroy_module_data(mod, AS_PLAYER_MDI_SESSION, *obj);
	ap_admin_clear_objects(&mod->session_admin);
}

static void onshutdown(struct as_player_module * mod)
{
	/* Player character lifetimes are largely controlled by 
	 * as_game_admin module and it is expected that 
	 * all player characters have been removed by this point. */
	assert(ap_admin_get_object_count(&mod->player_admin) == 0);
	ap_admin_destroy(&mod->player_admin);
	ap_admin_destroy(&mod->session_admin);
}

struct as_player_module * as_player_create_module()
{
	struct as_player_module * mod = ap_module_instance_new(AS_PLAYER_MODULE_NAME,
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	ap_module_set_module_data(mod, AS_PLAYER_MDI_SESSION, 
		sizeof(struct as_player_session), NULL, NULL);
	ap_admin_init(&mod->player_admin, sizeof(struct ap_character *), 128);
	ap_admin_init(&mod->session_admin, sizeof(struct as_player_session *), 128);
	return mod;
}

void as_player_add_callback(
	struct as_player_module * mod,
	enum as_player_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t as_player_attach_data(
	struct as_player_module * mod,
	enum as_player_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

struct as_player_character * as_player_get_character_ad(
	struct as_player_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->char_ad_offset);
}

boolean as_player_add(struct as_player_module * mod, struct ap_character * character)
{
	struct as_player_cb_add cb = { character };
	struct ap_character ** obj = ap_admin_add_object(
		&mod->player_admin, character->id, character->name);
	if (!obj)
		return FALSE;
	*obj = character;
	ap_module_enum_callback(mod, AS_PLAYER_CB_ADD, &cb);
	return TRUE;
}

struct ap_character * as_player_get_by_id(
	struct as_player_module * mod, 
	uint32_t character_id)
{
	struct ap_character ** obj = ap_admin_get_object_by_id(
		&mod->player_admin, character_id);
	if (!obj)
		return NULL;
	return *obj;
}

struct ap_character * as_player_get_by_name(
	struct as_player_module * mod,
	const char * character_name)
{
	struct ap_character ** obj = ap_admin_get_object_by_name(
		&mod->player_admin, character_name);
	if (!obj)
		return NULL;
	return *obj;
}

boolean as_player_remove(
	struct as_player_module * mod, 
	struct ap_character * character)
{
	struct as_player_cb_remove cb = { character };
	if (!ap_admin_remove_object(&mod->player_admin, character->id, character->name))
		return FALSE;
	ap_module_enum_callback(mod, AS_PLAYER_CB_REMOVE, &cb);
	return TRUE;
}

struct ap_character * as_player_iterate(
	struct as_player_module * mod, 
	size_t * index)
{
	struct ap_character ** c = NULL;
	if (!ap_admin_iterate_id(&mod->player_admin, index, 
			(void **)&c))
		return NULL;
	return *c;
}

struct as_player_session * as_player_get_session(
	struct as_player_module * mod,
	const char * character_name)
{
	struct as_player_session ** obj = 
		ap_admin_get_object_by_name(&mod->session_admin, character_name);
	if (!obj) {
		struct as_player_session * session = ap_module_create_module_data(mod,
			AS_PLAYER_MDI_SESSION);
		obj = ap_admin_add_object_by_name(&mod->session_admin, character_name);
		assert(obj != NULL);
		*obj = session;
		return session;
	}
	return *obj;
}

void as_player_send_packet(
	struct as_player_module * mod, 
	struct ap_character * character)
{
	struct as_player_character * pc = 
		ap_module_get_attached_data(character, mod->char_ad_offset);
	if (pc->conn)
		as_server_send_packet(mod->as_server, pc->conn);
}

void as_player_send_custom_packet(
	struct as_player_module * mod,
	struct ap_character * character,
	void * buffer,
	uint16_t length)
{
	struct as_player_character * pc = 
		ap_module_get_attached_data(character, mod->char_ad_offset);
	if (pc->conn)
		as_server_send_custom_packet(mod->as_server, pc->conn, buffer, length);
}
