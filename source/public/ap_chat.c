#include "public/ap_chat.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_admin.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"

#include <assert.h>
#include <stdlib.h>

struct command {
	ap_module_t module_;
	ap_chat_command_t callback;
};

struct ap_chat_module {
	struct ap_module_instance instance;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
	struct ap_admin cmd_admin;
};

static boolean onregister(
	struct ap_chat_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, 
		AP_PACKET_MODULE_NAME);
	return TRUE;
}

static void onshutdown(struct ap_chat_module * mod)
{
	ap_admin_destroy(&mod->cmd_admin);
}

struct ap_chat_module * ap_chat_create_module()
{
	struct ap_chat_module * mod = ap_module_instance_new(AP_CHAT_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	au_packet_init(&mod->packet, sizeof(uint8_t),
		AU_PACKET_TYPE_UINT8, 1, /* Packet Type */ 
		AU_PACKET_TYPE_INT32, 1, /* CID */
		AU_PACKET_TYPE_INT8, 1, /* Chat Type */
		AU_PACKET_TYPE_INT32, 1, /* Reserved */
		/* Sender ID */
		AU_PACKET_TYPE_CHAR, AP_CHARACTER_MAX_NAME_LENGTH + 1,
		/* Target ID */
		AU_PACKET_TYPE_CHAR, AP_CHARACTER_MAX_NAME_LENGTH + 1,
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* chatting message */
		AU_PACKET_TYPE_END);
	ap_admin_init(&mod->cmd_admin, sizeof(struct command), 128);
	return mod;
}

void ap_chat_add_callback(
	struct ap_chat_module * mod,
	enum ap_chat_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

boolean ap_chat_add_command(
	struct ap_chat_module * mod,
	const char * command,
	ap_module_t module_,
	ap_chat_command_t cmd)
{
	struct command command_ = { module_, cmd };
	void * obj = ap_admin_add_object_by_name(&mod->cmd_admin, command);
	if (!obj) {
		assert(0);
		return FALSE;
	}
	memcpy(obj, &command_, sizeof(command_));
	return TRUE;
}

void ap_chat_exec(
	struct ap_chat_module * mod,
	struct ap_character * character,
	const char * command, 
	uint32_t argc,
	const char * const * argv)
{
	const struct command * obj = 
		ap_admin_get_object_by_name(&mod->cmd_admin, command);
	if (obj)
		obj->callback(obj->module_, character, argc, argv);
}

boolean ap_chat_on_receive(
	struct ap_chat_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_chat_cb_receive cb = { 0 };
	const char * senderid = NULL;
	const char * targetid = NULL;
	const char * msg = NULL;
	uint16_t msglen = 0;
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type,
			&cb.character_id,
			&cb.chat_type,
			NULL, /* Reserved */
			&senderid,
			&targetid,
			&msg, &msglen)) {
		return FALSE;
	}
	if (senderid)
		strlcpy(cb.sender_id, senderid, sizeof(cb.sender_id));
	if (targetid)
		strlcpy(cb.target_id, targetid, sizeof(cb.target_id));
	if (msg) {
		if (msglen >= sizeof(cb.message))
			msglen = sizeof(cb.message) - 1;
		memcpy(cb.message, msg, msglen);
		cb.message[msglen] = '\0';
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_CHAT_CB_RECEIVE, &cb);
}

void ap_chat_make_packet(
	struct ap_chat_module * mod,
	enum ap_chat_packet_type type,
	uint32_t * character_id,
	enum ap_chat_type chat_type,
	const char * sender_id,
	const char * target_id,
	const char * message)
{
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t msglen = (uint16_t)strlen(message);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_CHATTING_PACKET_TYPE, 
		&type,
		character_id, /* Character Id */
		&chat_type, /* Chat Type */
		NULL, /* Reserved */
		sender_id, /* Sender Id */
		target_id, /* Target Id */
		message, &msglen); /* Message */
	ap_packet_set_length(mod->ap_packet, length);
}
