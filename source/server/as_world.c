#include "core/log.h"
#include "core/malloc.h"

#include "public/ap_world.h"

#include "server/as_login.h"
#include "server/as_server.h"
#include "server/as_world.h"

#define SERVER_NAME "Test"

struct as_world_module {
	struct ap_module_instance instance;
	struct ap_world_module * ap_world;
	struct as_login_module * as_login;
	struct as_server_module * as_server;
};

static boolean cb_receive(struct as_world_module * mod, void * data)
{
	struct ap_world_cb_receive * d = data;
	struct as_server_conn * conn = d->user_data;
	switch (d->type) {
	case AP_WORLD_PACKET_GET_WORLD:
		ap_world_make_result_get_world_all_packet(mod->ap_world,
			"Archlord", SERVER_NAME, AP_WORLD_STATUS_GOOD, 0);
		as_server_send_packet(mod->as_server, conn);
		ap_world_make_result_char_count_packet(mod->ap_world, SERVER_NAME, 
			as_login_get_attached_conn_data(mod->as_login, conn)->character_count);
		as_server_send_packet(mod->as_server, conn);
		return TRUE;
	case AP_WORLD_PACKET_GET_CHAR_COUNT:
		return TRUE;
	default:
		return FALSE;
	}
}

static boolean onregister(
	struct as_world_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_world, AP_WORLD_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_login, AS_LOGIN_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_server, AS_SERVER_MODULE_NAME);
	ap_world_add_callback(mod->ap_world, AP_WORLD_CB_RECEIVE, mod, cb_receive);
	return TRUE;
}

struct as_world_module * as_world_create_module()
{
	struct as_world_module * mod = ap_module_instance_new(AS_WORLD_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	return mod;
}

