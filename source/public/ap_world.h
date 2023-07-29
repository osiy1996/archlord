#ifndef _AP_WORLD_H_
#define _AP_WORLD_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_module_instance.h"

#define AP_WORLD_MODULE_NAME "AgpmWorld"

#define AP_WORLD_MAX_WORLD_NAME 32
#define AP_WORLD_MAX_SERVER_IN_WORLD 32
#define AP_WORLD_MAX_ENCODED_LENGTH 2048
#define AP_WORLD_MAX_SERVER_REGION 32

BEGIN_DECLS

enum ap_world_status {
	AP_WORLD_STATUS_UNKNOWN = 0,
	AP_WORLD_STATUS_GOOD,
	AP_WORLD_STATUS_ABOVE_NORMAL,
	AP_WORLD_STATUS_NORMAL,
	AP_WORLD_STATUS_BELOW_NORMAL,
	AP_WORLD_STATUS_BAD,
	AP_WORLD_STATUS_COUNT
};

enum ap_world_flag_bits {
	AP_WORLD_FLAG_NONE = 0,
	AP_WORLD_FLAG_NEW = 0x01,
	AP_WORLD_FLAG_NC17 = 0x02,
	AP_WORLD_FLAG_EVENT = 0x04,
	AP_WORLD_FLAG_AIM_EVENT = 0x08,
	AP_WORLD_FLAG_TEST_SERVER = 0x10,
};

enum ap_world_packet_type {
	AP_WORLD_PACKET_GET_WORLD = 0,
	AP_WORLD_PACKET_RESULT_GET_WORLD,
	AP_WORLD_PACKET_RESULT_GET_WORLD_ALL,
	AP_WORLD_PACKET_GET_CHAR_COUNT,
	AP_WORLD_PACKET_RESULT_CHAR_COUNT,
};

enum ap_world_data_type {
	AP_WORLD_DATA_TYPE_PUBLIC = 0,
	AP_WORLD_DATA_TYPE_CLIENT,
};

enum ap_world_callback_id {
	AP_WORLD_CB_RECEIVE,
};

struct ap_world_cb_receive {
	enum ap_world_packet_type type;
	char world_name[AP_WORLD_MAX_WORLD_NAME];
	enum ap_world_status world_status;
	char server_region[AP_WORLD_MAX_SERVER_REGION];
	void * user_data;
};

struct ap_world_module * ap_world_create_module();

void ap_world_add_callback(
	struct ap_world_module * mod,
	enum ap_world_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

boolean ap_world_on_receive(
	struct ap_world_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_world_make_result_get_world_all_packet(
	struct ap_world_module * mod,
	const char * group_name,
	const char * world_name,
	enum ap_world_status status,
	uint32_t flags);

void ap_world_make_result_char_count_packet(
	struct ap_world_module * mod,
	const char * world_name,
	uint32_t count);

END_DECLS

#endif /* _AP_WORLD_H_ */
