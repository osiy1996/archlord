#include "utility/au_packet.h"

#include <assert.h>
#include <stdarg.h>
#include <string.h>

static const uint32_t TYPE_LENGTH[] = { 
		0x01, 0x01, 0x01, 0x02, 0x02, 0x04, 
		0x04, 0x08, 0x08, 0x04, 0x0C, 0x40, 
		0x08, 0x00, 0x06, 0x02 };

static uint16_t block_size(const void * block)
{
	return *(uint16_t *)(block);
}

void au_packet_init(
	struct au_packet * packet,
	uint32_t flag_length, ...)
{
	uint16_t type;
	va_list ap;
	memset(packet, 0, sizeof(*packet));
	va_start(ap, flag_length);
	type = va_arg(ap, uint16_t);
	while (type != AU_PACKET_TYPE_END) {
		uint32_t i;
		assert(type <  AU_PACKET_TYPE_COUNT);
		assert(packet->field_count < COUNT_OF(packet->field_type));
		i = packet->field_count++;
		packet->field_type[i] = type;
		packet->field_len[i] = va_arg(ap, uint16_t);
		assert(packet->field_len[i] > 0);
		type = va_arg(ap, uint16_t);
	}
	va_end(ap);
	packet->flag_len = flag_length;
}

boolean au_packet_get_field(
	const struct au_packet * p,
	boolean is_packet,
	const void * data,
	uint16_t length, ...)
{
	uint32_t flag_bits = 0;
	uint32_t bit = 1U << 0;;
	int ret = 0;
	va_list args;
	uint32_t i;
	const uint8_t * cursor = NULL;
	const uint8_t * end = 
		(const uint8_t *)((uintptr_t)data + length);
	if (!p->flag_len)
		return FALSE;
	if (is_packet) {
		if (length < sizeof(struct au_packet_header) + p->flag_len)
			return FALSE;
		cursor = 
			(const uint8_t *)((uintptr_t)data + p->flag_len + 13);
		switch (p->flag_len) {
		case 4:
			flag_bits = *(const uint32_t *)((uintptr_t)data + 13);
			break;
		case 3:
		default:
			return FALSE;
		case 2:
			flag_bits = *(const uint16_t *)((uintptr_t)data + 13);
			break;
		case 1:
			flag_bits = ((const uint8_t *)data)[13];
			break;
		}
	}
	else {
		length = block_size(data);
		if (length < p->flag_len)
			return FALSE;
		cursor = 
			(const uint8_t *)((uintptr_t)data + p->flag_len + 2);
		end = (const uint8_t *)((uintptr_t)data + length + 2);
		switch (p->flag_len) {
		case 4:
			flag_bits = *(const uint32_t *)((uintptr_t)data + 2);
			break;
		case 3:
		default:
			return FALSE;
		case 2:
			flag_bits = *(const uint16_t *)((uintptr_t)data + 2);
			break;
		case 1:
			flag_bits = ((const uint8_t *)data)[2];
			break;
		}
	}
	if (!p->field_count)
		return TRUE;
	va_start(args, length);
	for (i = 0; i < p->field_count; i++, bit <<= 1) {
		void ** arg = va_arg(args, void **);
		uint16_t size;
		if (flag_bits & bit) {
			if (p->field_type[i] == AU_PACKET_TYPE_PACKET) {
				if (p->field_len[i] <= 1) {
					size = *(uint16_t*)cursor;
					if (arg)
						*arg = (void*)cursor;
					cursor = cursor + size + 2;
					if (cursor > end) {
						va_end(args);
						return FALSE;
					}
				}
				else {
					uint8_t num_blocks = *cursor++;
					uint32_t j;
					size = 0;
					for (j = 0; j < num_blocks; j++) {
						if (arg)
							arg[j] = (void*)cursor;
						size = block_size(cursor);
						cursor = cursor + size + 2;
						if (cursor > end) {
							va_end(args);
							return FALSE;
						}
					}
				}
			}
			else if (p->field_type[i] == AU_PACKET_TYPE_MEMORY_BLOCK) {
				uint16_t * arg_sz = va_arg(args, uint16_t *);
				size = block_size(cursor);
				if (arg)
					*arg = (void*)(cursor + 2);
				if (arg_sz)
					*arg_sz = size;
				cursor = cursor + size + 2;
				if (cursor > end) {
					va_end(args);
					return FALSE;
				}
			}
			else if (p->field_type[i]) {
				uint16_t size = TYPE_LENGTH[p->field_type[i]] * 
					p->field_len[i];
				if (cursor + size > end) {
					va_end(args);
					return FALSE;
				}
				if (arg)
					memcpy((void*)arg, cursor, size);
				cursor = cursor + size;
			}
			else {
				if (arg)
					*arg = (void*)cursor;
				cursor = cursor + (TYPE_LENGTH[p->field_type[i]] * 
					p->field_len[i]);
				if (cursor > end) {
					va_end(args);
					return FALSE;
				}
			}
		}
		else if (p->field_type[i] == AU_PACKET_TYPE_PACKET) {
			if (arg)
				*arg = NULL;
		}
		else if (p->field_type[i] == AU_PACKET_TYPE_MEMORY_BLOCK) {
			uint16_t * unused = va_arg(args, uint16_t *);
		}
	}
	va_end(args);
	return TRUE;
}

void au_packet_make_packet(
	const struct au_packet * p,
	void * buffer,
	boolean is_packet,
	uint16_t * length, 
	uint8_t type, ...)
{
	uint32_t flag = 0;
	uint16_t len;
	va_list ap;
	void * field;
	uint32_t bit;
	uint32_t i;
	uint8_t * cursor;
	if (is_packet)
		len = sizeof(struct au_packet_header) + p->flag_len;
	else
		len = sizeof(uint16_t)/* packet length */ + p->flag_len;
	va_start(ap, type);
	field = va_arg(ap, void *);
	bit = 0x01;
	for (i = 0; i < p->field_count; i++) {
		if (field) {
			boolean set_flag = TRUE;
			if (p->field_type[i] == AU_PACKET_TYPE_PACKET) {
				if (p->field_len[i] > 1) {
					void ** packet_array = (void **)field;
					uint32_t j;
					for (j = 0; j < p->field_len[i]; ++j) {
						if (!packet_array[j])
							break;
						len += sizeof(uint16_t) + 
							*(uint16_t *)packet_array[j];
					}
					if (j > 0) {
						/* if packet type is array packet.. */
						len += sizeof(uint8_t);
					}
					else {
						set_flag = FALSE;
					}
				}
				else {
					len += sizeof(uint16_t) + 
						*(uint16_t *)field;
				}
			}
			else if (p->field_type[i] == AU_PACKET_TYPE_MEMORY_BLOCK) {
				field = va_arg(ap, void *);
				if (field) {
					len += sizeof(uint16_t) + 
						*(uint16_t *)field;
				}
			}
			else {
				len += TYPE_LENGTH[p->field_type[i]] * 
					p->field_len[i];
			}
			if (set_flag)
				flag |= bit;
		}
		field = va_arg(ap, void *);
		bit <<= 1;
	}
	va_end(ap);
#ifdef	_DEBUG
	assert(len > 0);
	//assert(len < PACKET_BUFFER_SIZE * 40);
#else
	//if (len < 0 || len >= PACKET_BUFFER_SIZE * 40)
	//	return FALSE;
#endif	//_DEBUG
	if (is_packet)
		len += 1; /* end guard byte */
	if (is_packet) {
		*((uint8_t *)buffer) = AU_PACKET_FRONT_GUARD_BYTE;
		*((uint8_t *)buffer + len - 1) = AU_PACKET_REAR_GUARD_BYTE;
		/* Set frame tick */
		((struct au_packet_header *)buffer)->frame_tick = 0;
		/* TODO: Implement actual frame tick.. */
	}
	if (length)
		*length = len;
	((struct au_packet_header *)buffer)->length = len;
	if (is_packet) {
		((struct au_packet_header *)buffer)->owner_id =	 0;
		((struct au_packet_header *)buffer)->flags = 0;
	}
	if (is_packet) {
		((struct au_packet_header *)buffer)->type	= type;
		switch (p->flag_len) {
		case 1:
			*(uint8_t *)((uintptr_t)buffer + 
				sizeof(struct au_packet_header)) = flag;
			break;
		case 2:
			*(uint16_t *)((uintptr_t)buffer + 
				sizeof(struct au_packet_header)) = flag;
			break;
		case 4:
			*(uint32_t *)((uintptr_t)buffer + 
				sizeof(struct au_packet_header)) = flag;
			break;
		}
		cursor = (void *)((uintptr_t)buffer + 
			sizeof(struct au_packet_header) + p->flag_len);
	}
	else
	{
		*(uint16_t *)buffer = len - sizeof(uint16_t);
		switch (p->flag_len) {
		case 1:
			*(uint8_t *)((uint8_t *)buffer + 
				sizeof(uint16_t)) = flag;
			break;
		case 2:
			*(uint16_t *)((uint8_t *)buffer + 
				sizeof(uint16_t)) = flag;
			break;
		case 4:
			*(uint32_t *)((uint8_t *)buffer + 
				sizeof(uint16_t)) = flag;
			break;
		}
		cursor = (void *)((uintptr_t) buffer + 
			sizeof(uint16_t) + p->flag_len);
	}
	va_start(ap, type);
	field = va_arg(ap, void *);
	for (i = 0; i < p->field_count; i++) {
		if (field) {
			if (p->field_type[i] == AU_PACKET_TYPE_PACKET) {
				if (p->field_len[i] > 1) {
					uint8_t * array_size = (uint8_t *)cursor;
					void ** packet_array = (void **)field;
					uint32_t j;
					cursor += sizeof(uint8_t);
					for (j = 0; j < p->field_len[i]; ++j) {
						if (!packet_array[j])
							break;
						len = sizeof(uint16_t) + *(uint16_t *) packet_array[j];
						if (len > 0)
							memcpy(cursor, packet_array[j], len);
						cursor += len;
					}
					if (j > 0)
						*array_size = j;
					else
						cursor -= sizeof(uint8_t);
					len = 0;
				}
				else {
					len = sizeof(uint16_t) + *(uint16_t *)field;
				}
			}
			else if (p->field_type[i] == AU_PACKET_TYPE_MEMORY_BLOCK) {
				void * ln = va_arg(ap, void *);
				if (ln) {
					memcpy(cursor, ln, sizeof(uint16_t));
					cursor += sizeof(uint16_t);
					len = *(uint16_t *)ln;
				}
			}
			else {
				len = TYPE_LENGTH[p->field_type[i]] * 
					p->field_len[i];
			}
			if (len > 0)
				memcpy(cursor, field, len);
			cursor += len;
			len = 0;
		}
		field = va_arg(ap, void *);
	}
	va_end(ap);
}