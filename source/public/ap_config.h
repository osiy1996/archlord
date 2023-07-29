#ifndef _AP_CONFIG_H_
#define _AP_CONFIG_H_

#include "public/ap_module_instance.h"

#define AP_CONFIG_MODULE_NAME "AgpmConfig"

BEGIN_DECLS

struct ap_config_module;

struct ap_config_module * ap_config_create_module();

const char * ap_config_get(struct ap_config_module * mod, const char * key);

void ap_config_make_packet(
	struct ap_config_module * mod,
	boolean drop_item_on_death,
	boolean exp_penalty_on_death,
	boolean is_test_server);

END_DECLS

#endif /* _AP_CONFIG_H_ */
