#ifndef _AP_OPTIMIZED_PACKET2_H_
#define _AP_OPTIMIZED_PACKET2_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_factors.h"
#include "public/ap_module_instance.h"

#define AP_OPTIMIZED_PACKET2_MODULE_NAME "AgpmOptimizedPacket2"

BEGIN_DECLS

enum ap_optimized_packet2_type {
	AP_OPTIMIZED_PACKET2_NONE = 0,
	AP_OPTIMIZED_PACKET2_ADD_CHARACTER_VIEW,
	AP_OPTIMIZED_PACKET2_ADD_CHARACTER,
	AP_OPTIMIZED_PACKET2_RELEASE_MOVE_ACTION,
};

enum ap_optimized_packet2_callback_id {
	AP_OPTIMIZED_PACKET2_CB_RECEIVE_CHAR_VIEW,
	AP_OPTIMIZED_PACKET2_CB_RECEIVE_CHAR_MOVE,
	AP_OPTIMIZED_PACKET2_CB_RECEIVE_CHAR_ACTION,
};

struct ap_optimized_packet2_cb_receive_char_view {
	uint8_t packet_type;
	uint32_t character_id;
	void * user_data;
};

struct ap_optimized_packet2_cb_receive_char_move {
	uint32_t character_id;
	struct au_pos pos;
	struct au_pos dst_pos;
	uint8_t move_flags;
	uint8_t move_direction;
	uint8_t next_action_type;
	uint32_t next_action_skill_id;
	void * user_data;
};

struct ap_optimized_packet2_cb_receive_char_action {
	uint32_t character_id;
	uint32_t target_id;
	boolean force_attack;
	void * user_data;
};

struct ap_optimized_packet2_module * ap_optimized_packet2_create_module();

void ap_optimized_packet2_add_callback(
	struct ap_optimized_packet2_module * mod,
	enum ap_optimized_packet2_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

boolean ap_optimized_packet2_on_receive_char_view(
	struct ap_optimized_packet2_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

boolean ap_optimized_packet2_on_receive_char_move(
	struct ap_optimized_packet2_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

boolean ap_optimized_packet2_on_receive_char_action(
	struct ap_optimized_packet2_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_optimized_packet2_make_char_view_packet(
	struct ap_optimized_packet2_module * mod,
	struct ap_character * character);

/**
 * Make character view packet with custom buffer.
 * \param[in]  character Character pointer.
 * \param[out] buffer    Output buffer.
 * \param[out] length    Output buffer length.
 */
void ap_optimized_packet2_make_char_view_packet_buffer(
	struct ap_optimized_packet2_module * mod,
	struct ap_character * character,
	void * buffer,
	uint16_t * length);

void ap_optimized_packet2_make_char_move_packet(
	struct ap_optimized_packet2_module * mod,
	struct ap_character * character,
	const struct au_pos * pos,
	const struct au_pos * dst_pos,
	uint8_t move_flags,
	uint8_t move_direction);

void ap_optimized_packet2_make_char_all_item_skill(
	struct ap_optimized_packet2_module * mod,
	struct ap_character * character);

void ap_optimized_packet2_make_char_action_packet(
	struct ap_optimized_packet2_module * mod,
	uint32_t character_id,
	uint32_t target_id,
	const struct ap_character_action_info * info);

END_DECLS

#endif /* _AP_OPTIMIZED_PACKET2_H_ */
