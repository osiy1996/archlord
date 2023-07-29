#ifndef _AS_LOGIN_H_
#define _AS_LOGIN_H_

#include "core/macros.h"
#include "core/types.h"

#include "server/as_account.h"

#define AS_LOGIN_MODULE_NAME "AgsmLogin"

BEGIN_DECLS

struct as_server_conn;
struct as_account;

enum as_login_stage {
	AS_LOGIN_STAGE_ANY,
	AS_LOGIN_STAGE_AWAIT_VERSION,
	AS_LOGIN_STAGE_AWAIT_LOGIN,
	AS_LOGIN_STAGE_LOGGED_IN,
	AS_LOGIN_STAGE_ENTERING_GAME,
	AS_LOGIN_STAGE_DISCONNECTED
};

enum as_login_callback_id {
	AS_LOGIN_CB_LOGGED_IN,
};

struct as_login_conn_ad {
	enum as_login_stage stage;
	struct as_account * account;
	uint32_t character_count;
	struct ap_character * characters[AS_ACCOUNT_MAX_CHARACTER];
};

struct as_login_cb_logged_in {
	const char * account_id;
};

struct as_login_module * as_login_create_module();

boolean as_login_init_presets(struct as_login_module * mod);

void as_login_add_callback(
	struct as_login_module * mod,
	enum as_login_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

struct as_login_conn_ad * as_login_get_attached_conn_data(
	struct as_login_module * mod,
	struct as_server_conn * conn);

void as_login_generate_salt(uint8_t * salt, size_t size);

boolean as_login_hash_password(
	const char * password,
	const uint8_t * salt,
	size_t saltsize,
	uint8_t * hash,
	size_t hashsize);

boolean as_login_confirm_auth_key(
	struct as_login_module * mod,
	const char * character_name,
	uint32_t auth_key);

END_DECLS

#endif /* _AS_LOGIN_H_ */
