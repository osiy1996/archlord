#ifndef _AS_DROP_ITEM_H_
#define _AS_DROP_ITEM_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_item.h"
#include "public/ap_module.h"

#define AS_DROP_ITEM_MODULE_NAME "AgsmDropItem"

BEGIN_DECLS

typedef struct ap_item *(*as_drop_item_generate_t)(
	ap_module_t mod, 
	const struct ap_item_template * temp,
	uint32_t stack_count,
	void * user_data);

enum as_drop_item_callback_id {
	AS_DROP_ITEM_CB_,
};

struct as_drop_item_character_attachment {
	uint64_t process_drop_tick;
	uint32_t drop_owner_character_id;
};

struct as_drop_item_module * as_drop_item_create_module();

void as_drop_item_add_callback(
	struct as_drop_item_module * mod,
	enum as_drop_item_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t as_drop_item_attach_data(
	struct as_drop_item_module * mod,
	enum as_drop_item_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

struct as_drop_item_character_attachment * as_drop_item_get_character_attachment(
	struct as_drop_item_module * mod,
	struct ap_character * character);

boolean as_drop_item_distribute(
	struct as_drop_item_module * mod, 
	struct ap_character * character,
	uint32_t item_tid,
	uint32_t stack_count);

boolean as_drop_item_distribute_custom(
	struct as_drop_item_module * mod,
	struct ap_character * character,
	uint32_t item_tid,
	uint32_t stack_count,
	ap_module_t callback_module,
	void * user_data,
	as_drop_item_generate_t callback);

boolean as_drop_item_distribute_exclusive(
	struct as_drop_item_module * mod,
	struct ap_character * character,
	uint32_t item_tid,
	uint32_t stack_count,
	ap_module_t callback_module,
	void * user_data,
	as_drop_item_generate_t callback);

END_DECLS

#endif /* _AS_DROP_ITEM_H_ */