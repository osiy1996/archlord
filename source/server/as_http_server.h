#ifndef _AS_HTTP_SERVER_H_
#define _AS_HTTP_SERVER_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module.h"

#define AS_HTTP_SERVER_MODULE_NAME "AgsmHttpServer"

#define AS_HTTP_SERVER_MAX_REQUEST_SIZE 8096

enum as_http_server_callback_id {
	AS_HTTP_SERVER_CB_REQUEST,
};

BEGIN_DECLS

struct as_http_server_cb_request {
	char request[AS_HTTP_SERVER_MAX_REQUEST_SIZE];
};

struct as_http_server_module * as_http_server_create_module();

void as_http_server_add_callback(
	struct as_http_server_module * mod,
	enum as_http_server_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

void as_http_server_poll_requests(struct as_http_server_module * mod);

void as_http_server_get_request_param(
	struct as_http_server_module * mod,
	const char * key,
	char * param,
	size_t maxcount);

void as_http_server_set_response(
	struct as_http_server_module * mod,
	const char * response);

END_DECLS

#endif /* _AS_HTTP_SERVER_H_ */
