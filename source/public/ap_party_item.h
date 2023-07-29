#ifndef _AP_PARTY_ITEM_H_
#define _AP_PARTY_ITEM_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_character.h"
#include "public/ap_define.h"
#include "public/ap_module_instance.h"

#include <assert.h>

#define AP_PARTY_ITEM_MODULE_NAME "AgpmPartyItem"

BEGIN_DECLS

struct ap_party_item_module * ap_party_item_create_module();

void ap_party_item_make_packet(
	struct ap_party_item_module * mod, 
	struct ap_item * item);

END_DECLS

#endif /* _AP_PARTY_ITEM_H_ */