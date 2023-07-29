#include "public/ap_system_message.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_admin.h"
#include "public/ap_character.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"
#include "utility/au_table.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

struct ap_system_message_module {
	struct ap_module_instance instance;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
};

static boolean onregister(
	struct ap_system_message_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	return TRUE;
}

struct ap_system_message_module * ap_system_message_create_module()
{
	struct ap_system_message_module * mod = ap_module_instance_new(AP_SYSTEM_MESSAGE_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint8_t),
		AU_PACKET_TYPE_INT8, 1, /* Operation */
		AU_PACKET_TYPE_INT32, 1, /* System Message Code */
		AU_PACKET_TYPE_INT32, 1, /* Integer Param 1 */
		AU_PACKET_TYPE_INT32, 1, /* Integer Param 2 */
		AU_PACKET_TYPE_CHAR, AP_SYSTEM_MESSAGE_MAX_STRING_LENGTH + 1, /* String Param 1 */
		AU_PACKET_TYPE_CHAR, AP_SYSTEM_MESSAGE_MAX_STRING_LENGTH + 1, /* String Param 2 */
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_system_message_make_packet(
	struct ap_system_message_module * mod,
	enum ap_system_message_code code,
	uint32_t param1,
	uint32_t param2,
	const char * string1,
	const char * string2)
{
	uint8_t type = AP_SYSTEM_MESSAGE_PACKET_SYSTEM_MESSAGE;
	uint8_t reserved0 = 0;
	uint32_t reserved1 = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	char str1[AP_SYSTEM_MESSAGE_MAX_STRING_LENGTH + 1];
	char str2[AP_SYSTEM_MESSAGE_MAX_STRING_LENGTH + 1];
	if (string1)
		strlcpy(str1, string1, sizeof(str1));
	if (string2)
		strlcpy(str2, string2, sizeof(str2));
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_SYSTEM_MESSAGE_PACKET_TYPE,
		&type, /* Operation */
		&code, /* System Message Code */
		(param1 != UINT32_MAX) ? &param1 : NULL, /* Integer Param 1 */
		(param2 != UINT32_MAX) ? &param2 : NULL, /* Integer Param 2 */
		!strisempty(str1) ? str1 : NULL, /* String Param 1 */
		!strisempty(str2) ? str2 : NULL); /* String Param 2 */
	ap_packet_set_length(mod->ap_packet, length);
}
