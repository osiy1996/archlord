#ifndef _AC_EVENT_POINT_LIGHT_H_
#define _AC_EVENT_POINT_LIGHT_H_

#include "public/ap_module_instance.h"

#define AC_EVENT_POINT_LIGHT_MODULE_NAME "AgcmEventPointLight"

BEGIN_DECLS

struct ap_event_point_light_data;

struct ac_event_point_light_data {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	float radius;
	uint32_t enable_flag;
};

struct ac_event_point_light_module * ac_event_point_light_create_module();

struct ac_event_point_light_data * ac_event_point_light_get_data(
	struct ac_event_point_light_module * mod,
	struct ap_event_point_light_data * data);

END_DECLS

#endif /* _AC_EVENT_POINT_LIGHT_H_ */
