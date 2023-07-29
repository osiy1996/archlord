#ifndef _AS_PLAYER_H_
#define _AS_PLAYER_H_

#include "core/macros.h"
#include "core/types.h"

#include "server/as_account.h"

#define AS_PLAYER_MODULE_NAME "AgsmPlayer"

BEGIN_DECLS

struct as_server_conn;

enum as_player_module_data_index {
	AS_PLAYER_MDI_SESSION,
};

enum as_player_callback_id {
	AS_PLAYER_CB_ADD,
	AS_PLAYER_CB_REMOVE,
};

struct as_player_session {
	int dummy;
};

/* 'struct ap_character' attachment. */
struct as_player_character {
	struct as_server_conn * conn;
	struct as_account * account;
	struct as_player_session * session;
};

/** \brief AS_PLAYER_CB_ADD callback data. */
struct as_player_cb_add {
	struct ap_character * character;
};

/** \brief AS_PLAYER_CB_REMOVE callback data. */
struct as_player_cb_remove {
	struct ap_character * character;
};

struct as_player_module * as_player_create_module();

void as_player_add_callback(
	struct as_player_module * mod,
	enum as_player_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t as_player_attach_data(
	struct as_player_module * mod,
	enum as_player_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

struct as_player_character * as_player_get_character_ad(
	struct as_player_module * mod,
	struct ap_character * character);

boolean as_player_add(struct as_player_module * mod, struct ap_character * character);

struct ap_character * as_player_get_by_id(
	struct as_player_module * mod, 
	uint32_t character_id);

struct ap_character * as_player_get_by_name(
	struct as_player_module * mod,
	const char * character_name);

boolean as_player_remove(struct as_player_module * mod, struct ap_character * character);

struct ap_character * as_player_iterate(struct as_player_module * mod, size_t * index);

struct as_player_session * as_player_get_session(
	struct as_player_module * mod,
	const char * character_name);

void as_player_send_packet(
	struct as_player_module * mod,
	struct ap_character * character);

/**
 * Send packet with custom buffer.
 * \param[in,out] character Character pointer.
 * \param[in,out] buffer    Packet data.
 * \param[in]     length    Packet length.
 */
void as_player_send_custom_packet(
	struct as_player_module * mod,
	struct ap_character * character,
	void * buffer,
	uint16_t length);

END_DECLS

#endif /* _AS_PLAYER_H_ */
