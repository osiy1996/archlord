#ifndef _AP_EVENT_GUILD_H_
#define _AP_EVENT_GUILD_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_event_manager.h"
#include "public/ap_module_instance.h"

#define AP_EVENT_GUILD_MODULE_NAME "AgpmEventGuild"

#define AP_EVENT_GUILD_MAX_USE_RANGE 800

BEGIN_DECLS

enum ap_event_guild_packet_type {
	AP_EVENT_GUILD_PACKET_TYPE_REQUEST = 0,
	AP_EVENT_GUILD_PACKET_TYPE_GRANT,
	AP_EVENT_GUILD_PACKET_TYPE_REQUEST_WAREHOUSE,
	AP_EVENT_GUILD_PACKET_TYPE_GRANT_WAREHOUSE,
	AP_EVENT_GUILD_PACKET_TYPE_REQUEST_WORLD_CHAMPIONSHIP,
	AP_EVENT_GUILD_PACKET_TYPE_GRANT_WORLD_CHAMPIONSHIP,
};

enum ap_event_guild_callback_id {
	AP_EVENT_GUILD_CB_RECEIVE,
};

struct ap_event_guild_cb_receive {
	enum ap_event_guild_packet_type type;
	struct ap_event_manager_base_packet * event;
	uint32_t character_id;
	void * user_data;
};

struct ap_event_guild_module * ap_event_guild_create_module();

void ap_event_guild_add_callback(
	struct ap_event_guild_module * mod,
	enum ap_event_guild_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

boolean ap_event_guild_on_receive(
	struct ap_event_guild_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

/**
 * Make event npc dialog packet.
 * \param[in] type         Packet type.
 * \param[in] event        (optional) Event pointer.
 * \param[in] character_id (optional) Character id pointer.
 */
void ap_event_guild_make_packet(
	struct ap_event_guild_module * mod,
	enum ap_event_guild_packet_type type,
	const struct ap_event_manager_event * event,
	uint32_t * character_id);

inline struct ap_event_guild_data * ap_event_guild_get_data(
	struct ap_event_manager_event * event)
{
	return (struct ap_event_guild_data *)event->data;
}

END_DECLS

#endif /* _AP_EVENT_GUILD_H_ */
