#ifndef _AS_EVENT_BINDING_H_
#define _AS_EVENT_BINDING_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_event_binding.h"
#include "public/ap_define.h"
#include "public/ap_module.h"
#include "public/ap_object.h"
#include "public/ap_sector.h"

#define AS_EVENT_BINDING_MODULE_NAME "AgsmEventBinding"

BEGIN_DECLS

enum as_event_binding_callback_id {
	AS_EVENT_BINDING_CB_,
};

struct as_event_binding_module * as_event_binding_create_module();

const struct au_pos * as_event_binding_find_by_region_id(
	struct as_event_binding_module * mod,
	uint32_t region_id);

/**
 * Find a binding by region and binding type.
 * \param[in] region_id Region id.
 * \param[in] type      Binding type.
 * \param[in] pos       (optional) Position filter.
 *
 * \return If a binding is found, returns a position.
 *         Otherwise, returns NULL.
 */
const struct au_pos * as_event_binding_find_by_type(
	struct as_event_binding_module * mod,
	uint32_t region_id,
	enum ap_event_binding_type type,
	const struct au_pos * pos);

END_DECLS

#endif /* _AS_EVENT_BINDING_H_ */
