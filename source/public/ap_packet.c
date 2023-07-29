#include <assert.h>

#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_packet.h"

#define BUFFER_COUNT 8
#define BUFFER_SIZE ((size_t)1u << 24)
#define TEMPORARY_BUFFER_SIZE ((size_t)1u << 20)
#define TEMPORARY_BUFFER_COUNT 128

struct ap_packet_module {
	struct ap_module_instance instance;
	uint32_t active_buffer_index;
	void * buffers[BUFFER_COUNT];
	uint16_t lengths[BUFFER_COUNT];
	void * buffer_enc;
	void * temp_buffers[TEMPORARY_BUFFER_COUNT];
	uint32_t temp_buffer_cursor;
};

static size_t calc_size()
{
	return BUFFER_SIZE * BUFFER_COUNT + /* buffer */
		BUFFER_SIZE + /* buffer_enc */
		TEMPORARY_BUFFER_SIZE * TEMPORARY_BUFFER_COUNT;
}

static void * advance(void ** memory, size_t size)
{
	void * p = *memory;
	*memory = (void *)((uintptr_t)p + size);
	return p;
}

static void onshutdown(ap_module_t module_)
{
	struct ap_packet_module * mod = module_;
	dealloc(mod->buffers[0]);
}

struct ap_packet_module * ap_packet_create_module()
{
	struct ap_packet_module * mod = ap_module_instance_new("AgpmPacket", sizeof(*mod),
		NULL, NULL, NULL, onshutdown);
	uint32_t i;
	void * memory = alloc(calc_size());
	mod->active_buffer_index = 0;
	for (i = 0; i < BUFFER_COUNT; i++)
		mod->buffers[i] = advance(&memory, BUFFER_SIZE);
	mod->buffer_enc = advance(&memory, BUFFER_SIZE);
	for (i = 0; i < TEMPORARY_BUFFER_COUNT; i++) {
		mod->temp_buffers[i] = advance(&memory, 
			TEMPORARY_BUFFER_SIZE);
	}
	return mod;
}

void ap_packet_set_active_buffer(struct ap_packet_module * mod, uint32_t index)
{
	assert(index < BUFFER_COUNT);
	mod->active_buffer_index = index;
}

void * ap_packet_get_buffer(struct ap_packet_module * mod)
{
	return mod->buffers[mod->active_buffer_index];
}

void ap_packet_set_length(struct ap_packet_module * mod, uint16_t length)
{
	mod->lengths[mod->active_buffer_index] = length;
}

uint16_t ap_packet_get_length(struct ap_packet_module * mod)
{
	return mod->lengths[mod->active_buffer_index];
}

void * ap_packet_get_buffer_enc(struct ap_packet_module * mod)
{
	return mod->buffer_enc;
}

void * ap_packet_get_temp_buffer(struct ap_packet_module * mod)
{
	assert(mod->temp_buffer_cursor < TEMPORARY_BUFFER_COUNT);
	return mod->temp_buffers[mod->temp_buffer_cursor++];
}

void ap_packet_reset_temp_buffers(struct ap_packet_module * mod)
{
	mod->temp_buffer_cursor = 0;
}

void ap_packet_pop_temp_buffers(struct ap_packet_module * mod, uint32_t count)
{
	assert(count <= mod->temp_buffer_cursor);
	mod->temp_buffer_cursor -= count;
}
