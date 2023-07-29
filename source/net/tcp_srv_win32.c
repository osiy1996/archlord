#include <net/tcp_srv.h>
#include <net/internal.h>
#include <core/malloc.h>
#include <core/log.h>
#include <core/vector.h>
#include <core/string.h>
#include <core/ring_buffer.h>
#include <task/task.h>
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <inttypes.h>

#define CLIENT_SEND_RB_SIZE (1u << 19)

enum io_type {
	IO_READ,
	IO_WRITE,
	IO_INVALID,
};

struct client_ctx;
struct buffer_ctx;

struct buffer_ctx {
	WSAOVERLAPPED overlapped;
	enum io_type type;
	uint64_t client_id;
	char * data;
	struct buffer_ctx * next;
};

struct client_ctx {
	uint64_t id;
	ULONG ip;
	SOCKET sock;
	LONG in_use;
	LONG refcount;
	LONG should_dc;
	boolean is_sending;
	struct ring_buffer * send_rb;
	CRITICAL_SECTION send_lock;
	HANDLE comp_port;
};

struct task_accept_conn_data {
	struct task_descriptor desc;
	struct tcp_srv_state * state;
};

struct task_process_conn_data {
	struct task_descriptor desc;
	struct tcp_srv_state * state;
	uint64_t client_id;
	struct buffer_ctx * freelist;
};

struct tcp_srv_state {
	HANDLE comp_port;
	SOCKET listen_sock;
	HANDLE listen_event;
	CRITICAL_SECTION io_lock;
	CRITICAL_SECTION free_buffer_lock;
	boolean accept_conn;
	uint32_t max_conn_per_ip;
	uint32_t max_conn_count;
	uint32_t conn_count;
	size_t buffer_size;
	struct client_ctx * clients;
	CRITICAL_SECTION conn_lock;
	struct tcp_conn_event * conn_events;
	uint64_t * dc_events;
	CRITICAL_SECTION recv_lock;
	struct tcp_recv_event * recv_events;
	CRITICAL_SECTION recv_freelist_lock;
	struct tcp_recv_event * recv_freelist;
	struct task_process_conn_data * task_data;
	struct task_accept_conn_data task_accept;
};

static void linger_and_close(SOCKET sock)
{
	LINGER linger = { 1, 0 };
	setsockopt(sock, SOL_SOCKET, SO_LINGER,
		(char *)&linger, sizeof(LINGER));
	closesocket(sock);
}

static void free_state(struct tcp_srv_state * st)
{
	dealloc(st);
}

static struct buffer_ctx * get_buffer(
	struct tcp_srv_state * st,
	struct buffer_ctx ** head,
	uint64_t client_id,
	enum io_type type)
{
	struct buffer_ctx * buf = *head;
	if (buf) {
		*head = buf->next;
		memset(buf, 0, sizeof(*buf));
		buf->client_id = client_id;
		buf->type = type;
		buf->next = NULL;
		buf->data = (char *)((uintptr_t)buf + sizeof(*buf));
		return buf;
	}
	buf = alloc(sizeof(*buf) + st->buffer_size);
	memset(buf, 0, sizeof(*buf));
	buf->client_id = client_id;
	buf->type = type;
	buf->data = (char *)((uintptr_t)buf + sizeof(*buf));
	return buf;
}

static void release_buffer(
	struct buffer_ctx ** head,
	struct buffer_ctx * buf)
{
	buf->next = *head;
	*head = buf;
}

static void insert_conn_event(
	struct tcp_srv_state * st,
	uint64_t id,
	enum tcp_conn_event_type type,
	const char * ip,
	uint16_t port)
{
	struct tcp_conn_event e;
	memset(&e, 0, sizeof(e));
	e.id = id;
	e.type = type;
	strlcpy(e.ip, ip, sizeof(e.ip));
	e.port = port;
	EnterCriticalSection(&st->conn_lock);
	vec_push_back(&st->conn_events, &e);
	LeaveCriticalSection(&st->conn_lock);
}

static uint64_t client_get_id(struct client_ctx * ctx)
{
	return InterlockedCompareExchange64(&ctx->id, 0, 0);
}

static void client_set_id(struct client_ctx * ctx, uint64_t id)
{
	InterlockedExchange64(&ctx->id, id);
}

static struct client_ctx * client_from_id(
	struct tcp_srv_state * st,
	uint64_t id)
{
	uint32_t index = (uint32_t)id;
	return &st->clients[index];
}

static void client_next_seq(struct client_ctx * ctx)
{
	uint64_t id = client_get_id(ctx);
	uint32_t seq = (uint32_t)(id >> 32) + 1;
	client_set_id(ctx, ((uint64_t)seq << 32) | (uint32_t)id);
	InterlockedExchange(&ctx->refcount, (LONG)(seq << 16));
}

static void client_release(struct client_ctx * ctx)
{
	ctx->ip = 0;
	if (ctx->sock != INVALID_SOCKET) {
		closesocket(ctx->sock);
		ctx->sock = INVALID_SOCKET;
	}
	client_next_seq(ctx);
	rb_reset(ctx->send_rb);
	InterlockedExchange(&ctx->should_dc, FALSE);
	InterlockedExchange(&ctx->is_sending, FALSE);
	InterlockedExchange(&ctx->in_use, FALSE);
}

static void client_ref(struct client_ctx * ctx)
{
	InterlockedIncrement(&ctx->refcount);
}

static boolean client_ref_with_id(
	struct client_ctx * ctx,
	uint64_t id)
{
	uint32_t seq = (uint32_t)(id >> 32);
	LONG rc = InterlockedCompareExchange(&ctx->refcount, 0, 0);
	while (seq == (uint32_t)(rc >> 16) &&
		(rc & 0xFFFF) < 0xFFFF) {
		LONG rcwas = InterlockedCompareExchange(&ctx->refcount,
			rc + 1, rc);
		if (rcwas == rc)
			return TRUE;
		rc = rcwas;
	}
	return FALSE;
}

static void client_deref(
	struct tcp_srv_state * st,
	struct client_ctx * ctx)
{
	if (!(InterlockedDecrement(&ctx->refcount) & 0xffff)) {
		insert_conn_event(st, client_get_id(ctx),
			TCP_CONN_EVENT_DISCONNECTED, "", 0);
		client_release(ctx);
		InterlockedDecrement(&st->conn_count);
	}
}

static void post_recv(
	struct tcp_srv_state * st,
	struct client_ctx * ctx,
	struct buffer_ctx ** buf_head)
{
	struct buffer_ctx * buf = get_buffer(st, buf_head, 
		client_get_id(ctx), IO_READ);
	DWORD flags = 0;
	WSABUF wsabuf = {
		.buf = buf->data,
		.len = (ULONG)st->buffer_size };
	client_ref(ctx);
	if (WSARecv(ctx->sock, &wsabuf, 1, NULL, &flags,
		&buf->overlapped, NULL) == SOCKET_ERROR &&
		WSAGetLastError() != WSA_IO_PENDING) {
		release_buffer(buf_head, buf);
		InterlockedExchange(&ctx->should_dc, TRUE);
		client_deref(st, ctx);
	}
}

static void post_send(
	struct tcp_srv_state * st,
	struct client_ctx * ctx,
	struct buffer_ctx ** buf_head)
{
	struct buffer_ctx * buf = get_buffer(st, buf_head, 
		client_get_id(ctx), IO_WRITE);
	WSABUF wsabuf = { 0, NULL };
	EnterCriticalSection(&ctx->send_lock);
	wsabuf.buf = buf->data;
	wsabuf.len = (ULONG)rb_read_tmp(ctx->send_rb, wsabuf.buf,
		st->buffer_size);
	LeaveCriticalSection(&ctx->send_lock);
	if (!wsabuf.len) {
		release_buffer(buf_head, buf);
		return;
	}
	client_ref(ctx);
	if (WSASend(ctx->sock, &wsabuf, 1, NULL, 0,
		&buf->overlapped, NULL) == SOCKET_ERROR &&
		WSAGetLastError() != WSA_IO_PENDING) {
		release_buffer(buf_head, buf);
		InterlockedExchange(&ctx->should_dc, TRUE);
		client_deref(st, ctx);
	}
	else {
		ctx->is_sending = TRUE;
	}
}

static struct tcp_recv_event * get_recv_event(
	struct tcp_srv_state * st)
{
	struct tcp_recv_event * e;
	EnterCriticalSection(&st->recv_freelist_lock);
	e = st->recv_freelist;
	if (!e) {
		LeaveCriticalSection(&st->recv_freelist_lock);
		e = alloc(sizeof(*e) + st->buffer_size);
		memset(e, 0, sizeof(*e));
		e->data = (void *)((uintptr_t)e + sizeof(*e));
		return e;
	}
	st->recv_freelist = e->next;
	if (e->next)
		e->next->prev = NULL;
	e->next = NULL;
	LeaveCriticalSection(&st->recv_freelist_lock);
	return e;
}

static void free_recv_event(
	struct tcp_srv_state * st,
	struct tcp_recv_event * e)
{
	EnterCriticalSection(&st->recv_freelist_lock);
	if (st->recv_freelist)
		st->recv_freelist->prev = e;
	e->prev = NULL;
	e->next = st->recv_freelist;
	st->recv_freelist = e;
	LeaveCriticalSection(&st->recv_freelist_lock);
}

static void insert_recv_event(
	struct tcp_srv_state * st,
	struct tcp_recv_event * e)
{
	EnterCriticalSection(&st->recv_lock);
	if (st->recv_events)
		st->recv_events->prev = e;
	e->prev = NULL;
	e->next = st->recv_events;
	st->recv_events = e;
	LeaveCriticalSection(&st->recv_lock);
}

static void proc_read(
	struct tcp_srv_state * st,
	struct client_ctx * ctx,
	struct buffer_ctx * buf,
	struct buffer_ctx ** buf_head,
	DWORD count)
{
	boolean dc = FALSE;
	if (count) {
		struct tcp_recv_event * e = get_recv_event(st);
		e->id = client_get_id(ctx);
		memcpy(e->data, buf->data, count);
		e->len = count;
		insert_recv_event(st, e);
	}
	else {
		InterlockedExchange(&ctx->should_dc, TRUE);
		dc = TRUE;
	}
	if (!dc &&
		!InterlockedCompareExchange(&ctx->should_dc, 0, 0))
		post_recv(st, ctx, buf_head);
}

static void proc_write(
	struct tcp_srv_state * st,
	struct client_ctx * ctx,
	DWORD count)
{
	EnterCriticalSection(&ctx->send_lock);
	rb_forward_read(ctx->send_rb, (size_t)count);
	LeaveCriticalSection(&ctx->send_lock);
	ctx->is_sending = FALSE;
}

static struct client_ctx * get_idle_client(
	struct tcp_srv_state * st)
{
	uint32_t i;
	for (i = 0; i < st->max_conn_count; i++) {
		if (!InterlockedCompareExchange(
			&st->clients[i].in_use, TRUE, FALSE))
			return &st->clients[i];
	}
	return NULL;
}

static uint32_t count_same_ip(struct tcp_srv_state * st, ULONG ip)
{
	uint32_t i;
	uint32_t c = 0;
	for (i = 0; i < st->max_conn_count; i++) {
		if (InterlockedCompareExchange(
			&st->clients[i].in_use, FALSE, FALSE) &&
			st->clients[i].ip == ip)
			c++;
	}
	return c;
}

static size_t get_client_size(size_t rb_size)
{
	return (sizeof(struct client_ctx) + rb_req_size(rb_size));
}

static boolean task_accept_conn(void * data)
{
	struct task_accept_conn_data * task = data;
	struct tcp_srv_state * st = task->state;
	struct buffer_ctx * buf_head = NULL;
	uint32_t count = 0;
	while (count++ < 10) {
		WSANETWORKEVENTS events;
		int r;
		struct client_ctx * ctx;
		SOCKET sock;
		SOCKADDR_IN sa;
		int len = sizeof(SOCKADDR_IN);
		const char opt = 1;
		ULONG ip = 0;
		char ipstr[32] = "";
		r = WSAWaitForMultipleEvents(1, &st->listen_event, 
			FALSE, 0, FALSE);
		if (r == WSA_WAIT_TIMEOUT /*|| ret == WSA_WAIT_FAILED*/) {
			//printf("Total bytes: received=%d, sent=%d\n", InterlockedCompareExchange(&s->total_received, 0, 0), InterlockedCompareExchange(&s->total_sent, 0, 0));
			break;
		}
		if (WSAEnumNetworkEvents(st->listen_sock,
			st->listen_event, &events) == SOCKET_ERROR) {
			ERROR("WSAEnumNetworkEvents() failed, WSAGetLastError()=%d",
				WSAGetLastError());
			break;
		}
		if (!(events.lNetworkEvents & FD_ACCEPT) ||
			!InterlockedCompareExchange(&st->accept_conn, 0, 0))
			continue;
		if (events.iErrorCode[FD_ACCEPT_BIT]) {
			ERROR("Unexpected accept error: %d",
				events.iErrorCode[FD_ACCEPT_BIT]);
			break;
		}
		memset(&sa, 0, sizeof(sa));
		sock = WSAAccept(st->listen_sock,
			(struct sockaddr *)&sa, &len, NULL, (DWORD_PTR)NULL);
		if (sock == INVALID_SOCKET) {
			r = WSAGetLastError();
			if (r != WSAEWOULDBLOCK)
				ERROR("Failed to establish new connection, WSAGetLastError() = %d", r);
			continue;
		}
		if (sa.sin_family != AF_INET) {
			ERROR("Unsupported address family: %d",
				sa.sin_family);
			linger_and_close(sock);
			continue;
		}
		if (InterlockedCompareExchange(&st->conn_count, 0, 0) >= (LONG)st->max_conn_count) {
			WARN("Reached maximum number of connections.");
			linger_and_close(sock);
			continue;
		}
		ip = sa.sin_addr.s_addr;
		if (count_same_ip(st, ip) >= st->max_conn_per_ip) {
			linger_and_close(sock);
			continue;
		}
		ctx = get_idle_client(st);
		if (!ctx) {
			linger_and_close(sock);
			continue;
		}
		ctx->sock = sock;
		ctx->ip = ip;
		if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
			&opt, sizeof(char)) == SOCKET_ERROR) {
			linger_and_close(ctx->sock);
			ctx->sock = INVALID_SOCKET;
			client_release(ctx);
			continue;
		}
		ctx->comp_port = CreateIoCompletionPort((HANDLE)sock,
			NULL, (ULONG_PTR)NULL, 0);
		if (!ctx->comp_port) {
			linger_and_close(ctx->sock);
			ctx->sock = INVALID_SOCKET;
			client_release(ctx);
			continue;
		}
		client_ref(ctx);
		InterlockedIncrement(&st->conn_count);
		inet_ntop(sa.sin_family, &sa.sin_addr,
			ipstr, sizeof(ipstr));
		insert_conn_event(st, client_get_id(ctx),
			TCP_CONN_EVENT_CONNECTED,
			ipstr, sa.sin_port);
		post_recv(st, ctx, &buf_head);
		client_deref(st, ctx);
	}
	return TRUE;
}

static boolean task_process_conn(void * data)
{
	struct task_process_conn_data * task = data;
	struct tcp_srv_state * st = task->state;
	struct client_ctx * client_ctx = client_from_id(st,
		task->client_id);
	uint32_t i;
	if (!client_ref_with_id(client_ctx, task->client_id))
		return TRUE;
	for (i = 0; i < 2; i++) {
		DWORD count = 0;
		struct buffer_ctx * buf_ctx = NULL;
		int ret;
		OVERLAPPED * overlapped = NULL;
		ULONG_PTR _unused;
		ret = GetQueuedCompletionStatus(client_ctx->comp_port,
			&count, &_unused, &overlapped, 0);
		if (!ret) {
			DWORD io_error = GetLastError();
			if (io_error == WAIT_TIMEOUT)
				break;
			ERROR("GetQueuedCompletionStatus() failed, GetLastError() = %d",
				io_error);
			if (overlapped) {
				buf_ctx = CONTAINING_RECORD(overlapped, 
					struct buffer_ctx, overlapped);
				if (buf_ctx)
					release_buffer(&task->freelist, buf_ctx);
				client_deref(st, client_ctx);
			}
			break;
		}
		if (overlapped) {
			buf_ctx = CONTAINING_RECORD(overlapped, 
				struct buffer_ctx, overlapped);
			switch (buf_ctx->type) {
			case IO_READ:
				proc_read(st, client_ctx, buf_ctx,
					&task->freelist, count);
				break;
			case IO_WRITE:
				proc_write(st, client_ctx, count);
				break;
			}
			release_buffer(&task->freelist, buf_ctx);
		}
		client_deref(st, client_ctx);
	}
	if (!client_ctx->is_sending)
		post_send(st, client_ctx, &task->freelist);
	client_deref(st, client_ctx);
	return TRUE;
}

struct tcp_srv_state * tcp_srv_create(
	const char * addr,
	uint16_t port,
	uint32_t max_conn_count)
{
	struct tcp_srv_state * st;
	int r;
	SOCKADDR_IN addrin;
	uint32_t i;
	st = alloc(sizeof(*st) + 
		max_conn_count * get_client_size(CLIENT_SEND_RB_SIZE));
	memset(st, 0, sizeof(*st));
	st->conn_events = vec_new(sizeof(*st->conn_events));
	st->dc_events = vec_new(sizeof(*st->dc_events));
	st->clients = (struct client_ctx *)(
		(uintptr_t)st + sizeof(*st));
	st->accept_conn = TRUE;
	st->task_data = alloc(
		max_conn_count * sizeof(struct task_process_conn_data));
	memset(st->task_data, 0,
		max_conn_count * sizeof(struct task_process_conn_data));
	InitializeCriticalSection(&st->io_lock);
	InitializeCriticalSection(&st->free_buffer_lock);
	InitializeCriticalSection(&st->conn_lock);
	InitializeCriticalSection(&st->recv_lock);
	InitializeCriticalSection(&st->recv_freelist_lock);
	memset(st->clients, 0,
		max_conn_count * get_client_size(CLIENT_SEND_RB_SIZE));
	/* Initialize accept conn. task */
	st->task_accept.desc.work_cb = task_accept_conn;
	st->task_accept.desc.data = &st->task_accept;
	st->task_accept.state = st;
	for (i = 0; i < max_conn_count; i++) {
		/* Initialize task data */
		struct task_process_conn_data * task = &st->task_data[i];
		task->desc.work_cb = task_process_conn;
		task->desc.data = task;
		task->state = st;
		/* Initialize client context */
		st->clients[i].id = i;
		client_next_seq(&st->clients[i]);
		st->clients[i].sock = INVALID_SOCKET;
		st->clients[i].send_rb = rb_create_prealloc(
			(void *)((uintptr_t)&st->clients[max_conn_count] +
				i * rb_req_size(CLIENT_SEND_RB_SIZE)),
			CLIENT_SEND_RB_SIZE);
		InitializeCriticalSection(&st->clients[i].send_lock);
	}
	st->comp_port = CreateIoCompletionPort(
		INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)NULL, 0);
	st->listen_sock = WSASocket(AF_INET, SOCK_STREAM, 
		IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
	if (st->listen_sock == INVALID_SOCKET) {
		ERROR("Failed to create listener socket, WSAGetLastError() = %d",
			WSAGetLastError());
		free_state(st);
		return NULL;
	}
	st->listen_event = WSACreateEvent();
	if (st->listen_event == WSA_INVALID_EVENT) {
		ERROR("Failed to create listener event, WSAGetLastError() = %d",
			WSAGetLastError());
		free_state(st);
		return NULL;
	}
	r = WSAEventSelect(st->listen_sock, st->listen_event,
		FD_ACCEPT);
	if (r == SOCKET_ERROR) {
		ERROR("WSAEventSelect() failed, WSAGetLastError() = %d",
			WSAGetLastError());
		free_state(st);
		return NULL;
	}
	memset(&addrin, 0, sizeof(addrin));
	addrin.sin_family = AF_INET;
	addrin.sin_addr.S_un.S_addr = inet_addr(addr);
	addrin.sin_port = htons(port);
	if (bind(st->listen_sock, (struct sockaddr *)&addrin,
		sizeof(struct sockaddr)) == SOCKET_ERROR) {
		ERROR("Failed to bind socket, WSAGetLastError() = %d",
			WSAGetLastError());
		free_state(st);
		return NULL;
	}
	if (listen(st->listen_sock, 32) == SOCKET_ERROR) {
		ERROR("Failed to listen socket, WSAGetLastError() = %d",
			WSAGetLastError());
		free_state(st);
		return NULL;
	}
	st->max_conn_count = max_conn_count;
	st->max_conn_per_ip = 10;
	st->conn_count = 0;
	st->buffer_size = (size_t)1u << 13;
	return st;
}

void tcp_srv_destroy(struct tcp_srv_state * state)
{
	uint32_t i;
	closesocket(state->listen_sock);
	WSACloseEvent(state->listen_event);
	for (i = 0; i < state->max_conn_count; i++) {
		struct client_ctx * c = &state->clients[i];
		while (InterlockedCompareExchange(&c->in_use, 
				FALSE, FALSE)) {
			client_deref(state, c);
		}
		DeleteCriticalSection(&c->send_lock);
	}
	DeleteCriticalSection(&state->io_lock);
	DeleteCriticalSection(&state->free_buffer_lock);
	DeleteCriticalSection(&state->conn_lock);
	DeleteCriticalSection(&state->recv_lock);
	DeleteCriticalSection(&state->recv_freelist_lock);
	vec_free(state->conn_events);
	vec_free(state->dc_events);
	dealloc(state->task_data);
	dealloc(state);
}

void tcp_srv_spawn_tasks(struct tcp_srv_state * state)
{
	uint32_t i;
	uint32_t count = 0;
	task_add(&state->task_accept.desc, FALSE);
	for (i = 0; i < state->max_conn_count; i++) {
		struct client_ctx * cc =&state->clients[i];
		if (InterlockedCompareExchange(&cc->in_use, FALSE, FALSE)) {
			struct task_process_conn_data * task =
				&state->task_data[count++];
			task->client_id = client_get_id(cc);
			task_add(&task->desc, FALSE);
		}
	}
}

uint32_t tcp_srv_poll_conn(
	struct tcp_srv_state * state,
	struct tcp_conn_event * events,
	uint32_t maxcount)
{
	uint32_t c;
	EnterCriticalSection(&state->conn_lock);
	c = vec_count(state->conn_events);
	if (c > maxcount)
		c = maxcount;
	memcpy(events, state->conn_events, c * sizeof(*events));
	vec_erase_chunk(state->conn_events, 0, c);
	LeaveCriticalSection(&state->conn_lock);
	return c;
}

struct tcp_recv_event * tcp_srv_poll_recv(
	struct tcp_srv_state * state)
{
	struct tcp_recv_event * e;
	EnterCriticalSection(&state->recv_lock);
	e = state->recv_events;
	state->recv_events = NULL;
	LeaveCriticalSection(&state->recv_lock);
	return e;
}

void tcp_srv_free_recv(
	struct tcp_srv_state * state,
	struct tcp_recv_event * e)
{
	if (!e)
		return;
	/* We want to link the last event in `e` to first 
	 * event in `recv_event_freelist`. */
	struct tcp_recv_event * first = e;
	struct tcp_recv_event * last = NULL;
	while (e) {
		last = e;
		e = e->next;
	}
	EnterCriticalSection(&state->recv_freelist_lock);
	last->next = state->recv_freelist;
	if (state->recv_freelist)
		state->recv_freelist->prev = last;
	state->recv_freelist = e;
	LeaveCriticalSection(&state->recv_freelist_lock);
}

void tcp_srv_consume_send(
	struct tcp_srv_state * state,
	uint64_t id,
	struct ring_buffer * rb)
{
	uint32_t index = (uint32_t)id;
	struct client_ctx * ctx;
	if (index >= state->max_conn_count) {
		ERROR("Invalid id: %08X", index);
		return;
	}
	ctx = &state->clients[index];
	if (!client_ref_with_id(ctx, id)) {
		WARN("Found a stale connection.");
		return;
	}
	EnterCriticalSection(&ctx->send_lock);
	rb_consume_other(ctx->send_rb, rb);
	LeaveCriticalSection(&ctx->send_lock);
	client_deref(state, ctx);
}

void tcp_srv_disconnect(struct tcp_srv_state * state, uint64_t id)
{
	uint32_t index = (uint32_t)id;
	struct client_ctx * ctx;
	if (index >= state->max_conn_count) {
		ERROR("Invalid id: %08X", index);
		return;
	}
	ctx = &state->clients[index];
	if (!client_ref_with_id(ctx, id)) {
		WARN("Found a stale connection.");
		return;
	}
	while (InterlockedCompareExchange(&ctx->in_use, 
			FALSE, FALSE)) {
		client_deref(state, ctx);
	}
}