#ifndef _AP_ITEM_CONVERT_INTERNAL_H_
#define _AP_ITEM_CONVERT_INTERNAL_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_item_convert.h"

#include "utility/au_packet.h"

BEGIN_DECLS

extern struct ap_item_convert_context * g_ApItemConvertCtx;

inline struct ap_item_convert_context * apitemconv_get_ctx()
{
	return g_ApItemConvertCtx;
}

END_DECLS

#endif /* _AP_ITEM_CONVERT_INTERNAL_H_ */
