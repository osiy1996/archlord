#ifndef _AP_EVENT_SKILL_MASTER_H_
#define _AP_EVENT_SKILL_MASTER_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_event_manager.h"
#include "public/ap_module_instance.h"

#define AP_EVENT_SKILL_MASTER_MODULE_NAME "AgpmEventSkillMaster"

#define	AP_EVENT_SKILL_MASTER_MAX_USE_RANGE 600

BEGIN_DECLS

enum ap_event_skill_master_packet_type {
	AP_EVENT_SKILL_MASTER_PACKET_TYPE_BUY_SKILL_BOOK = 0,
	AP_EVENT_SKILL_MASTER_PACKET_TYPE_LEARN_SKILL,
	AP_EVENT_SKILL_MASTER_PACKET_TYPE_REQUEST_UPGRADE,
	AP_EVENT_SKILL_MASTER_PACKET_TYPE_RESPONSE_UPGRADE,
	AP_EVENT_SKILL_MASTER_PACKET_TYPE_REQUEST_EVENT,
	AP_EVENT_SKILL_MASTER_PACKET_TYPE_RESPONSE_EVENT,
	AP_EVENT_SKILL_MASTER_PACKET_TYPE_LEARN_RESULT,
	AP_EVENT_SKILL_MASTER_PACKET_TYPE_BUY_RESULT,
	AP_EVENT_SKILL_MASTER_PACKET_TYPE_SKILL_INITIALIZE,
	AP_EVENT_SKILL_MASTER_PACKET_TYPE_SKILL_INITIALIZE_RESULT,
};

enum ap_event_skill_master_event_request_result {
	AP_EVENT_SKILL_MASTER_EVENT_REQUEST_RESULT_SUCCESS = 0,
	AP_EVENT_SKILL_MASTER_EVENT_REQUEST_RESULT_FAIL
};

enum ap_event_skill_master_learn_result {
	AP_EVENT_SKILL_MASTER_LEARN_RESULT_SUCCESS = 0,
	AP_EVENT_SKILL_MASTER_LEARN_RESULT_FAIL,
	AP_EVENT_SKILL_MASTER_LEARN_RESULT_ALREADY_LEARN,
	AP_EVENT_SKILL_MASTER_LEARN_RESULT_NOT_ENOUGH_SKILLPOINT,
	AP_EVENT_SKILL_MASTER_LEARN_RESULT_LOW_LEVEL,
	AP_EVENT_SKILL_MASTER_LEARN_RESULT_NOT_LEARNABLE_CLASS,
	AP_EVENT_SKILL_MASTER_LEARN_RESULT_NOT_ENOUGH_HEROICPOINT,
	AP_EVENT_SKILL_MASTER_LEARN_RESULT_NOT_ENOUGH_CHARISMAPOINT,
};

enum ap_event_skill_master_callback_id {
	AP_EVENT_SKILL_MASTER_CB_RECEIVE,
};

struct ap_event_skill_master_data {
	enum au_race_type race;
	enum au_char_class_type class_;
};

struct ap_event_skill_master_cb_receive {
	enum ap_event_skill_master_packet_type type;
	struct ap_event_manager_base_packet * event;
	uint32_t character_id;
	uint32_t skill_id;
	uint32_t skill_tid;
	void * user_data;
};

struct ap_event_skill_master_module * ap_event_skill_master_create_module();

void ap_event_skill_master_add_callback(
	struct ap_event_skill_master_module * mod,
	enum ap_event_skill_master_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

boolean ap_event_skill_master_on_receive(
	struct ap_event_skill_master_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

/**
 * Make event skill master packet.
 * \param[in] type		                   Packet type.
 * \param[in] event             (optional) Event pointer.
 * \param[in] character_id      (optional) Character id pointer.
 * \param[in] skill_id          (optional) Skill id pointer.
 * \param[in] skill_tid         (optional) Skill tid pointer.
 * \param[in] result                       Event result.
 * \param[in] skill_point       (optional) Skill point pointer.
 * \param[in] is_reset_consumed (optional) Reset flag pointer.
 */
void ap_event_skill_master_make_packet(
	struct ap_event_skill_master_module * mod,
	enum ap_event_skill_master_packet_type type,
	const struct ap_event_manager_event * event,
	const uint32_t * character_id,
	const uint32_t * skill_id,
	const uint32_t * skill_tid,
	uint32_t result,
	const uint8_t * skill_point,
	const boolean * is_reset_consumed);

inline struct ap_event_skill_master_data * ap_event_skill_master_get_data(
	struct ap_event_manager_event * event)
{
	return (struct ap_event_skill_master_data *)event->data;
}

END_DECLS

#endif /* _AP_EVENT_SKILL_MASTER_H_ */
