#ifndef _AP_PACKET_H_
#define _AP_PACKET_H_

#include "public/ap_module_instance.h"

#define AP_PACKET_MODULE_NAME "AgpmPacket"

BEGIN_DECLS

struct ap_packet_module;

struct ap_packet_module * ap_packet_create_module();

void ap_packet_set_active_buffer(struct ap_packet_module * mod, uint32_t index);

void * ap_packet_get_buffer(struct ap_packet_module * mod);

void ap_packet_set_length(struct ap_packet_module * mod, uint16_t length);

uint16_t ap_packet_get_length(struct ap_packet_module * mod);

void * ap_packet_get_buffer_enc(struct ap_packet_module * mod);

void * ap_packet_get_temp_buffer(struct ap_packet_module * mod);

void ap_packet_reset_temp_buffers(struct ap_packet_module * mod);

void ap_packet_pop_temp_buffers(struct ap_packet_module * mod, uint32_t count);

END_DECLS

#endif /* _AP_PACKET_H_ */
