#ifndef _AE_ITEM_H_
#define _AE_ITEM_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_item.h"
#include "public/ap_module.h"

#define AE_ITEM_MODULE_NAME "AgemItem"

BEGIN_DECLS

enum ae_item_callback_id {
};

struct ae_item_module * ae_item_create_module();

void ae_item_add_callback(
	struct ae_item_module * mod,
	enum ae_item_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

END_DECLS

#endif /* _AE_ITEM_H_ */
