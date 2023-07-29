#include <assert.h>
#include <stdlib.h>

#include "core/log.h"
#include "core/malloc.h"

#include "public/ap_auction.h"
#include "public/ap_event_manager.h"
#include "public/ap_object.h"

#define INI_NAME_START "AuctionStart"
#define INI_NAME_END "AuctionEnd"
#define INI_NAME_HAVE_EVENT "HaveAuctionEvent"

struct ap_auction_module {
	struct ap_module_instance instance;
	struct ap_event_manager_module * ap_event_manager;
};

static boolean event_ctor(struct ap_auction_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	e->data = alloc(sizeof(struct ap_auction_event));
	((struct ap_auction_event *)e->data)->has_auction_event = FALSE;
	return TRUE;
}

static boolean event_dtor(struct ap_auction_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	dealloc(e->data);
	e->data = NULL;
	return TRUE;
}

static boolean event_read(
	struct ap_auction_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_auction_event * e = ap_auction_get_event(mod, data);
	const char * value_name;
	if (!ap_module_stream_read_next_value(stream))
		return TRUE;
	value_name = ap_module_stream_get_value_name(stream);
	if (strcmp(value_name, INI_NAME_START) != 0)
		return TRUE;
	while (ap_module_stream_read_next_value(stream)) {
		value_name = ap_module_stream_get_value_name(stream);
		if (strcmp(value_name, INI_NAME_HAVE_EVENT) == 0) {
			e->has_auction_event = 
				strtol(ap_module_stream_get_value(stream), 
					NULL, 10) != 0;
		}
		else if (strcmp(value_name, INI_NAME_END) == 0) {
			break;
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

static boolean event_write(
	struct ap_auction_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_auction_event * e = ap_auction_get_event(mod, data);
	if (!ap_module_stream_write_i32(stream, INI_NAME_START, 0) ||
		!ap_module_stream_write_i32(stream, INI_NAME_HAVE_EVENT, 
				e->has_auction_event) ||
		!ap_module_stream_write_i32(stream, INI_NAME_END, 0)) {
		return FALSE;
	}
	return TRUE;
}

static boolean onregister(
	struct ap_auction_module * mod,
	struct ap_module_registry * registry)
{
	mod->ap_event_manager = ap_module_registry_get_module(registry, AP_EVENT_MANAGER_MODULE_NAME);
	if (!mod->ap_event_manager) {
		ERROR("Failed to retrieve module (%s).", AP_EVENT_MANAGER_MODULE_NAME);
		return FALSE;
	}
	if (!ap_event_manager_register_event(mod->ap_event_manager,
			AP_EVENT_MANAGER_FUNCTION_AUCTION, mod, event_ctor, event_dtor, 
			event_read, event_write, NULL)) {
		ERROR("Failed to register event.");
		return FALSE;
	}
	return TRUE;
}

struct ap_auction_module * ap_auction_create_module()
{
	struct ap_auction_module * mod = ap_module_instance_new(AP_AUCTION_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	return mod;
}

struct ap_auction_event * ap_auction_get_event(
	struct ap_auction_module * mod, 
	struct ap_event_manager_event * e)
{
	return e->data;
}
