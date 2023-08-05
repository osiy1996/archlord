#include <assert.h>

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_admin.h"
#include "public/ap_event_binding.h"
#include "public/ap_event_manager.h"
#include "public/ap_map.h"
#include "public/ap_object.h"

#include "server/as_event_binding.h"

struct bindinglist {
	struct ap_event_binding_event * event;
	struct bindinglist * next;
};

struct as_event_binding_module {
	struct ap_module_instance instance;
	struct ap_event_binding_module * ap_event_binding;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_map_module * ap_map;
	struct ap_object_module * ap_object;
	struct ap_admin region_binding_admin;
	struct ap_admin region_bind_type_admin;
	struct ap_event_binding_event ** buffer;
};

static float distance(
	const struct au_pos * p1, 
	const struct au_pos * p2)
{
	float dx = p1->x - p2->x;
	float dz = p1->z - p2->z;
	return sqrtf(dx * dx + dz + dz);
}

static void addtotail(
	struct bindinglist * list, 
	struct ap_event_binding_event * event)
{
	struct bindinglist * last = NULL;
	struct bindinglist * cur = list;
	struct bindinglist * link;
	while (cur) {
		last = cur;
		cur = cur->next;
	}
	assert(last);
	link = alloc(sizeof(*link));
	link->event = event;
	link->next = NULL;
	last->next = link;
}

static boolean parsebinding(
	struct as_event_binding_module * mod,
	struct ap_object * obj,
	struct ap_event_binding_event * event)
{
	event->binding.base_pos = obj->position;
	if (!strisempty(event->binding.town_name)) {
		struct ap_map_region_template * r = 
			ap_map_get_region_template_by_name(mod->ap_map,
				event->binding.town_name);
		if (r) {
			struct bindinglist * list = 
				ap_admin_get_object_by_id(&mod->region_binding_admin, r->id);
			uint64_t id;
			if (list) {
				addtotail(list, event);
			}
			else {
				list = ap_admin_add_object_by_id(&mod->region_binding_admin, r->id);
				assert(list != NULL);
				list->event = event;
				list->next = NULL;
			}
			assert(event->binding.binding_type != 0);
			id = r->id | ((uint64_t)event->binding.binding_type << 32);
			list = ap_admin_get_object_by_id(&mod->region_bind_type_admin, id);
			if (list) {
				addtotail(list, event);
			}
			else {
				list = ap_admin_add_object_by_id(&mod->region_bind_type_admin, id);
				assert(list != NULL);
				list->event = event;
				list->next = NULL;
			}
		}
		else {
			WARN("Failed to retrieve binding region (%u).",
				event->binding_id);
		}
	}
	return TRUE;
}

static boolean cbobjectinit(
	struct as_event_binding_module * mod, 
	struct ap_object * obj)
{
	struct ap_event_manager_attachment * events = 
		ap_event_manager_get_attachment(mod->ap_event_manager, obj);
	uint32_t i;
	for (i = 0; i < events->event_count; i++) {
		struct ap_event_manager_event * e = &events->events[i];
		switch (e->function) {
		case AP_EVENT_MANAGER_FUNCTION_BINDING: {
			struct ap_event_binding_event * binding = ap_event_binding_get_event(e);
			if (!parsebinding(mod, obj, binding)) {
				ERROR("Failed to parse binding (%u).", 
					binding->binding_id);
				return FALSE;
			}
			break;
		}
		}
	}
	return TRUE;
}

static boolean onregister(
	struct as_event_binding_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_binding, AP_EVENT_BINDING_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_map, AP_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_object, AP_OBJECT_MODULE_NAME);
	ap_object_add_callback(mod->ap_object, AP_OBJECT_CB_INIT_OBJECT, 
		mod, cbobjectinit);
	return TRUE;
}

static void onshutdown(struct as_event_binding_module * mod)
{
	size_t index = 0;
	struct bindinglist * list = NULL;
	while (ap_admin_iterate_id(&mod->region_binding_admin, &index,
			&list)) {
		struct bindinglist * cur = list->next;
		while (cur) {
			struct bindinglist * next = cur->next;
			dealloc(cur);
			cur = next;
		}
	}
	index = 0;
	while (ap_admin_iterate_id(&mod->region_bind_type_admin, 
			&index, &list)) {
		struct bindinglist * cur = list->next;
		while (cur) {
			struct bindinglist * next = cur->next;
			dealloc(cur);
			cur = next;
		}
	}
	ap_admin_destroy(&mod->region_binding_admin);
	ap_admin_destroy(&mod->region_bind_type_admin);
	vec_free(mod->buffer);
}

struct as_event_binding_module * as_event_binding_create_module()
{
	struct as_event_binding_module * mod = ap_module_instance_new(AS_EVENT_BINDING_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	ap_admin_init(&mod->region_binding_admin, 
		sizeof(struct bindinglist), 256);
	ap_admin_init(&mod->region_bind_type_admin, 
		sizeof(struct bindinglist), 256);
	mod->buffer = vec_new_reserved(sizeof(*mod->buffer), 16);
	return mod;
}

const struct au_pos * as_event_binding_find_by_region_id(
	struct as_event_binding_module * mod,
	uint32_t region_id)
{
	struct ap_map_region_template * r = 
		ap_map_get_region_template(mod->ap_map, region_id);
	if (!r)
		return NULL;
	return NULL;
}

static const struct au_pos * findbytype(
	struct as_event_binding_module * mod,
	uint32_t region_id,
	const enum ap_event_binding_type * type_list,
	uint32_t type_count,
	const struct au_pos * pos)
{
	uint32_t i;
	if (pos) {
		const struct au_pos * nearest = NULL;
		float nd = 0.0f;
		for (i = 0; i < type_count; i++) {
			struct bindinglist * cur;
			uint64_t id = region_id | ((uint64_t)type_list[i] << 32);
			struct bindinglist * list = 
				ap_admin_get_object_by_id(&mod->region_bind_type_admin, id);
			if (!list)
				continue;
			cur = list;
			while (cur) {
				float d = distance(pos, &cur->event->binding.base_pos);
				if (!nearest || d < nd) {
					nearest = &cur->event->binding.base_pos;
					nd = d;
				}
				cur = cur->next;
			}
			return nearest;
		}
	}
	else {
		for (i = 0; i < type_count; i++) {
			uint64_t id = region_id | ((uint64_t)type_list[i] << 32);
			struct bindinglist * list = 
				ap_admin_get_object_by_id(&mod->region_bind_type_admin, id);
			if (!list)
				continue;
			return &list->event->binding.base_pos;
		}
	}
	return NULL;
}

const struct au_pos * as_event_binding_find_by_type(
	struct as_event_binding_module * mod,
	uint32_t region_id,
	enum ap_event_binding_type type,
	const struct au_pos * pos)
{
	if (type == AP_EVENT_BINDING_TYPE_NONE) {
		enum ap_event_binding_type typelist[] = {
			AP_EVENT_BINDING_TYPE_RESURRECTION,
			AP_EVENT_BINDING_TYPE_NEW_CHARACTER,
			AP_EVENT_BINDING_TYPE_SIEGEWAR_OFFENSE,
			AP_EVENT_BINDING_TYPE_SIEGEWAR_DEFENSE_INNER,
			AP_EVENT_BINDING_TYPE_SIEGEWAR_ARCHLORD,
			AP_EVENT_BINDING_TYPE_SIEGEWAR_ARCHLORD_ATTACKER,
			AP_EVENT_BINDING_TYPE_SIEGEWAR_DEFENSE_OUTTER };
		return findbytype(mod, region_id, typelist, COUNT_OF(typelist), pos);
	}
	else {
		return findbytype(mod, region_id, &type, 1, pos);
	}
}
