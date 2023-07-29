#ifndef _AS_UI_STATUS_H_
#define _AS_UI_STATUS_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_ui_status.h"
#include "public/ap_module.h"

#define AS_UI_STATUS_MODULE_NAME "AgsmUIStatus"

BEGIN_DECLS

struct as_database_codec;

enum as_ui_status_character_database_id {
	AS_UI_STATUS_CHARACTER_DB_UI_STATUS_ITEMS,
	AS_UI_STATUS_CHARACTER_DB_UI_STATUS_HP_POTION_TID,
	AS_UI_STATUS_CHARACTER_DB_UI_STATUS_MP_POTION_TID,
	AS_UI_STATUS_CHARACTER_DB_UI_STATUS_OPTION_VIEW_HELMET,
	AS_UI_STATUS_CHARACTER_DB_UI_STATUS_AUTO_USE_HP_GAUGE,
	AS_UI_STATUS_CHARACTER_DB_UI_STATUS_AUTO_USE_MP_GAUGE,
};

/** \brief as_character_db attachment. */
struct as_ui_status_character_db {
	struct ap_ui_status_item items[AP_UI_STATUS_MAX_QUICKBELT_ITEM_COUNT];
	uint32_t hp_potion_tid;
	uint32_t mp_potion_tid;
	boolean option_view_helmet;
	uint8_t auto_use_hp_gauge;
	uint8_t auto_use_mp_gauge;
};

struct as_ui_status {
	int dummy;
};

struct as_ui_status_module * as_ui_status_create_module();

/**
 * Retrieve attached data.
 * \param[in] character Character pointer.
 * 
 * \return Character database object attachment.
 */
struct as_ui_status_character_db * as_ui_status_get_character_db(
	struct as_ui_status_module * mod,
	struct as_character_db * character);

END_DECLS

#endif /* _AS_UI_STATUS_H_ */
