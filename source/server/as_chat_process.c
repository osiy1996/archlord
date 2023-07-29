#include "server/as_chat_process.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_chat.h"

#include "server/as_map.h"

#include <assert.h>

struct as_chat_process_module {
	struct ap_module_instance instance;
	struct ap_chat_module * ap_chat;
	struct as_map_module * as_map;
	struct ap_character ** list;
};

static boolean cbreceive(
	struct as_chat_process_module * mod,
	struct ap_chat_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	char argbuf[8][32] = { 0 };
	char * args[8] = { 0 };
	char command[32];
	uint32_t count = 0;
	char * nextarg = NULL;
	char * arg;
	if (cb->type != AP_CHAT_PACKET_TYPE_CHAT) {
		/* Other packet types are ignored.  */
		return TRUE;
	}
	if (cb->message[0] != '/') {
		ap_chat_make_packet(mod->ap_chat, AP_CHAT_PACKET_TYPE_CHAT, &c->id,
			AP_CHAT_TYPE_NORMAL, NULL, NULL, cb->message);
		as_map_broadcast(mod->as_map, c);
		return TRUE;
	}
	arg = strtok_s(cb->message, " ", &nextarg);
	if (!arg)
		return TRUE;
	strlcpy(command, arg, sizeof(command));
	arg = strtok_s(NULL, " ", &nextarg);
	while (arg && count < 8) {
		uint32_t i = count++;
		strlcpy(argbuf[i], arg, 32);
		args[i] = argbuf[i];
		arg = strtok_s(NULL, " ", &nextarg);
	}
	ap_chat_exec(mod->ap_chat, c, command, count, args);
	return TRUE;
}

static boolean onregister(
	struct as_chat_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_chat, AP_CHAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	ap_chat_add_callback(mod->ap_chat, AP_CHAT_CB_RECEIVE, mod, cbreceive);
	return TRUE;
}

static void onshutdown(struct as_chat_process_module * mod)
{
	vec_free(mod->list);
}

struct as_chat_process_module * as_chat_process_create_module()
{
	struct as_chat_process_module * mod = ap_module_instance_new(AS_CHAT_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	mod->list = vec_new_reserved(sizeof(*mod->list), 128);
	return mod;
}
