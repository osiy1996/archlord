#ifndef _NET_TCP_SRV_H_
#define _NET_TCP_SRV_H_

#include <core/macros.h>
#include <core/types.h>

BEGIN_DECLS

struct ring_buffer;
struct tcp_srv_state;

enum tcp_conn_event_type {
	TCP_CONN_EVENT_CONNECTED,
	TCP_CONN_EVENT_DISCONNECTED,
};

struct tcp_conn_event {
	/* Client id. */
	uint64_t id;
	enum tcp_conn_event_type type;
	char ip[32];
	uint16_t port;
};

struct tcp_recv_event {
	/* Client id. */
	uint64_t id;
	void * data;
	uint32_t len;
	struct tcp_recv_event * prev;
	struct tcp_recv_event * next;
};

/*
 * Creates a TCP server.
 */
struct tcp_srv_state * tcp_srv_create(
	const char * addr,
	uint16_t port,
	uint32_t max_conn_count);

/*
 * Closes all sockets and destroys server state.
 */
void tcp_srv_destroy(struct tcp_srv_state * state);

/*
 * Generates tasks that will:
 * - Accept new connections.
 * - Process completed I/O operations.
 * - Issue new I/O operations.
 */
void tcp_srv_spawn_tasks(struct tcp_srv_state * state);

/*
 * Polls connections events.
 * Returns the number of events.
 */
uint32_t tcp_srv_poll_conn(
	struct tcp_srv_state * state,
	struct tcp_conn_event * events,
	uint32_t maxcount);

/*
 * Returns a linked-list containing recv events.
 */
struct tcp_recv_event * tcp_srv_poll_recv(
	struct tcp_srv_state * state);

/*
 * Free recv events returned from ``tcp_srv_poll_recv``.
 */
void tcp_srv_free_recv(
	struct tcp_srv_state * state,
	struct tcp_recv_event * e);

/*
 * Consumes a ring buffer that contains data that 
 * needs to be sent to the client.
 */
void tcp_srv_consume_send(
	struct tcp_srv_state * state,
	uint64_t id,
	struct ring_buffer * rb);

/*
 * Disconnects client.
 *
 * If client id is valid, this function will 
 * produce a disconnect event.
 */
void tcp_srv_disconnect(struct tcp_srv_state * state, uint64_t id);

END_DECLS

#endif /* _NET_TCP_SRV_H_ */
