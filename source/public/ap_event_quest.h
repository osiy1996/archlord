#ifndef _AP_EVENT_QUEST_H_
#define _AP_EVENT_QUEST_H_

#include "public/ap_module_instance.h"

#define AP_EVENT_QUEST_MODULE_NAME "AgpmEventQuest"

BEGIN_DECLS

struct ap_event_manager_event;

struct ap_event_quest_event {
	uint32_t quest_group_id;
};

struct ap_event_quest_module * ap_event_quest_create_module();

struct ap_event_quest_event * ap_event_quest_get_event(struct ap_event_manager_event * e);

END_DECLS

#endif /* _AP_EVENT_QUEST_H_ */
