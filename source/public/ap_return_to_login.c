#include "public/ap_return_to_login.h"

#include "core/log.h"
#include "core/string.h"

#include "public/ap_character.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"

#include <assert.h>

#define MAXLOGINSERVERADDRSIZE 25

struct ap_return_to_login_module {
	struct ap_module_instance instance;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
};

static boolean onregister(
	struct ap_return_to_login_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	return TRUE;
}

struct ap_return_to_login_module * ap_return_to_login_create_module()
{
	struct ap_return_to_login_module * mod = ap_module_instance_new(
		AP_RETURN_TO_LOGIN_MODULE_NAME, sizeof(*mod), onregister, NULL, NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint8_t),
		AU_PACKET_TYPE_INT8, 1, /* packet type */
		AU_PACKET_TYPE_INT32, 1, /* character id */
		AU_PACKET_TYPE_CHAR, AP_LOGIN_MAX_ID_LENGTH + 1, /* account name */
		AU_PACKET_TYPE_CHAR, AP_LOGIN_SERVER_GROUP_NAME_SIZE, /* server group name */
		AU_PACKET_TYPE_CHAR, MAXLOGINSERVERADDRSIZE, /* login server address */
		AU_PACKET_TYPE_INT32, 1, /* encrypt code */
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_return_to_login_add_callback(
	struct ap_return_to_login_module * mod,
	enum ap_return_to_login_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

boolean ap_return_to_login_on_receive(
	struct ap_return_to_login_module * mod,
	const void * data,
	uint16_t length,
	enum ap_return_to_login_domain domain,
	void * user_data)
{
	struct ap_return_to_login_cb_receive cb = { 0 };
	const char * accountid = NULL;
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type, /* packet type */
			NULL, /* character id */
			&accountid, /* account name */
			NULL, /* server group name */
			NULL, /* login server address */
			&cb.auth_key)) { /* encrypt code */
		return FALSE;
	}
	AU_PACKET_GET_STRING(cb.account_id, accountid);
	cb.domain = domain;
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_RETURN_TO_LOGIN_CB_RECEIVE, &cb);
}

void ap_return_to_login_make_send_key_addr_packet(
	struct ap_return_to_login_module * mod,
	uint32_t character_id,
	const char * addr,
	uint32_t auth_key)
{
	uint32_t type = AP_RETURN_TO_LOGIN_PACKET_SEND_KEY_ADDR;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	char addrbuf[MAXLOGINSERVERADDRSIZE] = { 0 };
	strlcpy(addrbuf, addr, sizeof(addrbuf));
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length, AP_RETURNTOLOGIN_PACKET_TYPE,
		&type, /* packet type */
		&character_id, /* character id */
		NULL, /* account name */
		NULL, /* server group name */
		addrbuf, /* login server address */
		&auth_key); /* encrypt code */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_return_to_login_make_end_process_packet(
	struct ap_return_to_login_module * mod)
{
	uint32_t type = AP_RETURN_TO_LOGIN_PACKET_END_PROCESS;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length, AP_RETURNTOLOGIN_PACKET_TYPE,
		&type, /* packet type */
		NULL, /* character id */
		NULL, /* account name */
		NULL, /* server group name */
		NULL, /* login server address */
		NULL); /* encrypt code */
	ap_packet_set_length(mod->ap_packet, length);
}
