#ifndef _AP_EVENT_MANAGER_H_
#define _AP_EVENT_MANAGER_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_module_instance.h"

#define AP_EVENT_MANAGER_MODULE_NAME "ApmEventManager"

#define AP_EVENT_MANAGER_MAX_EVENT_COUNT 5
#define AP_EVENT_MANAGER_IS_FUNCTION_VALID(function) \
	((function) >= AP_EVENT_MANAGER_FUNCTION_NONE && \
		(function) < AP_EVENT_MANAGER_FUNCTION_COUNT)

#define AP_EVENT_MANAGER_EID_FLAG_LOCAL 0x40000000
#define AP_EVENT_MANAGER_TARGET_NUMBER 3

BEGIN_DECLS

enum ap_event_manager_function_type {
	AP_EVENT_MANAGER_FUNCTION_NONE = 0,
	AP_EVENT_MANAGER_FUNCTION_SPAWN,
	AP_EVENT_MANAGER_FUNCTION_FACTOR,
	AP_EVENT_MANAGER_FUNCTION_VEHICLE,
	AP_EVENT_MANAGER_FUNCTION_SCHEDULE,
	AP_EVENT_MANAGER_FUNCTION_HIDDEN,
	AP_EVENT_MANAGER_FUNCTION_SHOP,
	AP_EVENT_MANAGER_FUNCTION_INFORMATION,
	AP_EVENT_MANAGER_FUNCTION_TELEPORT,
	AP_EVENT_MANAGER_FUNCTION_NPCTRADE,
	AP_EVENT_MANAGER_FUNCTION_CONVERSATION,
	AP_EVENT_MANAGER_FUNCTION_NATURE,
	AP_EVENT_MANAGER_FUNCTION_STATUS,
	AP_EVENT_MANAGER_FUNCTION_ACTION,
	AP_EVENT_MANAGER_FUNCTION_SKILL,
	AP_EVENT_MANAGER_FUNCTION_SHRINE_LEGACY,
	AP_EVENT_MANAGER_FUNCTION_UVU_REWARD,
	AP_EVENT_MANAGER_FUNCTION_ITEM_REPAIR,
	AP_EVENT_MANAGER_FUNCTION_MASTERY_SPECIALIZE,
	AP_EVENT_MANAGER_FUNCTION_BINDING,
	AP_EVENT_MANAGER_FUNCTION_BANK,
	AP_EVENT_MANAGER_FUNCTION_NPCDIALOG,
	AP_EVENT_MANAGER_FUNCTION_ITEMCONVERT,
	AP_EVENT_MANAGER_FUNCTION_GUILD,
	AP_EVENT_MANAGER_FUNCTION_PRODUCT,
	AP_EVENT_MANAGER_FUNCTION_SKILLMASTER,
	AP_EVENT_MANAGER_FUNCTION_REFINERY,
	AP_EVENT_MANAGER_FUNCTION_QUEST,
	AP_EVENT_MANAGER_FUNCTION_AUCTION,
	AP_EVENT_MANAGER_FUNCTION_CHAR_CUSTOMIZE,
	AP_EVENT_MANAGER_FUNCTION_POINTLIGHT,
	AP_EVENT_MANAGER_FUNCTION_REMISSION,
	AP_EVENT_MANAGER_FUNCTION_WANTEDCRIMINAL,
	AP_EVENT_MANAGER_FUNCTION_SIEGEWAR_NPC,
	AP_EVENT_MANAGER_FUNCTION_TAX,
	AP_EVENT_MANAGER_FUNCTION_GUILD_WAREHOUSE,
	AP_EVENT_MANAGER_FUNCTION_ARCHLORD,
	AP_EVENT_MANAGER_FUNCTION_GAMBLE,
	AP_EVENT_MANAGER_FUNCTION_GACHA,
	AP_EVENT_MANAGER_FUNCTION_WORLD_CHAMPIONSHIP,
	AP_EVENT_MANAGER_FUNCTION_SHRINE,
	AP_EVENT_MANAGER_FUNCTION_COUNT
};

enum ap_event_manager_area_type {
	AP_EVENT_MANAGER_AREA_SPHERE = 0,
	AP_EVENT_MANAGER_AREA_FAN,
	AP_EVENT_MANAGER_AREA_BOX,
	AP_EVENT_MANAGER_MAX_AREA
};

enum ap_event_manager_cond_type {
	AP_EVENT_MANAGER_COND_NONE = 0x00,
	AP_EVENT_MANAGER_COND_TARGET = 0x01,
	AP_EVENT_MANAGER_COND_AREA = 0x02,
	AP_EVENT_MANAGER_COND_ENVIRONMENT = 0x04,
	AP_EVENT_MANAGER_COND_TIME = 0x08
};

struct ap_event_manager_cond_target {
	//AgpdFactor					m_stFactor;
	uint32_t item_tid[AP_EVENT_MANAGER_TARGET_NUMBER];
	uint32_t cid;
};

struct ap_event_manager_cond_area {
	enum ap_event_manager_area_type type;
	union {
		float sphere_radius;
		struct {
			float radius;
			float angle;
		} pan;
		struct au_box box;
	} data;
};

struct ap_event_manager_cond_time {
	int32_t active_time_offset;
	int32_t end_time_offset;
};

struct ap_event_manager_condition {
	struct ap_event_manager_cond_target * target;
	struct ap_event_manager_cond_area * area;
	struct ap_event_manager_cond_time * time;
};

struct ap_event_manager_event {
	int32_t eid;
	enum ap_event_manager_function_type function;
	void * data;
	struct ap_event_manager_condition cond;
	/** \brief Source module data (object, character, etc.). */
	void * source;
	uint32_t event_start_time;
};

struct ap_event_manager_attachment {
	uint32_t event_count;
	struct ap_event_manager_event events[AP_EVENT_MANAGER_MAX_EVENT_COUNT];
};

struct ap_event_manager_cb_make_packet {
	struct ap_event_manager_event * event;
	void * custom_data;
};

struct ap_event_manager_base_packet {
	enum ap_base_type source_type;
	uint32_t source_id;
	uint32_t eid;
	enum ap_event_manager_function function;
};

struct ap_event_manager_module * ap_event_manager_create_module();

boolean ap_event_manager_register_event(
	struct ap_event_manager_module * mod,
	enum ap_event_manager_function_type function,
	ap_module_t event_module,
	ap_module_default_t ctor,
	ap_module_default_t dtor,
	ap_module_stream_read_t stream_read,
	ap_module_stream_write_t stream_write,
	void * user_data);

/**
 * Register function packet callbacks.
 * \param[in] function       Function type.
 * \param[in] make_packet    Callback to make function packets.
 * \param[in] receive_packet Callback to process received 
 *                           function packets..
 *
 * \return TRUE if successful. Otherwise FALSE.
 */
boolean ap_event_manager_register_packet(
	struct ap_event_manager_module * mod,
	enum ap_event_manager_function_type function,
	ap_module_default_t make_packet,
	ap_module_default_t receive_packet);

void ap_event_manager_set_cond(
	struct ap_event_manager_module * mod,
	struct ap_event_manager_event * e, 
	uint32_t cond);

/**
 * Event manager module attaches the same structure to all 
 * types of module data (characters, objects, items, etc.).
 * \param[in] data Module data.
 *
 * \return Data attachment.
 */
struct ap_event_manager_attachment * ap_event_manager_get_attachment(
	struct ap_event_manager_module * mod,
	void * data);

struct ap_event_manager_event * ap_event_manager_add_function(
	struct ap_event_manager_module * mod,
	struct ap_event_manager_attachment * attachment,
	enum ap_event_manager_function_type function,
	void * source);

void ap_event_manager_remove_function(
	struct ap_event_manager_module * mod,
	struct ap_event_manager_attachment * attachment,
	uint32_t index);

/**
 * Find event function in object.
 * \param[in] source   Source data (character, object, item, etc.).
 * \param[in] function Event function.
 *
 * \return Event pointer if event function is found.
 *         Otherwise NULL.
 */
struct ap_event_manager_event * ap_event_manager_get_function(
	struct ap_event_manager_module * mod,
	void * source,
	enum ap_event_manager_function_type function);

/**
 * Parse base packet.
 * \param[in]  data   Data to parse.
 * \param[out] packet Output structure pointer.
 *
 * \return TRUE if successful. Otherwise FALSE.
 */
boolean ap_event_manager_get_base_packet(
	struct ap_event_manager_module * mod,
	const void * data,
	struct ap_event_manager_base_packet * packet);

/**
 * Make event base packet.
 * \param[in]  event  Event.
 * \param[out] buffer Output buffer.
 */
void ap_event_manager_make_base_packet(
	struct ap_event_manager_module * mod,
	const struct ap_event_manager_event * event,
	void * buffer);

/**
 * Make event data packet.
 * \param[in]  event  Event pointer.
 * \param[out] buffer Output buffer.
 * \param[out] length Output buffer length.
 *
 * \return TRUE if successful. Otherwise FALSE.
 */
boolean ap_event_manager_make_event_data_packet(
	struct ap_event_manager_module * mod,
	struct ap_event_manager_event * event,
	void * buffer,
	uint16_t * length);

END_DECLS

#endif /* _AP_EVENT_MANAGER_H_ */
