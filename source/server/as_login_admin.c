#include "core/log.h"
#include "core/malloc.h"

#include "public/ap_define.h"
#include "public/ap_login.h"
#include "public/ap_module.h"
#include "public/ap_return_to_login.h"
#include "public/ap_world.h"

#include "server/as_login_admin.h"
#include "server/as_login.h"
#include "server/as_server.h"

struct as_login_admin_module {
	struct ap_module_instance instance;
	struct ap_login_module * ap_login;
	struct ap_return_to_login_module * ap_return_to_login;
	struct ap_world_module * ap_world;
	struct as_login_module * as_login;
	struct as_server_module * as_server;
	enum as_login_stage req_stage[UINT8_MAX];
};

static boolean cb_receive(
	struct as_login_admin_module * mod,
	struct as_server_cb_receive * data)
{
	struct as_login_conn_ad * ad;
	if (data->conn->server_type != AS_SERVER_LOGIN)
		return TRUE;
	ad = as_login_get_attached_conn_data(mod->as_login, data->conn);
	if (mod->req_stage[data->packet_type] != AS_LOGIN_STAGE_ANY &&
		mod->req_stage[data->packet_type] != ad->stage) {
		return FALSE;
	}
	switch (data->packet_type) {
	case AP_LOGIN_PACKET_TYPE:
		return ap_login_on_receive(mod->ap_login, data->data, data->length, data->conn);
	case AP_RETURNTOLOGIN_PACKET_TYPE:
		return ap_return_to_login_on_receive(mod->ap_return_to_login, data->data, data->length, AP_RETURN_TO_LOGIN_DOMAIN_LOGIN, data->conn);
	case AP_WORLD_PACKET_TYPE:
		return ap_world_on_receive(mod->ap_world, data->data, data->length, data->conn);
	default:
		return FALSE;
	}
}

static boolean onregister(
	struct as_login_admin_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_login, AP_LOGIN_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_return_to_login, AP_RETURN_TO_LOGIN_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_world, AP_WORLD_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_login, AS_LOGIN_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_server, AS_SERVER_MODULE_NAME);
	as_server_add_callback(mod->as_server, AS_SERVER_CB_RECEIVE, mod, cb_receive);
	return TRUE;
}

struct as_login_admin_module * as_login_admin_create_module()
{
	struct as_login_admin_module * mod = ap_module_instance_new(AS_LOGIN_ADMIN_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, NULL);
	uint32_t i;
	for (i = 0; i < COUNT_OF(mod->req_stage); i++)
		mod->req_stage[i] = AS_LOGIN_STAGE_LOGGED_IN;
	mod->req_stage[AP_LOGIN_PACKET_TYPE] = AS_LOGIN_STAGE_ANY;
	mod->req_stage[AP_RETURNTOLOGIN_PACKET_TYPE] = AS_LOGIN_STAGE_AWAIT_VERSION;
	return mod;
}
