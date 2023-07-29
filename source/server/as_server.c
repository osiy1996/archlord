#include <assert.h>
#include <stdlib.h>

#include "core/log.h"
#include "core/malloc.h"
#include "core/ring_buffer.h"
#include "core/vector.h"

#include <net/net.h>
#include <net/tcp_srv.h>

#include "task/task.h"

#include "utility/au_packet.h"

#include "public/ap_admin.h"
#include "public/ap_config.h"
#include "public/ap_define.h"
#include "public/ap_module.h"
#include "public/ap_packet.h"
#include "public/ap_startup_encryption.h"
#include "public/ap_tick.h"

#include "server/as_server.h"

#define MAX_PACKET_SIZE (1u << 15)

struct srv_module {
	enum as_server_type type;
	struct tcp_srv_state * tcp_server;
	struct ap_admin conn_admin;
};

struct as_server_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	struct ap_packet_module * ap_packet;
	struct ap_startup_encryption_module * ap_startup_encryption;
	struct ap_tick_module * ap_tick;
	struct srv_module * servers[AS_SERVER_COUNT];
	uint64_t * traverse_buffer;
	void * parse_buffer;
};

static struct as_server_conn * find_conn(
	struct srv_module * srv,
	uint64_t id)
{
	struct as_server_conn ** conn = 
		ap_admin_get_object_by_id(&srv->conn_admin, id);
	return conn ? *conn : NULL;
}

static void remove_conn(
	struct as_server_module * mod,
	struct srv_module * srv, 
	struct as_server_conn * conn)
{
	uint64_t id = conn->id;
	ap_module_enum_callback(mod, AS_SERVER_CB_DISCONNECT, conn);
	if (!ap_admin_remove_object_by_id(&srv->conn_admin, id)) {
		ERROR("Failed to remove connection.");
		assert(0);
		return;
	}
	ap_module_destroy_module_data(mod, AS_SERVER_MDI_CONNECTION, conn);
}

static void poll_conn(
	struct as_server_module * mod,
	struct srv_module * srv)
{
	struct tcp_conn_event events[16];
	uint32_t count = tcp_srv_poll_conn(srv->tcp_server,
		events, COUNT_OF(events));
	uint32_t i;
	for (i = 0; i < count; i++) {
		const struct tcp_conn_event * e = &events[i];
		switch (e->type) {
		case TCP_CONN_EVENT_CONNECTED: {
			struct as_server_conn ** conn_ptr;
			struct as_server_conn * conn;
			/*INFO(
				"Connected: Id = 0x%llx, Ip = %s, Port = %u",
				e->id, e->ip, e->port);*/
			if (find_conn(srv, e->id)) {
				ERROR("Connection id clash found (%llX).", e->id);
				break;
			}
			conn_ptr = ap_admin_add_object_by_id(&srv->conn_admin,
				e->id);
			if (!conn_ptr) {
				ERROR("Failed to add connection.");
				break;
			}
			conn = ap_module_create_module_data(mod,
				AS_SERVER_MDI_CONNECTION);
			conn->id = e->id;
			conn->server_type = srv->type;
			*conn_ptr = conn;
			ap_module_enum_callback(mod, 
				AS_SERVER_CB_CONNECT, conn);
			break;
		}
		case TCP_CONN_EVENT_DISCONNECTED: {
			struct as_server_conn * conn = find_conn(srv, e->id);
			/*INFO(
				"Disconnected: Id = 0x%llx", e->id);*/
			if (!conn) {
				/* It is possible for connection to have been 
				 * removed prior to receiving a disconnect event 
				 * due to a forceful disconnect. */
				break;
			}
			remove_conn(mod, srv, conn);
			break;
		}
		}
	}
}

static boolean process_read_buffer(
	struct as_server_module * mod,
	struct srv_module * srv,
	struct as_server_conn * conn)
{
	uint8_t * data = conn->recv_buffer->rcursor;
	size_t usage = conn->recv_buffer->usage;
	uint8_t type = 0;
	uint16_t length = 0;
	if (usage < 12)
		return FALSE;
	length = *(uint16_t *)(data + 1);
	if (length < 12 || length > MAX_PACKET_SIZE)
		return FALSE;
	if (length > usage) {
		/* Haven't received the full packet yet */
		return FALSE;
	}
	data = mod->parse_buffer;
	rb_read(conn->recv_buffer, data, length);
	switch (conn->stage) {
	case AS_SERVER_CONN_STAGE_READY: {
		struct as_server_cb_receive cb = { 0 };
		if (data[0] != AU_PACKET_FRONT_PRIVATE_BYTE)
			return FALSE;
		if (!au_blowfish_decrypt_private(&conn->blowfish, 
				data, &length)) {
			return FALSE;
		}
		conn->last_processed_frame_tick = 
			((struct au_packet_header *)data)->frame_tick;
		cb.conn = conn;
		cb.packet_type = data[3];
		cb.data = data;
		cb.length = length;
		if (!ap_module_enum_callback(mod, AS_SERVER_CB_RECEIVE, &cb)) {
			as_server_disconnect(mod, conn);
			return FALSE;
		}
		break;
	}
	case AS_SERVER_CONN_STAGE_AWAIT_PUBLIC_KEY:
	case AS_SERVER_CONN_STAGE_AWAIT_PRIVATE_KEY:
		if (data[0] != AU_PACKET_FRONT_GUARD_BYTE)
			return FALSE;
		if (data[3] != AP_STARTUP_ENCRYPTION_PACKET_TYPE)
			return FALSE;
		if (!ap_startup_encryption_on_receive(mod->ap_startup_encryption,
				data, length, conn)) {
			as_server_disconnect(mod, conn);
			return FALSE;
		}
		break;
	}
	return TRUE;
}

static boolean cb_startup_enc_receive(struct as_server_module * mod, void * data)
{
	struct ap_startup_encryption_cb_receive * d = data;
	struct as_server_conn * conn = d->user_data;
	switch (d->type) {
	case AP_STARTUP_ENCRYPTION_PACKET_REQUEST_PUBLIC: {
		uint8_t key[] = { 
			0x14, 0x07, 0x0E, 0x29, 0xF4, 0x97, 0x1A, 0x9A, 
			0xDB, 0xC0, 0x30, 0x27, 0xB5, 0xFF, 0xC9, 0xA7, 
			0xFD, 0x60, 0x20, 0x8E, 0xAC, 0xF0, 0x01, 0xBF, 
			0xCC, 0x71, 0x0A, 0xAE, 0x4C, 0xE3, 0x95, 0x49 };
		if (conn->stage != AS_SERVER_CONN_STAGE_AWAIT_PUBLIC_KEY)
			return FALSE;
		ap_startup_encryption_make_packet(mod->ap_startup_encryption,
			AP_STARTUP_ENCRYPTION_PACKET_PUBLIC,
			key, sizeof(key));
		as_server_send_packet(mod, conn);
		au_blowfish_set_public_key(&conn->blowfish, 
			key, sizeof(key));
		conn->stage = AS_SERVER_CONN_STAGE_AWAIT_PRIVATE_KEY;
		return TRUE;
	}
	case AP_STARTUP_ENCRYPTION_PACKET_MAKE_PRIVATE:
		if (conn->stage != AS_SERVER_CONN_STAGE_AWAIT_PRIVATE_KEY)
			return FALSE;
		if (!d->data || d->length != 32)
			return FALSE;
		ap_startup_encryption_make_packet(mod->ap_startup_encryption,
			AP_STARTUP_ENCRYPTION_PACKET_COMPLETE,
			NULL, 0);
		as_server_send_packet(mod, conn);
		au_blowfish_set_private_key(&conn->blowfish, 
			d->data, d->length);
		conn->stage = AS_SERVER_CONN_STAGE_READY;
		return TRUE;
	default:
		return FALSE;
	}
}

static void poll_data(struct as_server_module * mod, struct srv_module * srv)
{
	struct tcp_recv_event * e;
	struct tcp_recv_event * head;
	head = tcp_srv_poll_recv(srv->tcp_server);
	e = head;
	while (e) {
		struct as_server_conn * conn = find_conn(srv, e->id);
		/*INFO("Recv: Id = %llx, Len = %u",
			e->id, e->len);*/
		if (conn) {
			if (rb_avail_write(conn->recv_buffer) < e->len) {
				WARN("Receive buffer overflow.");
				as_server_disconnect(mod, conn);
				continue;
			}
			rb_write(conn->recv_buffer, e->data, e->len);
		}
		e = e->next;
	}
	if (head)
		tcp_srv_free_recv(srv->tcp_server, head);
}

static struct srv_module * create_server(
	struct as_server_module * mod, 
	enum as_server_type type)
{
	struct srv_module * srv = alloc(sizeof(*srv));
	const char * ip;
	const char * port;
	memset(srv, 0, sizeof(*srv));
	/* TODO: IP/port configuration */
	switch (type) {
	case AS_SERVER_LOGIN:
		ip = ap_config_get(mod->ap_config, "LoginServerIP");
		port = ap_config_get(mod->ap_config, "LoginServerPort");
		break;
	case AS_SERVER_GAME:
		ip = ap_config_get(mod->ap_config, "GameServerIP");
		port = ap_config_get(mod->ap_config, "GameServerPort");
		break;
	default:
		return NULL;
	}
	if (!ip || !port) {
		ERROR("Failed to retrieve server IP or port.");
		return NULL;
	}
	srv->type = type;
	srv->tcp_server = 
		tcp_srv_create(ip, (uint16_t)strtoul(port, NULL, 10), 128);
	if (!srv->tcp_server) {
		ERROR("Failed to create tcp server.");
		dealloc(srv);
		return NULL;
	}
	ap_admin_init(&srv->conn_admin, 
		sizeof(struct as_server_conn *), 128);
	return srv;
}

static void destroy_server(struct srv_module * srv)
{
	if (srv->tcp_server)
		tcp_srv_destroy(srv->tcp_server);
	ap_admin_destroy(&srv->conn_admin);
	dealloc(srv);
}

static boolean conn_ctor(struct as_server_module * mod, void * data)
{
	struct as_server_conn * c = data;
	c->stage = AS_SERVER_CONN_STAGE_AWAIT_PUBLIC_KEY;
	c->recv_buffer = rb_create((size_t)1u << 16);
	c->send_buffer = rb_create((size_t)1u << 22);
	return TRUE;
}

static boolean conn_dtor(struct as_server_module * mod, void * data)
{
	struct as_server_conn * c = data;
	rb_destroy(c->recv_buffer);
	rb_destroy(c->send_buffer);
	return TRUE;
}

static boolean onregister(
	struct as_server_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_startup_encryption, AP_STARTUP_ENCRYPTION_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	ap_startup_encryption_add_callback(mod->ap_startup_encryption,
		AP_STARTUP_ENCRYPTION_CB_RECEIVE, mod, cb_startup_enc_receive);
	return TRUE;
}

static void onclose(struct as_server_module * mod)
{
	uint32_t i;
	as_server_disconnect_all(mod);
	for (i = 0; i < AS_SERVER_COUNT; i++) {
		if (mod->servers[i])
			destroy_server(mod->servers[i]);
	}
}

static void onshutdown(struct as_server_module * mod)
{
	vec_free(mod->traverse_buffer);
}

struct as_server_module * as_server_create_module()
{
	const size_t BUFSIZE = (size_t)1u << 20;
	struct as_server_module * mod = ap_module_instance_new(AS_SERVER_MODULE_NAME,
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	ap_module_set_module_data(mod, 
		AS_SERVER_MDI_CONNECTION, sizeof(struct as_server_conn),
		conn_ctor, conn_dtor);
	mod->traverse_buffer = vec_new_reserved(sizeof(uint64_t), 128);
	mod->parse_buffer = alloc(MAX_PACKET_SIZE);
	return mod;
}

boolean as_server_create_servers(struct as_server_module * mod, uint32_t flags)
{
	if (flags & AS_SERVER_CREATE_LOGIN) {
		mod->servers[AS_SERVER_LOGIN] = 
			create_server(mod, AS_SERVER_LOGIN);
		if (!mod->servers[AS_SERVER_LOGIN]) {
			ERROR("Failed to create login server.");
			return FALSE;
		}
	}
	if (flags & AS_SERVER_CREATE_GAME) {
		mod->servers[AS_SERVER_GAME] = 
			create_server(mod, AS_SERVER_GAME);
		if (!mod->servers[AS_SERVER_GAME]) {
			ERROR("Failed to create world server.");
			return FALSE;
		}
	}
	return TRUE;
}

void as_server_poll_server(struct as_server_module * mod)
{
	uint64_t tick = ap_tick_get(mod->ap_tick);
	uint32_t i;
	for (i = 0; i < AS_SERVER_COUNT; i++) {
		struct srv_module * srv = mod->servers[i];
		size_t index = 0;
		uint64_t * id;
		uint32_t count;
		uint32_t j;
		if (!srv)
			continue;
		vec_clear(mod->traverse_buffer);
		id = ap_admin_iterate_id(&srv->conn_admin, &index, NULL);
		while (id) {
			vec_push_back(&mod->traverse_buffer, id);
			id = ap_admin_iterate_id(&srv->conn_admin, &index, NULL);
		}
		count = vec_count(mod->traverse_buffer);
		for (j = 0; j < count; j++) {
			struct as_server_conn ** cptr = 
				ap_admin_get_object_by_id(&srv->conn_admin, 
					mod->traverse_buffer[j]);
			if (cptr) {
				struct as_server_conn * conn = *cptr;
				uint32_t k = 0;
				if (conn->disconnect_tick && 
					tick >= conn->disconnect_tick) {
					as_server_disconnect(mod, conn);
					continue;
				}
				tcp_srv_consume_send(srv->tcp_server, 
					conn->id, conn->send_buffer);
				while (k < 5 && process_read_buffer(mod, srv, conn))
					k++;
			}
		}
		tcp_srv_spawn_tasks(srv->tcp_server);
		task_wait();
		poll_conn(mod, srv);
		poll_data(mod, srv);
	}
}

void as_server_add_callback(
	struct as_server_module * mod,
	enum as_server_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

struct as_server_conn * as_server_get_conn(
	struct as_server_module * mod,
	enum as_server_type server_type,
	uint64_t conn_id)
{
	return find_conn(mod->servers[server_type], conn_id);
}

void as_server_disconnect(
	struct as_server_module * mod, 
	struct as_server_conn * conn)
{
	struct srv_module * srv = mod->servers[conn->server_type];
	assert(srv != NULL);
	tcp_srv_disconnect(srv->tcp_server, conn->id);
	remove_conn(mod, srv, conn);
}

void as_server_disconnect_by_id(
	struct as_server_module * mod,
	enum ap_server_type server_type, 
	uint64_t conn_id)
{
	struct as_server_conn * conn = as_server_get_conn(mod, server_type, conn_id);
	if (conn)
		as_server_disconnect(mod, conn);
}

void as_server_disconnect_in(
	struct as_server_module * mod,
	struct as_server_conn * conn, 
	uint64_t ms)
{
	conn->disconnect_tick = ap_tick_get(mod->ap_tick) + ms;
}

void as_server_disconnect_all(struct as_server_module * mod)
{
	uint32_t i;
	for (i = 0; i < COUNT_OF(mod->servers); i++) {
		struct srv_module * srv = mod->servers[i];
		size_t index = 0;
		uint64_t * id;
		uint32_t count;
		uint32_t j;
		if (!srv)
			continue;
		vec_clear(mod->traverse_buffer);
		id = ap_admin_iterate_id(&srv->conn_admin, &index, NULL);
		while (id) {
			vec_push_back(&mod->traverse_buffer, id);
			id = ap_admin_iterate_id(&srv->conn_admin, &index, 
				NULL);
		}
		count = vec_count(mod->traverse_buffer);
		for (j = 0; j < count; j++) {
			struct as_server_conn ** cptr = 
				ap_admin_get_object_by_id(&srv->conn_admin, 
					mod->traverse_buffer[j]);
			if (cptr) {
				struct as_server_conn * conn = *cptr;
				as_server_disconnect(mod, conn);
			}
		}
	}
}

void as_server_send_packet(
	struct as_server_module * mod, 
	struct as_server_conn * conn)
{
	void * packet = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = ap_packet_get_length(mod->ap_packet);
	void * data;
	((struct au_packet_header *)packet)->frame_tick = 
		conn->last_processed_frame_tick;
	if (conn->stage == AS_SERVER_CONN_STAGE_READY) {
		data = ap_packet_get_buffer_enc(mod->ap_packet);
		memcpy(data, packet, length);
		au_blowfish_encrypt_public(&conn->blowfish, data, &length);
	}
	else {
		data = packet;
	}
	if (rb_avail_write(conn->send_buffer) < length) {
		WARN("Send buffer overflow.");
		conn->disconnect_tick = ap_tick_get(mod->ap_tick);
		return;
	}
	rb_write(conn->send_buffer, data, length);
}

void as_server_send_custom_packet(
	struct as_server_module * mod,
	struct as_server_conn * conn,
	void * buffer,
	uint16_t length)
{
	void * data;
	if (rb_avail_write(conn->send_buffer) < length) {
		WARN("Send buffer overflow.");
		conn->disconnect_tick = ap_tick_get(mod->ap_tick);
		return;
	}
	((struct au_packet_header *)buffer)->frame_tick = 
		conn->last_processed_frame_tick;
	if (conn->stage == AS_SERVER_CONN_STAGE_READY) {
		data = ap_packet_get_buffer_enc(mod->ap_packet);
		memcpy(data, buffer, length);
		au_blowfish_encrypt_public(&conn->blowfish, data, &length);
	}
	else {
		data = buffer;
	}
	rb_write(conn->send_buffer, data, length);
}

void as_server_send_packet_by_id(
	struct as_server_module * mod,
	enum as_server_type server_type,
	uint64_t conn_id)
{
	struct as_server_conn * conn = as_server_get_conn(mod, server_type, conn_id);
	if (conn)
		as_server_send_packet(mod, conn);
}

size_t as_server_attach_data(
	struct as_server_module * mod,
	enum as_server_module_data_index data_index,
	size_t size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, size,
		callback_module, constructor, destructor);
}

uint64_t * as_server_iterate_conn(
	struct as_server_module * mod,
	enum as_server_type type,
	size_t * index,
	struct as_server_conn ** conn)
{
	struct srv_module * srv = mod->servers[type];
	uint64_t * id;
	struct as_server_conn ** c = NULL;
	assert(srv != NULL);
	id = ap_admin_iterate_id(&srv->conn_admin, index, (void **)&c);
	if (!id)
		return NULL;
	*conn = *c;
	return id;
}
