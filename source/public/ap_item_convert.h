#ifndef _AP_ITEM_CONVERT_H_
#define _AP_ITEM_CONVERT_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_item.h"
#include "public/ap_module_instance.h"

#define AP_ITEM_CONVERT_MODULE_NAME "AgpmItemConvert"

#define	AP_ITEM_CONVERT_MAX_SOCKET 8
#define	AP_ITEM_CONVERT_MAX_WEAPON_SOCKET 8
#define	AP_ITEM_CONVERT_MAX_ARMOUR_SOCKET 6
#define	AP_ITEM_CONVERT_MAX_ETC_SOCKET 5
#define	AP_ITEM_CONVERT_MAX_SPIRIT_STONE 5

#define	AP_ITEM_CONVERT_MAX_DESCRIPTION 128

#define	AP_ITEM_CONVERT_MAX_ITEM_RANK 16
#define	AP_ITEM_CONVERT_MAX_SPIRITSTONE_RANK 5

BEGIN_DECLS

struct ap_item_convert_module;

enum ap_item_convert_result {
	AP_ITEM_CONVERT_RESULT_NONE = 0,
	AP_ITEM_CONVERT_RESULT_SUCCESS,
	AP_ITEM_CONVERT_RESULT_IS_ALREADY_FULL,
	AP_ITEM_CONVERT_RESULT_IS_EGO_ITEM,
	AP_ITEM_CONVERT_RESULT_INVALID_CATALYST,
	AP_ITEM_CONVERT_RESULT_FAILED,
	AP_ITEM_CONVERT_RESULT_FAILED_AND_INIT_SAME,
	AP_ITEM_CONVERT_RESULT_FAILED_AND_INIT,
	AP_ITEM_CONVERT_RESULT_FAILED_AND_DESTROY,
	AP_ITEM_CONVERT_RESULT_INVALID_ITEM,
	AP_ITEM_CONVERT_RESULT_COUNT
};

enum ap_item_convert_socket_result {
	AP_ITEM_CONVERT_SOCKET_RESULT_NONE = 0,
	AP_ITEM_CONVERT_SOCKET_RESULT_SUCCESS,
	AP_ITEM_CONVERT_SOCKET_RESULT_IS_ALREADY_FULL,
	AP_ITEM_CONVERT_SOCKET_RESULT_IS_EGO_ITEM,
	AP_ITEM_CONVERT_SOCKET_RESULT_NOT_ENOUGH_MONEY,
	AP_ITEM_CONVERT_SOCKET_RESULT_FAILED,
	AP_ITEM_CONVERT_SOCKET_RESULT_FAILED_AND_INIT_SAME,
	AP_ITEM_CONVERT_SOCKET_RESULT_FAILED_AND_INIT,
	AP_ITEM_CONVERT_SOCKET_RESULT_FAILED_AND_DESTROY,
	AP_ITEM_CONVERT_SOCKET_RESULT_INVALID_ITEM,
	AP_ITEM_CONVERT_SOCKET_RESULT_COUNT
};

enum ap_item_convert_spirit_stone_result {
	AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_SUCCESS = 0,
	AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_FAILED,
	AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_INVALID_SPIRITSTONE,
	AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_IS_ALREADY_FULL,
	AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_IS_EGO_ITEM,
	AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_IMPROPER_ITEM,
	AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_INVALID_RANK,
	AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_COUNT
};

enum ap_item_convert_rune_result {
	AP_ITEM_CONVERT_RUNE_RESULT_NONE = 0,
	AP_ITEM_CONVERT_RUNE_RESULT_SUCCESS,
	AP_ITEM_CONVERT_RUNE_RESULT_INVALID_RUNE_ITEM,
	AP_ITEM_CONVERT_RUNE_RESULT_IS_ALREADY_FULL,
	AP_ITEM_CONVERT_RUNE_RESULT_IS_EGO_ITEM,
	AP_ITEM_CONVERT_RUNE_RESULT_IS_LOW_CHAR_LEVEL,
	AP_ITEM_CONVERT_RUNE_RESULT_IS_LOW_ITEM_LEVEL,
	AP_ITEM_CONVERT_RUNE_RESULT_IS_IMPROPER_PART,
	AP_ITEM_CONVERT_RUNE_RESULT_IS_ALREADY_ANTI_CONVERT,
	AP_ITEM_CONVERT_RUNE_RESULT_FAILED,
	AP_ITEM_CONVERT_RUNE_RESULT_FAILED_AND_INIT_SAME,
	AP_ITEM_CONVERT_RUNE_RESULT_FAILED_AND_INIT,
	AP_ITEM_CONVERT_RUNE_RESULT_FAILED_AND_DESTROY,
	AP_ITEM_CONVERT_RUNE_RESULT_INVALID_ITEM,
	AP_ITEM_CONVERT_RUNE_RESULT_COUNT
};

enum ap_item_convert_update_flag_bits {
	AP_ITEM_CONVERT_UPDATE_NONE                   = 0,
	AP_ITEM_CONVERT_UPDATE_PHYSICAL_CONVERT_LEVEL = 0x01,
	AP_ITEM_CONVERT_UPDATE_SOCKET_COUNT           = 0x02,
	AP_ITEM_CONVERT_UPDATE_CONVERT_TID            = 0x04,
	AP_ITEM_CONVERT_UPDATE_ALL                    = 0x07
};

enum ap_item_convert_packet_type {
	AP_ITEM_CONVERT_PACKET_TYPE_ADD = 0,
	AP_ITEM_CONVERT_PACKET_TYPE_REQUEST_PHYSICAL_CONVERT,
	AP_ITEM_CONVERT_PACKET_TYPE_REQUEST_ADD_SOCKET,
	AP_ITEM_CONVERT_PACKET_TYPE_REQUEST_RUNE_CONVERT,
	AP_ITEM_CONVERT_PACKET_TYPE_REQUEST_SPIRITSTONE_CONVERT,
	AP_ITEM_CONVERT_PACKET_TYPE_RESPONSE_PHYSICAL_CONVERT,
	AP_ITEM_CONVERT_PACKET_TYPE_RESPONSE_ADD_SOCKET,
	AP_ITEM_CONVERT_PACKET_TYPE_RESPONSE_SPIRITSTONE_CONVERT,
	AP_ITEM_CONVERT_PACKET_TYPE_RESPONSE_RUNE_CONVERT,
	AP_ITEM_CONVERT_PACKET_TYPE_RESPONSE_SPIRITSTONE_CHECK_RESULT,
	AP_ITEM_CONVERT_PACKET_TYPE_RESPONSE_RUNE_CHECK_RESULT,
	AP_ITEM_CONVERT_PACKET_TYPE_CHECK_CASH_RUNE_CONVERT,
	AP_ITEM_CONVERT_PACKET_TYPE_SOCKET_INITIALIZE,
};

enum ap_item_convert_callback_id {
	AP_ITEM_CONVERT_CB_RECEIVE,
};

struct ap_item_convert_physical	{
	uint32_t weapon_add_value;
	uint32_t armor_add_value;
	char rank[16];
	boolean is_convertable_spirit[AP_ITEM_CONVERT_MAX_SPIRITSTONE_RANK + 1];
	uint32_t success_rate[AP_ITEM_CONVERT_MAX_ITEM_RANK + 1];
	uint32_t fail_rate[AP_ITEM_CONVERT_MAX_ITEM_RANK + 1];
	uint32_t init_rate[AP_ITEM_CONVERT_MAX_ITEM_RANK + 1];
	uint32_t destroy_rate[AP_ITEM_CONVERT_MAX_ITEM_RANK + 1];
	uint32_t first_destroy_rank;
};

struct ap_item_convert_add_socket {
	uint32_t weapon_rate;
	uint32_t weapon_cost;
	uint32_t armor_rate;
	uint32_t armor_cost;
	uint32_t etc_rate;
	uint32_t etc_cost;
};

struct ap_item_convert_add_socket_fail {
	uint32_t weapon_keep_current;
	uint32_t weapon_initialize_same;
	uint32_t weapon_initialize;
	uint32_t weapon_destroy;
	uint32_t armor_keep_current;
	uint32_t armor_initialize_same;
	uint32_t armor_initialize;
	uint32_t armor_destroy;
	uint32_t etc_keep_current;
	uint32_t etc_initialize_same;
	uint32_t etc_initialize;
	uint32_t etc_destroy;
};

struct ap_item_convert_spirit_stone {
	uint32_t weapon_add_value;
	uint32_t weapon_rate;
	uint32_t armor_add_value;
	uint32_t armor_rate;
};

struct ap_item_convert_same_attr_bonus {
	uint32_t weapon_bonus;
	uint32_t armor_bonus;
};

struct ap_item_convert_rune {
	uint32_t weapon_rate;
	uint32_t armor_rate;
	uint32_t etc_rate;
};

struct ap_item_convert_rune_fail {
	uint32_t weapon_keep_current;
	uint32_t weapon_initialize_same;
	uint32_t weapon_initialize;
	uint32_t weapon_destroy;
	uint32_t armor_keep_current;
	uint32_t armor_initialize_same;
	uint32_t armor_initialize;
	uint32_t armor_destroy;
	uint32_t etc_keep_current;
	uint32_t etc_initialize_same;
	uint32_t etc_initialize;
	uint32_t etc_destroy;
};

struct ap_item_convert_point {
	uint32_t physical[AP_ITEM_CONVERT_MAX_ITEM_RANK + 1];
	uint32_t spirit_stone[AP_ITEM_CONVERT_MAX_SPIRIT_STONE + 1];
	uint32_t rune[AP_ITEM_CONVERT_MAX_SPIRIT_STONE + 1];
	uint32_t socket[AP_ITEM_CONVERT_MAX_SOCKET + 1];
};

struct ap_item_convert_socket_attr {
	boolean is_spirit_stone;
	uint32_t tid;
	struct ap_item_template * item_template;
};

struct ap_item_convert_item_template {
	boolean rune_convertable_equip_kind[AP_ITEM_EQUIP_KIND_COUNT];
	boolean rune_convertable_equip_part[AP_ITEM_PART_COUNT];
	uint32_t rune_success_rate;
	uint32_t rune_restrict_level;
	uint32_t anti_type_number;
	struct ap_skill_template * skill;
	uint32_t skill_level;
	uint32_t skill_rate;
	char description[AP_ITEM_CONVERT_MAX_DESCRIPTION + 1];
};

struct ap_item_convert_item {
	uint32_t update_flags;
	uint32_t physical_convert_level;
	uint32_t socket_count;
	uint32_t convert_count;
	struct ap_item_convert_socket_attr socket_attr[AP_ITEM_CONVERT_MAX_SOCKET];
	struct ap_factor rune_attr_factor;
};

struct ap_item_convert_cb_receive {
	enum ap_item_convert_packet_type type;
	uint32_t item_id;
	uint32_t convert_id;
	void * user_data;
};

struct ap_item_convert_module * ap_item_convert_create_module();

void ap_item_convert_add_callback(
	struct ap_item_convert_module * mod,
	enum ap_item_convert_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

boolean ap_item_convert_read_convert_table(
	struct ap_item_convert_module * mod,
	const char * file_path);

boolean ap_item_convert_read_rune_attribute_table(
	struct ap_item_convert_module * mod,
	const char * file_path,
	boolean decrypt);

struct ap_item_convert_item_template * ap_item_convert_get_item_template_attachment(
	struct ap_item_convert_module * mod,
	struct ap_item_template * temp);

struct ap_item_convert_item * ap_item_convert_get_item(
	struct ap_item_convert_module * mod,
	struct ap_item * item);

void ap_item_convert_set_physical_convert_bonus(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment,
	uint32_t previous_physical_convert_level);

boolean ap_item_convert_is_suitable_for_physical_convert(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment);

enum ap_item_convert_result ap_item_convert_attempt_physical_convert(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment,
	uint32_t rate);

void ap_item_convert_init_socket(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment);

void ap_item_convert_apply_spirit_stone(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment,
	struct ap_item_template * spirit_stone,
	int modifier);

enum ap_item_convert_spirit_stone_result ap_item_convert_is_suitable_for_spirit_stone(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment,
	struct ap_item * spirit_stone);

enum ap_item_convert_spirit_stone_result ap_item_convert_attempt_spirit_stone(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment,
	struct ap_item * spirit_stone);

enum ap_item_convert_rune_result ap_item_convert_is_suitable_for_rune_convert(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment,
	struct ap_item * rune);

enum ap_item_convert_rune_result ap_item_convert_attempt_rune_convert(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment,
	struct ap_item * rune);

boolean ap_item_convert_on_receive(
	struct ap_item_convert_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

/**
 * Make item convert add packet.
 * \param[in] item Item pointer.
 */
void ap_item_convert_make_add_packet(
	struct ap_item_convert_module * mod,
	struct ap_item * item);

/**
 * Make item convert add packet with custom buffer.
 * \param[in]  item   Item pointer.
 * \param[out] buffer Custom packet buffer.
 * \param[out] length Packet length.
 */
void ap_item_convert_make_add_packet_buffer(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	void * buffer,
	uint16_t * length);

void ap_item_convert_make_response_physical_convert_packet(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	enum ap_item_convert_result result);

void ap_item_convert_make_response_spirit_stone_packet(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	enum ap_item_convert_spirit_stone_result result,
	uint32_t add_spirit_stone_tid);

void ap_item_convert_make_response_rune_convert_packet(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	enum ap_item_convert_rune_result result,
	uint32_t add_convert_item_tid);

void ap_item_convert_make_response_spirit_stone_check_result_packet(
	struct ap_item_convert_module * mod,
	uint32_t character_id,
	uint32_t item_id,
	uint32_t spirit_stone_id,
	enum ap_item_convert_spirit_stone_result result);

void ap_item_convert_make_response_rune_check_result_packet(
	struct ap_item_convert_module * mod,
	uint32_t character_id,
	uint32_t item_id,
	uint32_t convert_id,
	enum ap_item_convert_rune_result result);

END_DECLS

#endif /* _AP_ITEM_CONVERT_H_ */
