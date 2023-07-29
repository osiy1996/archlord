#define strcasecamp _stricmp
#include "vendor/httplib/httplib.h"
#undef ERROR
#undef strcasecmp

#define NO_BOOLEAN
#include "server/as_http_server.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/os.h"
#include "core/string.h"

#include "public/ap_config.h"
#include "public/ap_module_instance.h"

#include <assert.h>
#include <stdlib.h>

struct worker_context {
	char ip[32];
	uint16_t port;
	mutex_t lock;
	const httplib::Request * request;
	char response[8096];
};

struct as_http_server_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	struct worker_context * worker_context;
	boolean pending_request;
};

static int worker(void * arg)
{
	struct worker_context * context = (struct worker_context *)arg;
	httplib::Server srv;
	if (!srv.is_valid())
		return 0;
	srv.set_pre_routing_handler([&](const httplib::Request & req, httplib::Response & res) {
		lock_mutex(context->lock);
		while (context->request) {
			unlock_mutex(context->lock);
			Sleep(100);
			lock_mutex(context->lock);
		}
		context->request = &req;
		unlock_mutex(context->lock);
		while (true) {
			Sleep(100);
			lock_mutex(context->lock);
			if (!context->request) {
				res.set_content(context->response, "text/plain");
				unlock_mutex(context->lock);
				break;
			}
			unlock_mutex(context->lock);
		}
		return httplib::Server::HandlerResponse::Handled;
		});
	if (!srv.listen(context->ip, context->port))
		return 0;
	return 0;
}

static boolean onregister(
	struct as_http_server_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	return TRUE;
}

static boolean oninitialize(struct as_http_server_module * mod)
{
	struct worker_context * context = (struct worker_context *)alloc(sizeof(*context));
	memset(context, 0, sizeof(*context));
	strlcpy(context->ip, ap_config_get(mod->ap_config, "WebServerIP"), 
		sizeof(context->ip));
	context->port = (uint16_t)strtoul(
		ap_config_get(mod->ap_config, "WebServerPort"), NULL, 10);
	context->lock = create_mutex();
	if (!create_thread(worker, context)) {
		ERROR("Failed to create worker thread.");
		return FALSE;
	}
	mod->worker_context = context;
	return TRUE;
}

static void onclose(struct as_http_server_module * mod)
{
}

static void onshutdown(struct as_http_server_module * mod)
{
}

struct as_http_server_module * as_http_server_create_module()
{
	struct as_http_server_module * mod = (struct as_http_server_module *)ap_module_instance_new(AS_HTTP_SERVER_MODULE_NAME,
		sizeof(*mod), (ap_module_instance_register_t)onregister, 
		(ap_module_instance_initialize_t)oninitialize, 
		(ap_module_instance_close_t)onclose, 
		(ap_module_instance_shutdown_t)onshutdown);
	return mod;
}

void as_http_server_add_callback(
	struct as_http_server_module * mod,
	enum as_http_server_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

void as_http_server_poll_requests(struct as_http_server_module * mod)
{
	static struct as_http_server_cb_request cb = { 0 };
	assert(mod->worker_context != NULL);
	lock_mutex(mod->worker_context->lock);
	if (!mod->worker_context->request) {
		unlock_mutex(mod->worker_context->lock);
		return;
	}
	strlcpy(cb.request, mod->worker_context->request->path.c_str(), sizeof(cb.request));
	mod->pending_request = TRUE;
	ap_module_enum_callback(mod, AS_HTTP_SERVER_CB_REQUEST, &cb);
	if (mod->pending_request)
		as_http_server_set_response(mod, "");
}

void as_http_server_get_request_param(
	struct as_http_server_module * mod,
	const char * key,
	char * param,
	size_t maxcount)
{
	assert(mod->worker_context != NULL);
	assert(mod->worker_context->request != NULL);
	std::string paramvalue = mod->worker_context->request->get_param_value(key);
	strlcpy(param, paramvalue.c_str(), maxcount);
}

void as_http_server_set_response(
	struct as_http_server_module * mod,
	const char * response)
{
	assert(mod->worker_context != NULL);
	assert(mod->worker_context->request != NULL);
	assert(mod->pending_request);
	strlcpy(mod->worker_context->response, response, 
		sizeof(mod->worker_context->response));
	mod->worker_context->request = NULL;
	unlock_mutex(mod->worker_context->lock);
	mod->pending_request = FALSE;
}
