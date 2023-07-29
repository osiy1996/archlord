#ifndef _AP_STARTUP_ENCRYPTION_H_
#define _AP_STARTUP_ENCRYPTION_H_

#include "public/ap_module_instance.h"

#define AP_STARTUP_ENCRYPTION_MODULE_NAME "AgpmStartupEncryption"

BEGIN_DECLS

struct ap_startup_encryption_module;

enum ap_startup_encryption_packet_type {
	AP_STARTUP_ENCRYPTION_PACKET_REQUEST_PUBLIC = 0,
	AP_STARTUP_ENCRYPTION_PACKET_PUBLIC,
	AP_STARTUP_ENCRYPTION_PACKET_MAKE_PRIVATE,
	AP_STARTUP_ENCRYPTION_PACKET_COMPLETE,
	AP_STARTUP_ENCRYPTION_PACKET_DYNCODE_PUBLIC,
	AP_STARTUP_ENCRYPTION_PACKET_DYNCODE_PRIVATE,
	AP_STARTUP_ENCRYPTION_PACKET_ALGORITHM_TYPE,
};

enum ap_startup_encryption_callback_id {
	AP_STARTUP_ENCRYPTION_CB_RECEIVE,
};

struct ap_startup_encryption_cb_receive {
	enum ap_startup_encryption_packet_type type;
	const uint8_t * data;
	uint16_t length;
	void * user_data;
};

struct ap_startup_encryption_module * ap_startup_encryption_create_module();

void ap_startup_encryption_add_callback(
	struct ap_startup_encryption_module * mod,
	enum ap_startup_encryption_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

boolean ap_startup_encryption_on_receive(
	struct ap_startup_encryption_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_startup_encryption_make_packet(
	struct ap_startup_encryption_module * mod,
	enum ap_startup_encryption_packet_type type,
	const void * data,
	uint16_t data_length);

END_DECLS

#endif /* _AP_STARTUP_ENCRYPTION_H_ */
