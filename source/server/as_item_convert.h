#ifndef _AS_ITEM_CONVERT_H_
#define _AS_ITEM_CONVERT_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_item_convert.h"
#include "public/ap_module.h"

#define AS_ITEM_CONVERT_MODULE_NAME "AgsmItemConvert"

BEGIN_DECLS

struct as_database_codec;

enum as_item_convert_database_id {
	AS_ITEM_CONVERT_DB_SOCKET_COUNT,
	AS_ITEM_CONVERT_DB_CONVERT_TID,
	AS_ITEM_CONVERT_DB_PHYSICAL_CONVERT_LEVEL,
};

enum as_item_convert_callback_id {
	AS_ITEM_CONVERT_CB_,
};

/** 
 * \brief Database item (struct as_item_db) attachment.
 */
struct as_item_convert_db {
	uint32_t socket_count;
	uint32_t convert_tid[AP_ITEM_CONVERT_MAX_SOCKET];
	uint32_t physical_convert_level;
};

struct as_item_convert_module * as_item_convert_create_module();

void as_item_convert_add_callback(
	struct as_item_convert_module * mod,
	enum as_item_convert_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

struct as_item_convert_db * as_item_convert_get_db(
	struct as_item_convert_module * mod,
	struct as_item_db * item);

END_DECLS

#endif /* _AS_ITEM_CONVERT_H_ */
