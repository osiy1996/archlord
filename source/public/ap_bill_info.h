#ifndef _AP_BILL_INFO_H_
#define _AP_BILL_INFO_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_character.h"
#include "public/ap_define.h"
#include "public/ap_module_instance.h"

#include <assert.h>

#define AP_BILL_INFO_MODULE_NAME "AgpmBillInfo"

enum ap_bill_info_packet_type {
	AP_BILL_INFO_PACKET_ADD = 0,
	AP_BILL_INFO_PACKET_CASHINFO,
	AP_BILL_INFO_PACKET_GUID,
};

BEGIN_DECLS

struct ap_bill_info_module * ap_bill_info_create_module();

void ap_bill_info_make_cash_info_packet(
	struct ap_bill_info_module * mod,
	uint32_t character_id,
	uint64_t wcoins,
	uint64_t pcoins);

END_DECLS

#endif /* _AP_BILL_INFO_H_ */