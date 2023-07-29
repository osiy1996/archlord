#ifndef _AP_LOGIN_H_
#define _AP_LOGIN_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_character.h"
#include "public/ap_define.h"
#include "public/ap_module_instance.h"

#define AP_LOGIN_MODULE_NAME "AgpmLogin"

#define AP_LOGIN_SERVER_GROUP_NAME_SIZE 32
#define AP_LOGIN_IP_ADDR_SIZE 23
#define AP_LOGIN_E_KEY_SIZE 16
#define AP_LOGIN_E_KEY_CHALLENGE 5
#define AP_LOGIN_J_AUTH_STRING 2048
#define AP_LOGIN_ENCRYPT_STRING_SIZE 32
#define AP_LOGIN_MAX_ID_LENGTH 48
#define AP_LOGIN_MAX_PW_LENGTH 32

BEGIN_DECLS

enum ap_login_packet_type {
	AP_LOGIN_PACKET_ENCRYPT_CODE = 0,
	/* AP_LOGIN_LOGIN_SUCCEEDED */
	AP_LOGIN_PACKET_SIGN_ON,
	/* EKEY */
	AP_LOGIN_PACKET_EKEY,
	/* AP_LOGIN_GET_UNION */
	AP_LOGIN_PACKET_UNION_INFO,
	/* AP_LOGIN_GET_CHARACTERS(S -> C) */
	AP_LOGIN_PACKET_CHARACTER_NAME,
	/* AP_LOGIN_GET_CHARACTER_FINISH(S -> C) */
	AP_LOGIN_PACKET_CHARACTER_NAME_FINISH,
	/* AP_LOGIN_GET_CHARACTERS(C -> S) */
	AP_LOGIN_PACKET_CHARACTER_INFO,
	/* AP_LOGIN_SEND_CHARACTER_FINISH */
	AP_LOGIN_PACKET_CHARACTER_INFO_FINISH,
	/* AP_LOGIN_SELECT_CHARACTER, AP_LOGIN_LOGIN_COMPLETE */
	AP_LOGIN_PACKET_ENTER_GAME,
	/* AP_LOGIN_RETURN_TO_SELECT_SERVER */
	AP_LOGIN_PACKET_RETURN_TO_SELECT_WORLD,
	/* AP_LOGIN_REQUEST_CREATE_CHARACTER_INFO, 
	 * AP_LOGIN_RESPONSE_CREATE_CHARACTER_INFO */
	AP_LOGIN_PACKET_RACE_BASE,
	/* AP_LOGIN_CREATE_CHARACTER */
	AP_LOGIN_PACKET_NEW_CHARACTER_NAME,
	/* AP_LOGIN_SEND_CREATE_CHARACTER_FINISH */
	AP_LOGIN_PACKET_NEW_CHARACTER_INFO_FINISH,
	AP_LOGIN_PACKET_REMOVE_CHARACTER,
	/* AP_LOGIN_UPDATE_CHARACTER */
	AP_LOGIN_PACKET_RENAME_CHARACTER,
	/* common login result code(error) */
	AP_LOGIN_PACKET_LOGIN_RESULT,
	/* only game server, 
	 * AP_LOGIN_REMOVE_DUPLICATE_ACCOUNT,  
	 * AP_LOGIN_REMOVE_DUPLICATE_CHARACTER */
	AP_LOGIN_PACKET_REMOVE_DUPLICATED_ACCOUNT,
	AP_LOGIN_PACKET_COMPENSATION_INFO,
	AP_LOGIN_PACKET_COMPENSATION_CHARACTER_SELECT,
	AP_LOGIN_PACKET_COMPENSATION_CHARACTER_CANCEL,
	AP_LOGIN_PACKET_COUPON_INFO,
	AP_LOGIN_PACKET_CREATE_CHARACTER,
	/* backward compatibility */
	AP_LOGIN_PACKET_INVALID_CLIENT_VERSION 
};

enum ap_login_result {
	AP_LOGIN_RESULT_INVALID_ACCOUNT = 0,
	AP_LOGIN_RESULT_INVALID_PASSWORD,
	AP_LOGIN_RESULT_INVALID_PASSWORD_LIMIT_EXCEED,
	AP_LOGIN_RESULT_NOT_APPLIED,
	AP_LOGIN_RESULT_NOT_ENOUGH_AGE,
	AP_LOGIN_RESULT_NOT_BETA_TESTER,
	AP_LOGIN_RESULT_DISCONNECTED_BY_DUPLICATE_ACCOUNT,
	AP_LOGIN_RESULT_CHAR_NAME_ALREADY_EXIST,
	AP_LOGIN_RESULT_FULL_SLOT,
	AP_LOGIN_RESULT_RENAME_SUCCESS,
	/* unknown fail(DB, server, ...) */
	AP_LOGIN_RESULT_RENAME_FAIL,
	/* old and new id are equi. */
	AP_LOGIN_RESULT_RENAME_SAME_OLD_NEW_ID,
	AP_LOGIN_RESULT_UNMAKABLE_CHAR_NAME,
	/* ekey */
	AP_LOGIN_RESULT_NEED_EKEY,
	AP_LOGIN_RESULT_CANNOT_DISCONNECT_TRY_LATER,
	AP_LOGIN_RESULT_GAMESERVER_NOT_READY,
	AP_LOGIN_RESULT_GAMESERVER_FULL,
	AP_LOGIN_RESULT_ACCOUNT_BLOCKED,
	AP_LOGIN_RESULT_CANT_DELETE_CHAR_B4_1DAY,
	AP_LOGIN_RESULT_CANT_DELETE_CHAR_GUILD_MASTER,
	AP_LOGIN_RESULT_NOT_BETAKEY,
	AP_LOGIN_RESULT_CHECK_CREATE_JUMPING_CHARACTER_SUCCESS,
	AP_LOGIN_RESULT_CHECK_CREATE_JUMPING_CHARACTER_FAIL,
	AP_LOGIN_RESULT_REAL_NAME_AUTH_FAIL,
	AP_LOGIN_RESULT_COUNT
};

enum ap_login_step {
	AP_LOGIN_STEP_NONE = 0,
	AP_LOGIN_STEP_CONNECT,
	AP_LOGIN_STEP_PASSWORD_CHECK_OK,
	AP_LOGIN_STEP_GET_EXIST_CHARACTER,
	AP_LOGIN_STEP_CREATE_CHARACTER,
	AP_LOGIN_STEP_REMOVE_CHARACTER,
	AP_LOGIN_STEP_REQUEST_SELECT_CHARACTER,
	AP_LOGIN_STEP_SELECT_CHARACTER,
	AP_LOGIN_STEP_ENTER_GAME_WORLD,
	AP_LOGIN_STEP_UNKNOWN,
};

enum ap_login_callback_id {
	AP_LOGIN_CB_RECEIVE,
};

struct ap_login_char_info {
	uint32_t tid;
	char char_name[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	uint32_t max_register_count;
	uint32_t slot_index;
	uint32_t union_info;
	uint32_t race_info;
	uint32_t hair_index;
	uint32_t face_index;
	char new_char_name[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	boolean is_jump_event_char;
};

struct ap_login_server_info {
	char ip_addr[AP_LOGIN_IP_ADDR_SIZE];
};

struct ap_login_version_info {
	uint32_t major;
	uint32_t minor;
};

struct ap_login_cb_receive {
	enum ap_login_packet_type type;
	char encrypt_string[AP_LOGIN_ENCRYPT_STRING_SIZE];
	char account_id[AP_LOGIN_MAX_ID_LENGTH + 1];
	char password[AP_LOGIN_MAX_PW_LENGTH + 1];
	uint32_t character_id;
	char server_name[AP_LOGIN_SERVER_GROUP_NAME_SIZE];
	struct ap_login_char_info * char_info;
	struct ap_login_server_info * server_info;
	enum ap_login_result login_result;
	struct ap_login_version_info * version_info;
	void * user_data;
};

struct ap_login_module * ap_login_create_module();

void ap_login_add_callback(
	struct ap_login_module * mod,
	enum ap_login_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

boolean ap_login_on_receive(
	struct ap_login_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_login_make_packet(
	struct ap_login_module * mod,
	enum ap_login_packet_type type);

void ap_login_make_encrypt_code_packet(
	struct ap_login_module * mod,
	const char * enc_string);

void ap_login_make_sign_on_packet(
	struct ap_login_module * mod,
	const char * account_id,
	const char * password);

void ap_login_make_union_info_packet(
	struct ap_login_module * mod,
	uint32_t union_type);

void ap_login_make_char_name_packet(
	struct ap_login_module * mod,
	const char * char_name,
	uint32_t slot_index,
	uint32_t total_count);

void ap_login_make_enter_game_packet(
	struct ap_login_module * mod,
	uint32_t character_id,
	const char * character_name,
	const char * server_addr);

void ap_login_make_new_char_name_packet(
	struct ap_login_module * mod,
	const char * char_name,
	uint32_t slot_index,
	uint32_t total_count);

void ap_login_make_login_result_packet(
	struct ap_login_module * mod,
	enum ap_login_result result,
	const char * character_name);

END_DECLS

#endif /* _AP_LOGIN_H_ */
