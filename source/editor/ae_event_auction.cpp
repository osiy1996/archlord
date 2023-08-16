#include "editor/ae_event_auction.h"

#include "core/log.h"

#include "public/ap_auction.h"

#include "client/ac_imgui.h"

#include <assert.h>

struct ae_event_auction_module {
	struct ap_module_instance instance;
	struct ap_auction_module * ap_auction;
	struct ap_event_manager_module * ap_event_manager;
};

static boolean onregister(
	struct ae_event_auction_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_auction, AP_AUCTION_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	return TRUE;
}

struct ae_event_auction_module * ae_event_auction_create_module()
{
	struct ae_event_auction_module * mod = (struct ae_event_auction_module *)ap_module_instance_new(AE_EVENT_AUCTION_MODULE_NAME, 
		sizeof(*mod), (ap_module_instance_register_t)onregister, NULL, NULL, NULL);
	return mod;
}

boolean ae_event_auction_render_as_node(
	struct ae_event_auction_module * mod,
	void * source,
	struct ap_event_manager_attachment * attachment)
{
	static bool checkbox;
	uint32_t i;
	uint32_t index;
	struct ap_event_manager_event * e = NULL;
	bool changed;
	for (i = 0 ; i < attachment->event_count; i++) {
		if (attachment->events[i].function == AP_EVENT_MANAGER_FUNCTION_AUCTION) {
			index = i;
			e = &attachment->events[i];
			break;
		}
	}
	checkbox = e != NULL;
	changed = ImGui::Checkbox("##EventAuctionEnabled", &checkbox);
	ImGui::SameLine();
	ImGui::Text("Event Function (Auction)");
	if (changed) {
		if (checkbox) {
			if (!e) {
				e = ap_event_manager_add_function(mod->ap_event_manager, attachment,
					AP_EVENT_MANAGER_FUNCTION_AUCTION, source);
				if (!e)
					return FALSE;
				return TRUE;
			}
		}
		else if (e) {
			ap_event_manager_remove_function(mod->ap_event_manager, attachment, index);
			return TRUE;
		}
	}
	return FALSE;
}
