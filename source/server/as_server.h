#ifndef _AS_SERVER_H_
#define _AS_SERVER_H_

#include "core/macros.h"
#include "core/types.h"

#include "utility/au_blowfish.h"

#include "public/ap_module.h"

#define AS_SERVER_MODULE_NAME "AgsmServer"

enum as_server_type {
	AS_SERVER_LOGIN,
	AS_SERVER_GAME,
	AS_SERVER_COUNT
};

enum as_server_create_flag_bits {
	AS_SERVER_CREATE_NONE = 0,
	AS_SERVER_CREATE_LOGIN = 1u << 0,
	AS_SERVER_CREATE_GAME = 1u << 1,
};

enum as_server_conn_stage {
	AS_SERVER_CONN_STAGE_AWAIT_PUBLIC_KEY,
	AS_SERVER_CONN_STAGE_AWAIT_PRIVATE_KEY,
	AS_SERVER_CONN_STAGE_READY
};

enum as_server_callback_id {
	AS_SERVER_CB_CONNECT,
	AS_SERVER_CB_DISCONNECT,
	AS_SERVER_CB_RECEIVE,
};

enum as_server_module_data_index {
	AS_SERVER_MDI_CONNECTION,
};

BEGIN_DECLS

struct ring_buffer;

struct as_server_conn {
	uint64_t id;
	enum as_server_type server_type;
	enum as_server_conn_stage stage;
	struct ring_buffer * recv_buffer;
	struct ring_buffer * send_buffer;
	struct au_blowfish blowfish;
	uint64_t connection_tick;
	uint64_t disconnect_tick;
	uint32_t last_processed_frame_tick;
};

struct as_server_cb_receive {
	struct as_server_conn * conn;
	uint8_t packet_type;
	const void * data;
	uint16_t length;
};

struct as_server_module * as_server_create_module();

boolean as_server_create_servers(struct as_server_module * mod, uint32_t flags);

void as_server_poll_server(struct as_server_module * mod);

void as_server_add_callback(
	struct as_server_module * mod,
	enum as_server_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

struct as_server_conn * as_server_get_conn(
	struct as_server_module * mod,
	enum as_server_type server_type,
	uint64_t conn_id);

void as_server_disconnect(
	struct as_server_module * mod, 
	struct as_server_conn * conn);

void as_server_disconnect_by_id(
	struct as_server_module * mod,
	enum ap_server_type server_type, 
	uint64_t conn_id);

void as_server_disconnect_in(
	struct as_server_module * mod,
	struct as_server_conn * conn, 
	uint64_t ms);

void as_server_disconnect_all(struct as_server_module * mod);

void as_server_send_packet(
	struct as_server_module * mod, 
	struct as_server_conn * conn);

/**
 * Send packet with custom buffer.
 * \param[in,out] conn   Connection pointer.
 * \param[in,out] buffer Packet data.
 * \param[in]     length Packet length.
 */
void as_server_send_custom_packet(
	struct as_server_module * mod,
	struct as_server_conn * conn,
	void * buffer,
	uint16_t length);

void as_server_send_packet_by_id(
	struct as_server_module * mod,
	enum as_server_type server_type,
	uint64_t conn_id);

size_t as_server_attach_data(
	struct as_server_module * mod,
	enum as_server_module_data_index data_index,
	size_t size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

uint64_t * as_server_iterate_conn(
	struct as_server_module * mod,
	enum as_server_type type,
	size_t * index,
	struct as_server_conn ** conn);

END_DECLS

#endif /* _AS_SERVER_H_ */
