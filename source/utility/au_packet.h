#ifndef _AU_PACKET_H_
#define _AU_PACKET_H_

#include "core/macros.h"
#include "core/types.h"

#define AU_PACKET_FRONT_GUARD_BYTE 0xD6
#define AU_PACKET_FRONT_PUBLIC_BYTE 0xA1
#define AU_PACKET_FRONT_PRIVATE_BYTE 0xB1
#define AU_PACKET_REAR_GUARD_BYTE 0x6B
#define AU_PACKET_REAR_PUBLIC_BYTE 0xAF
#define AU_PACKET_REAR_PRIVATE_BYTE 0xBF

#define AU_PACKET_TYPE_END 0xFF

#define AU_PACKET_GET_STRING(DST, FIELD)\
	if ((FIELD)) {\
		strlcpy((DST), (FIELD), sizeof(DST));\
	}

BEGIN_DECLS

enum au_packet_field_type {
	AU_PACKET_TYPE_CHAR = 0,
	AU_PACKET_TYPE_INT8,
	AU_PACKET_TYPE_UINT8,
	AU_PACKET_TYPE_INT16,
	AU_PACKET_TYPE_UINT16,
	AU_PACKET_TYPE_INT32,
	AU_PACKET_TYPE_UINT32,
	AU_PACKET_TYPE_INT64,
	AU_PACKET_TYPE_UINT64,
	AU_PACKET_TYPE_FLOAT,
	AU_PACKET_TYPE_POS,
	AU_PACKET_TYPE_MATRIX,
	AU_PACKET_TYPE_PACKET,
	AU_PACKET_TYPE_MEMORY_BLOCK,
	AU_PACKET_TYPE_POS_BASEMETER,
	AU_PACKET_TYPE_WCHAR,
	AU_PACKET_TYPE_COUNT
};

#pragma pack(1)

struct au_packet_header {
	uint8_t guard_byte;
	uint16_t length;
	uint8_t type;
	uint8_t flags;
	uint32_t owner_id;
	uint32_t frame_tick;
};

#pragma pack()

struct au_packet {
	uint16_t field_type[64];
	uint16_t field_len[64];
	uint16_t field_count;
	uint16_t flag_len;
};

void au_packet_init(
	struct au_packet * packet,
	uint32_t flag_length, ...);

boolean au_packet_get_field(
	const struct au_packet * p,
	boolean is_packet,
	const void * data,
	uint16_t length, ...);

void au_packet_make_packet(
	const struct au_packet * p,
	void * buffer,
	boolean is_packet,
	uint16_t * length, 
	uint8_t type, ...);

END_DECLS

#endif /* _AU_PACKET_H_ */
