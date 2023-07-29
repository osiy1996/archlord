#ifndef _AP_EVENT_POINT_LIGHT_H_
#define _AP_EVENT_POINT_LIGHT_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module_instance.h"

#define AP_EVENT_POINT_LIGHT_MODULE_NAME "AgpmEventPointLight"

BEGIN_DECLS

struct ap_event_manager_event;

enum ap_event_point_light_module_data_index {
	AP_EVENT_POINT_LIGHT_MDI_EVENT_DATA,
};

struct ap_event_point_light_data {
	int placeholder;
};

struct ap_event_point_light_event {
	struct ap_event_point_light_data * data;
};

struct ap_event_point_light_module * ap_event_point_light_create_module();

void ap_event_point_light_add_stream_callback(
	struct ap_event_point_light_module * mod,
	const char * module_name,
	ap_module_t callback_module,
	ap_module_stream_read_t read_cb,
	ap_module_stream_write_t write_cb);

size_t ap_event_point_light_attach_data(
	struct ap_event_point_light_module * mod,
	enum ap_event_point_light_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

struct ap_event_point_light_event * ap_event_point_light_get_event(struct ap_event_manager_event * e);

END_DECLS

#endif /* _AP_EVENT_POINT_LIGHT_H_ */
