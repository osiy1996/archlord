#ifndef _AP_MAP_INTERNAL_H_
#define _AP_MAP_INTERNAL_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_map.h"

BEGIN_DECLS

struct ap_map_context {
	struct ap_module module_ctx;
	struct ap_map_region_template 
		region_templates[AP_MAP_MAX_REGION_ID + 1];
	struct ap_map_region_glossary * glossary;
};

struct ap_map_context * ap_map_get_ctx();

END_DECLS

#endif /* _AP_MAP_INTERNAL_H_ */