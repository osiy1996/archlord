#include "public/ap_login.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_define.h"
#include "public/ap_module.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"

struct ap_login_module {
	struct ap_module_instance instance;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
	struct au_packet packet_char_info;
	struct au_packet packet_server_info;
	struct au_packet packet_version_info;
};

static boolean onregister(
	struct ap_login_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, 
		AP_PACKET_MODULE_NAME);
	return TRUE;
}

struct ap_login_module * ap_login_create_module()
{
	struct ap_login_module * mod = ap_module_instance_new(AP_LOGIN_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint16_t),
		AU_PACKET_TYPE_UINT8, 1, /* Packet Type */ 
		AU_PACKET_TYPE_CHAR, AP_LOGIN_ENCRYPT_STRING_SIZE,
		/* Account ID */
		AU_PACKET_TYPE_CHAR, AP_LOGIN_MAX_ID_LENGTH + 1,
		AU_PACKET_TYPE_UINT8, 1, /* Account ID Length */
		/* Password */
		AU_PACKET_TYPE_CHAR, AP_LOGIN_MAX_PW_LENGTH + 1,
		AU_PACKET_TYPE_UINT8, 1, /* Password Length */
		AU_PACKET_TYPE_UINT32, 1, /* Character ID */
		/* Server Address */
		AU_PACKET_TYPE_CHAR, AP_LOGIN_SERVER_GROUP_NAME_SIZE,
		AU_PACKET_TYPE_PACKET, 1, /* Character Info. Packet */
		AU_PACKET_TYPE_PACKET, 1, /* Server Info. Packet */
		AU_PACKET_TYPE_UINT32, 1, /* Result */
		AU_PACKET_TYPE_PACKET, 1, /* Version Info. Packet */
		AU_PACKET_TYPE_CHAR, AP_LOGIN_J_AUTH_STRING + 1,
		AU_PACKET_TYPE_CHAR, AP_LOGIN_E_KEY_CHALLENGE,
		AU_PACKET_TYPE_INT32, 1, /* isLimited */
		AU_PACKET_TYPE_INT32, 1, /* isProtected */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_char_info, sizeof(uint16_t),
		AU_PACKET_TYPE_INT32, 1, /* TID */
		/* Character Name */
		AU_PACKET_TYPE_CHAR, AP_CHARACTER_MAX_NAME_LENGTH + 1,
		AU_PACKET_TYPE_INT32, 1, /* MaxRegisterChars */
		AU_PACKET_TYPE_INT32, 1, /* SlotIndex */
		AU_PACKET_TYPE_INT32, 1, /* Union Info */
		AU_PACKET_TYPE_INT32, 1, /* Racce Info */
		AU_PACKET_TYPE_INT32, 1, /* Hair Index */
		AU_PACKET_TYPE_INT32, 1, /* Face Index */
		/* New Character Name */
		AU_PACKET_TYPE_CHAR, AP_CHARACTER_MAX_NAME_LENGTH + 1,
		AU_PACKET_TYPE_INT32, 1, /* Is Jump Event Character */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_server_info, sizeof(uint8_t),
		AU_PACKET_TYPE_CHAR, AP_LOGIN_IP_ADDR_SIZE, /* IP Addr */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_version_info, sizeof(uint8_t),
		AU_PACKET_TYPE_UINT32, 1, /* Major Version */
		AU_PACKET_TYPE_UINT32, 1, /* Minor Version */
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_login_add_callback(
	struct ap_login_module * mod,
	enum ap_login_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

boolean ap_login_on_receive(
	struct ap_login_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_login_cb_receive cb = { 0 };
	const char * encrypt_string = NULL;
	const char * account_id = NULL;
	const char * pwd = NULL;
	void * char_info = NULL;
	void * server_info = NULL;
	void * version_info = NULL;
	struct ap_login_char_info ci = { 0 };
	struct ap_login_server_info si = { 0 };
	struct ap_login_version_info vi = { 0 };
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type,
			&encrypt_string,
			&account_id,
			NULL, /* Account ID length */
			&pwd,
			NULL, /* Password length */
			&cb.character_id,
			NULL, /* Server group name */
			&char_info,
			&server_info,
			&cb.login_result,
			&version_info,
			NULL, /* e-Key */
			NULL, /* Challenge number */
			NULL, /* Is limited */
			NULL)) { /* Is Protected */
		return FALSE;
	}
	AU_PACKET_GET_STRING(cb.encrypt_string, encrypt_string);
	AU_PACKET_GET_STRING(cb.account_id, account_id);
	AU_PACKET_GET_STRING(cb.password, pwd);
	if (char_info) {
		const char * cname = NULL;
		const char * ncname = NULL;
		if (!au_packet_get_field(&mod->packet_char_info, 
				FALSE, char_info, 0,
				&ci.tid,
				&cname,
				&ci.max_register_count,
				&ci.slot_index,
				&ci.union_info,
				&ci.race_info,
				&ci.hair_index,
				&ci.face_index,
				&ncname,
				&ci.is_jump_event_char)) {
			return FALSE;
		}
		AU_PACKET_GET_STRING(ci.char_name, cname);
		AU_PACKET_GET_STRING(ci.new_char_name, ncname);
		cb.char_info = &ci;
	}
	if (server_info) {
		const char * ip = NULL;
		if (!au_packet_get_field(&mod->packet_server_info, 
				FALSE, server_info, 0,
				&ip)) {
			return FALSE;
		}
		AU_PACKET_GET_STRING(si.ip_addr, ip);
		cb.server_info = &si;
	}
	if (version_info) {
		if (!au_packet_get_field(&mod->packet_version_info, 
				FALSE, version_info, 0,
				&vi.major,
				&vi.minor)) {
			return FALSE;
		}
		cb.version_info = &vi;
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_LOGIN_CB_RECEIVE, &cb);
}

void ap_login_make_packet(
	struct ap_login_module * mod,
	enum ap_login_packet_type type)
{
	uint8_t ptype = type;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_LOGIN_PACKET_TYPE, 
		&ptype, /* Packet Type */
		NULL, /* EncryptCode */
		NULL, /* AccountID */
		NULL, /* AccountID Length */
		NULL, /* AccountPassword */
		NULL, /* AccountPassword Length */
		NULL, /* lCID */
		NULL, /* World Name; */
		NULL, /* Char Info */
		NULL, /* Server Info */
		NULL, /* lResult */
		NULL, /* version info */
		NULL, /* gamestring */
		NULL, /* challenge number for ekey */
		NULL, /* isLimited */
		NULL); /* isProtected */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_login_make_encrypt_code_packet(
	struct ap_login_module * mod,
	const char * enc_string)
{
	uint8_t type = AP_LOGIN_PACKET_ENCRYPT_CODE;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	char str[AP_LOGIN_ENCRYPT_STRING_SIZE];
	strlcpy(str, enc_string, sizeof(str));
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_LOGIN_PACKET_TYPE, 
		&type, /* Packet Type */
		str, /* EncryptCode */
		NULL, /* AccountID */
		NULL, /* AccountID Length */
		NULL, /* AccountPassword */
		NULL, /* AccountPassword Length */
		NULL, /* lCID */
		NULL, /* World Name; */
		NULL, /* Char Info */
		NULL, /* Server Info */
		NULL, /* lResult */
		NULL, /* version info */
		NULL, /* gamestring */
		NULL, /* challenge number for ekey */
		NULL, /* isLimited */
		NULL); /* isProtected */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_login_make_sign_on_packet(
	struct ap_login_module * mod,
	const char * account_id,
	const char * password)
{
	uint8_t type = AP_LOGIN_PACKET_SIGN_ON;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	char id[AP_LOGIN_MAX_ID_LENGTH + 1];
	char pwd[AP_LOGIN_MAX_PW_LENGTH + 1];
	const char * field_id = NULL;
	const char * field_pwd = NULL;
	if (account_id) {
		strlcpy(id, account_id, sizeof(id));
		field_id = id;
	}
	if (password) {
		strlcpy(pwd, password, sizeof(pwd));
		field_pwd = pwd;
	}
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_LOGIN_PACKET_TYPE, 
		&type, /* Packet Type */
		NULL, /* EncryptCode */
		field_id, /* AccountID */
		NULL, /* AccountID Length */
		field_pwd, /* AccountPassword */
		NULL, /* AccountPassword Length */
		NULL, /* lCID */
		NULL, /* World Name; */
		NULL, /* Char Info */
		NULL, /* Server Info */
		NULL, /* lResult */
		NULL, /* version info */
		NULL, /* gamestring */
		NULL, /* challenge number for ekey */
		NULL, /* isLimited */
		NULL); /* isProtected */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_login_make_union_info_packet(
	struct ap_login_module * mod,
	uint32_t union_type)
{
	uint8_t type = AP_LOGIN_PACKET_UNION_INFO;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * char_info = ap_packet_get_temp_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->packet_char_info, char_info,
		FALSE, NULL, 0,
		NULL, /* TID */
		NULL, /* Character Name */
		NULL, /* Max Register Char */
		NULL, /* Slot Index */
		&union_type,
		NULL, /* Race */
		NULL, /* Hair */
		NULL, /* Face */
		NULL, /* New Character Name */
		NULL); /* Is Jump Event Character */
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_LOGIN_PACKET_TYPE, 
		&type, /* Packet Type */
		NULL, /* EncryptCode */
		NULL, /* AccountID */
		NULL, /* AccountID Length */
		NULL, /* AccountPassword */
		NULL, /* AccountPassword Length */
		NULL, /* lCID */
		NULL, /* World Name; */
		char_info, /* Char Info */
		NULL, /* Server Info */
		NULL, /* lResult */
		NULL, /* version info */
		NULL, /* gamestring */
		NULL, /* challenge number for ekey */
		NULL, /* isLimited */
		NULL); /* isProtected */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}

void ap_login_make_char_name_packet(
	struct ap_login_module * mod,
	const char * char_name,
	uint32_t slot_index,
	uint32_t total_count)
{
	uint8_t type = AP_LOGIN_PACKET_CHARACTER_NAME;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * char_info = ap_packet_get_temp_buffer(mod->ap_packet);
	char cname[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	strlcpy(cname, char_name, sizeof(cname));
	au_packet_make_packet(&mod->packet_char_info, char_info,
		FALSE, NULL, 0,
		NULL, /* TID */
		cname, /* Character Name */
		&total_count, /* Max Register Char */
		&slot_index, /* Slot Index */
		NULL, /* Union */
		NULL, /* Race */
		NULL, /* Hair */
		NULL, /* Face */
		NULL, /* New Character Name */
		NULL); /* Is Jump Event Character */
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_LOGIN_PACKET_TYPE, 
		&type, /* Packet Type */
		NULL, /* EncryptCode */
		NULL, /* AccountID */
		NULL, /* AccountID Length */
		NULL, /* AccountPassword */
		NULL, /* AccountPassword Length */
		NULL, /* lCID */
		NULL, /* World Name; */
		char_info, /* Char Info */
		NULL, /* Server Info */
		NULL, /* lResult */
		NULL, /* version info */
		NULL, /* gamestring */
		NULL, /* challenge number for ekey */
		NULL, /* isLimited */
		NULL); /* isProtected */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}

void ap_login_make_enter_game_packet(
	struct ap_login_module * mod,
	uint32_t character_id,
	const char * character_name,
	const char * server_addr)
{
	uint8_t type = AP_LOGIN_PACKET_ENTER_GAME;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * charinfo = ap_packet_get_temp_buffer(mod->ap_packet);
	void * serverinfo = ap_packet_get_temp_buffer(mod->ap_packet);
	char cname[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	char serveraddr[AP_LOGIN_IP_ADDR_SIZE];
	strlcpy(cname, character_name, sizeof(cname));
	strlcpy(serveraddr, server_addr, sizeof(serveraddr));
	au_packet_make_packet(&mod->packet_char_info, charinfo,
		FALSE, NULL, 0,
		NULL, /* TID */
		character_name, /* Character Name */
		NULL, /* Max Register Char */
		NULL, /* Slot Index */
		NULL, /* Union */
		NULL, /* Race */
		NULL, /* Hair */
		NULL, /* Face */
		NULL, /* New Character Name */
		NULL); /* Is Jump Event Character */
	au_packet_make_packet(&mod->packet_server_info, serverinfo,
		FALSE, NULL, 0,
		serveraddr);
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_LOGIN_PACKET_TYPE, 
		&type, /* Packet Type */
		NULL, /* EncryptCode */
		NULL, /* AccountID */
		NULL, /* AccountID Length */
		NULL, /* AccountPassword */
		NULL, /* AccountPassword Length */
		NULL, /* lCID */
		NULL, /* World Name; */
		charinfo, /* Char Info */
		serverinfo, /* Server Info */
		NULL, /* lResult */
		NULL, /* version info */
		NULL, /* gamestring */
		NULL, /* challenge number for ekey */
		NULL, /* isLimited */
		NULL); /* isProtected */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}

void ap_login_make_new_char_name_packet(
	struct ap_login_module * mod,
	const char * char_name,
	uint32_t slot_index,
	uint32_t total_count)
{
	uint8_t type = AP_LOGIN_PACKET_NEW_CHARACTER_NAME;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * char_info = ap_packet_get_temp_buffer(mod->ap_packet);
	char cname[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	strlcpy(cname, char_name, sizeof(cname));
	au_packet_make_packet(&mod->packet_char_info, char_info,
		FALSE, NULL, 0,
		NULL, /* TID */
		cname, /* Character Name */
		&total_count, /* Max Register Char */
		&slot_index, /* Slot Index */
		NULL, /* Union */
		NULL, /* Race */
		NULL, /* Hair */
		NULL, /* Face */
		NULL, /* New Character Name */
		NULL); /* Is Jump Event Character */
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_LOGIN_PACKET_TYPE, 
		&type, /* Packet Type */
		NULL, /* EncryptCode */
		NULL, /* AccountID */
		NULL, /* AccountID Length */
		NULL, /* AccountPassword */
		NULL, /* AccountPassword Length */
		NULL, /* lCID */
		NULL, /* World Name; */
		char_info, /* Char Info */
		NULL, /* Server Info */
		NULL, /* lResult */
		NULL, /* version info */
		NULL, /* gamestring */
		NULL, /* challenge number for ekey */
		NULL, /* isLimited */
		NULL); /* isProtected */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}

void ap_login_make_login_result_packet(
	struct ap_login_module * mod,
	enum ap_login_result result,
	const char * character_name)
{
	uint8_t type = AP_LOGIN_PACKET_LOGIN_RESULT;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * char_info = ap_packet_get_temp_buffer(mod->ap_packet);
	char cname[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	const char * cnameptr = NULL;
	if (character_name) {
		strlcpy(cname, character_name, sizeof(cname));
		cnameptr = cname;
	}
	au_packet_make_packet(&mod->packet_char_info, char_info,
		FALSE, NULL, 0,
		NULL, /* TID */
		cnameptr, /* Character Name */
		NULL, /* Max Register Char */
		NULL, /* Slot Index */
		NULL, /* Union */
		NULL, /* Race */
		NULL, /* Hair */
		NULL, /* Face */
		NULL, /* New Character Name */
		NULL); /* Is Jump Event Character */
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_LOGIN_PACKET_TYPE, 
		&type, /* Packet Type */
		NULL, /* EncryptCode */
		NULL, /* AccountID */
		NULL, /* AccountID Length */
		NULL, /* AccountPassword */
		NULL, /* AccountPassword Length */
		NULL, /* lCID */
		NULL, /* World Name; */
		char_info, /* Char Info */
		NULL, /* Server Info */
		&result, /* lResult */
		NULL, /* version info */
		NULL, /* gamestring */
		NULL, /* challenge number for ekey */
		NULL, /* isLimited */
		NULL); /* isProtected */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}
