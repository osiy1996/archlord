#ifndef _AP_RIDE_H_
#define _AP_RIDE_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_module_instance.h"

#define AP_RIDE_MODULE_NAME "AgpmRide"

extern size_t g_ApRideCharacterAttachmentOffset;

BEGIN_DECLS

enum ap_ride_packet_type {
	AP_RIDE_PACKET_NONE = 0,
	AP_RIDE_PACKET_RIDE_REQ,
	AP_RIDE_PACKET_RIDE_ACK,
	AP_RIDE_PACKET_DISMOUNT_REQ,
	AP_RIDE_PACKET_DISMOUNT_ACK,
	AP_RIDE_PACKET_TIMEOUT,
	AP_RIDE_PACKET_RIDE_TID,
};

enum ap_ride_callback_id {
	AP_RIDE_CB_RECEIVE,
};

struct ap_ride_cb_receive {
	enum ap_ride_packet_type type;
	void * user_data;
};

struct ap_ride_module * ap_ride_create_module();

void ap_ride_add_callback(
	struct ap_ride_module * mod,
	enum ap_ride_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

boolean ap_ride_on_receive(
	struct ap_ride_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_ride_make_packet(
	struct ap_ride_module * mod, 
	enum ap_ride_packet_type type,
	uint32_t character_id,
	uint32_t item_id);

END_DECLS

#endif /* _AP_RIDE_H_ */