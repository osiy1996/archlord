#ifndef _AS_PARTY_H_
#define _AS_PARTY_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_module.h"
#include "public/ap_party.h"

#define AS_PARTY_MODULE_NAME "AgsmParty"

#define AS_PARTY_MAX_CAST_QUEUE_COUNT 32

BEGIN_DECLS

enum as_party_callback_id {
};

struct as_party_attachment {
	int dummy;
};

struct as_party_module * as_party_create_module();

void as_party_add_callback(
	struct as_party_module * mod,
	enum as_party_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

struct as_party_attachment * as_party_get_attachment(
	struct as_party_module * mod, 
	struct ap_party * party);

void as_party_send_packet(struct as_party_module * mod, struct ap_party * party);

void as_party_send_packet_except(
	struct as_party_module * mod, 
	struct ap_party * party,
	struct ap_character * character);

END_DECLS

#endif /* _AS_PARTY_H_ */
