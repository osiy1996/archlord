#ifndef _AP_CHAT_H_
#define _AP_CHAT_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_character.h"
#include "public/ap_define.h"
#include "public/ap_module_instance.h"

#define AP_CHAT_MODULE_NAME "AgpmChatting"

BEGIN_DECLS

typedef void (*ap_chat_command_t)(
	ap_module_t module_,
	struct ap_character * character,
	uint32_t argc,
	const char * const * argv);

enum ap_chat_packet_type {
	AP_CHAT_PACKET_TYPE_CHAT = 0,
	AP_CHAT_PACKET_TYPE_SET_BLOCK_WHISPER,
	AP_CHAT_PACKET_TYPE_RELEASE_BLOCK_WHISPER,
	AP_CHAT_PACKET_TYPE_REPLY_BLOCK_WHISPER,
	AP_CHAT_PACKET_TYPE_OFFLINE_WHISPER_TARGET,
};

enum ap_chat_type {
	AP_CHAT_TYPE_NORMAL = 0,
	AP_CHAT_TYPE_WORD_BALLOON,
	AP_CHAT_TYPE_UNION,
	AP_CHAT_TYPE_GUILD,
	AP_CHAT_TYPE_PARTY,
	AP_CHAT_TYPE_WHISPER,
	AP_CHAT_TYPE_WHISPER2,
	AP_CHAT_TYPE_WHOLE_WORLD,
	AP_CHAT_TYPE_SYSTEM_LEVEL1,
	AP_CHAT_TYPE_SYSTEM_LEVEL2,
	AP_CHAT_TYPE_SYSTEM_LEVEL3,
	AP_CHAT_TYPE_NOTICE_LEVEL1,
	AP_CHAT_TYPE_NOTICE_LEVEL2,
	AP_CHAT_TYPE_GUILD_NOTICE,
	AP_CHAT_TYPE_GUILD_JOINT,
	AP_CHAT_TYPE_EMPHASIS,
	AP_CHAT_TYPE_SIEGEWARINFO,
	AP_CHAT_TYPE_SYSTEM_NO_PREFIX,
	AP_CHAT_TYPE_RESERVED,
	AP_CHAT_TYPE_RACE,
	AP_CHAT_TYPE_SCREAM,
	AP_CHAT_TYPE_COUNT
};

enum ap_chat_callback_id {
	AP_CHAT_CB_RECEIVE,
};

struct ap_chat_cb_receive {
	enum ap_chat_packet_type type;
	uint32_t character_id;
	enum ap_chat_type chat_type;
	char sender_id[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	char target_id[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	char message[128];
	void * user_data;
};

struct ap_chat_module * ap_chat_create_module();

void ap_chat_add_callback(
	struct ap_chat_module * mod,
	enum ap_chat_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

boolean ap_chat_add_command(
	struct ap_chat_module * mod,
	const char * command,
	ap_module_t module_,
	ap_chat_command_t cmd);

void ap_chat_exec(
	struct ap_chat_module * mod,
	struct ap_character * character,
	const char * command, 
	uint32_t argc,
	const char * const * argv);

boolean ap_chat_on_receive(
	struct ap_chat_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_chat_make_packet(
	struct ap_chat_module * mod,
	enum ap_chat_packet_type type,
	uint32_t * character_id,
	enum ap_chat_type chat_type,
	const char * sender_id,
	const char * target_id,
	const char * message);

END_DECLS

#endif /* _AP_CHAT_H_ */
