#ifndef _AP_RETURN_TO_LOGIN_H_
#define _AP_RETURN_TO_LOGIN_H_

#include "public/ap_login.h"

#define AP_RETURN_TO_LOGIN_MODULE_NAME "AgpmReturnToLogin"

BEGIN_DECLS

enum ap_return_to_login_domain {
	AP_RETURN_TO_LOGIN_DOMAIN_LOGIN,
	AP_RETURN_TO_LOGIN_DOMAIN_GAME,
};

enum ap_return_to_login_callback_id {
	AP_RETURN_TO_LOGIN_CB_RECEIVE,
};

enum ap_return_to_login_packet_type {
	AP_RETURN_TO_LOGIN_PACKET_REQUEST = 1,
	AP_RETURN_TO_LOGIN_PACKET_REQUEST_FAILED,
	AP_RETURN_TO_LOGIN_PACKET_REQUEST_KEY,
	AP_RETURN_TO_LOGIN_PACKET_RESPONSE_KEY,
	AP_RETURN_TO_LOGIN_PACKET_SEND_KEY_ADDR,
	AP_RETURN_TO_LOGIN_PACKET_RECONNECT_LOGIN_SERVER,
	AP_RETURN_TO_LOGIN_PACKET_END_PROCESS,
};

struct ap_return_to_login_cb_receive {
	enum ap_return_to_login_packet_type type;
	enum ap_return_to_login_domain domain;
	char account_id[AP_LOGIN_MAX_ID_LENGTH + 1];
	uint32_t auth_key;
	void * user_data;
};

struct ap_return_to_login_module * ap_return_to_login_create_module();

void ap_return_to_login_add_callback(
	struct ap_return_to_login_module * mod,
	enum ap_return_to_login_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

boolean ap_return_to_login_on_receive(
	struct ap_return_to_login_module * mod,
	const void * data,
	uint16_t length,
	enum ap_return_to_login_domain domain,
	void * user_data);

void ap_return_to_login_make_send_key_addr_packet(
	struct ap_return_to_login_module * mod,
	uint32_t character_id,
	const char * addr,
	uint32_t auth_key);

void ap_return_to_login_make_end_process_packet(
	struct ap_return_to_login_module * mod);

END_DECLS

#endif /* _AP_RETURN_TO_LOGIN_H_ */