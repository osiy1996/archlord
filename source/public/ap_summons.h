#ifndef _AP_SUMMONS_H_
#define _AP_SUMMONS_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_character.h"
#include "public/ap_define.h"
#include "public/ap_module_instance.h"

#include <assert.h>

#define AP_SUMMONS_MODULE_NAME "AgpmSummons"

BEGIN_DECLS

enum ap_summons_packet_type {
	AP_SUMMONS_PACKET_SUMMONS_INFO = 0,
};

enum ap_summons_callback_id {
	AP_SUMMONS_CB_RECEIVE,
};

struct ap_summons_character_attachment {
	struct ap_character * summoned_by;
};

struct ap_summons_cb_receive {
	enum ap_summons_packet_type type;
	uint32_t character_id;
	void * user_data;
};

struct ap_summons_module * ap_summons_create_module();

void ap_summons_add_callback(
	struct ap_summons_module * mod,
	enum ap_summons_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t ap_summons_attach_data(
	struct ap_summons_module * mod,
	enum ap_summons_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

struct ap_summons_character_attachment * ap_summons_get_character_attachment(
	struct ap_summons_module * mod,
	struct ap_character * character);

static inline struct ap_character * ap_summons_get_root(
	struct ap_summons_module * mod,
	struct ap_character * character)
{
	while (ap_character_is_summon(character)) {
		character = ap_summons_get_character_attachment(mod, character)->summoned_by;
		assert(character != NULL);
	}
	return character;
}

END_DECLS

#endif /* _AP_SUMMONS_H_ */