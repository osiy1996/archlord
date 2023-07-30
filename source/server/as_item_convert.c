#include <assert.h>

#include "core/log.h"
#include "core/malloc.h"

#include "public/ap_admin.h"

#include "server/as_database.h"
#include "server/as_item.h"
#include "server/as_item_convert.h"

#define DB_STREAM_MODULE_ID 1
#define MAKE_ID(ID) AS_DATABASE_MAKE_ID(DB_STREAM_MODULE_ID, ID)

struct as_item_convert_module {
	struct ap_module_instance instance;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct as_item_module * as_item;
	size_t item_offset;
};

static boolean cbdecode(
	struct as_item_convert_module * mod,
	struct as_item_cb_decode * cb)
{
	boolean result = TRUE;
	struct as_item_convert_db * db = as_item_convert_get_db(mod, cb->item);
	if (cb->module_id != DB_STREAM_MODULE_ID)
		return TRUE;
	switch (cb->field_id) {
	case AS_ITEM_CONVERT_DB_SOCKET_COUNT:
		result &= AS_DATABASE_DECODE(cb->codec, db->socket_count);
		break;
	case AS_ITEM_CONVERT_DB_CONVERT_TID:
		result &= as_database_read_decoded(cb->codec, 
			db->convert_tid, sizeof(db->convert_tid));
		break;
	case AS_ITEM_CONVERT_DB_PHYSICAL_CONVERT_LEVEL:
		result &= AS_DATABASE_DECODE(cb->codec, db->physical_convert_level);
		break;
	default:
		assert(0);
		return TRUE;
	}
	return FALSE;
}

static boolean cbencode(
	struct as_item_convert_module * mod,
	struct as_item_cb_decode * cb)
{
	boolean result = TRUE;
	struct as_item_convert_db * item = as_item_convert_get_db(mod, cb->item);
	result &= AS_DATABASE_ENCODE(cb->codec, 
		MAKE_ID(AS_ITEM_CONVERT_DB_SOCKET_COUNT),
		item->socket_count);
	result &= as_database_encode(cb->codec, 
		MAKE_ID(AS_ITEM_CONVERT_DB_CONVERT_TID), 
		item->convert_tid, sizeof(item->convert_tid));
	result &= AS_DATABASE_ENCODE(cb->codec, 
		MAKE_ID(AS_ITEM_CONVERT_DB_PHYSICAL_CONVERT_LEVEL),
		item->physical_convert_level);
	return result;
}

static boolean cbinitgen(
	struct as_item_convert_module * mod,
	struct as_item_cb_init_generated * cb)
{
	struct ap_item_convert_item * iconv = 
		ap_item_convert_get_item(mod->ap_item_convert, cb->item);
	iconv->update_flags = AP_ITEM_CONVERT_UPDATE_ALL;
	if (cb->item->temp->type == AP_ITEM_TYPE_EQUIP) {
		switch (cb->item->temp->equip.part) {
		case AP_ITEM_PART_BODY:
		case AP_ITEM_PART_HEAD:
		case AP_ITEM_PART_HANDS:
		case AP_ITEM_PART_LEGS:
		case AP_ITEM_PART_FOOT:
		case AP_ITEM_PART_HAND_LEFT:
		case AP_ITEM_PART_HAND_RIGHT:
		case AP_ITEM_PART_ACCESSORY_RING1:
		case AP_ITEM_PART_ACCESSORY_NECKLACE:
			iconv->socket_count = MAX(1, 
				cb->item->temp->min_socket_count);
			break;
		}
	}
	return TRUE;
}

static boolean cbload(
	struct as_item_convert_module * mod,
	struct as_item_cb_load * cb)
{
	struct ap_item_convert_item * iconv = 
		ap_item_convert_get_item(mod->ap_item_convert, cb->item);
	struct as_item_convert_db * db = as_item_convert_get_db(mod, cb->db);
	uint32_t i;
	iconv->physical_convert_level = db->physical_convert_level;
	iconv->socket_count = db->socket_count;
	for (i = 0; i < COUNT_OF(db->convert_tid); i++) {
		uint32_t tid = db->convert_tid[i];
		uint32_t index;
		struct ap_item_template * ctemp;
		if (!tid)
			break;
		if (iconv->convert_count >= COUNT_OF(iconv->socket_attr)) {
			assert(0);
			break;
		}
		ctemp = ap_item_get_template(mod->ap_item, tid);
		if (!ctemp)
			continue;
		index = iconv->convert_count++;
		if (ctemp->type == AP_ITEM_TYPE_USABLE &&
			ctemp->usable.usable_type == AP_ITEM_USABLE_TYPE_SPIRIT_STONE) {
			iconv->socket_attr[index].is_spirit_stone = TRUE;
			ap_item_convert_apply_spirit_stone(mod->ap_item_convert, cb->item,
				iconv, ctemp, 1);
		}
		else {
			iconv->socket_attr[index].is_spirit_stone = FALSE;
		}
		iconv->socket_attr[index].tid = tid;
		iconv->socket_attr[index].item_template = ctemp;
	}
	ap_item_convert_set_physical_convert_bonus(mod->ap_item_convert, cb->item,
		iconv, UINT32_MAX);
	return TRUE;
}

static boolean cbreflect(
	struct as_item_convert_module * mod,
	struct as_item_cb_reflect * cb)
{
	struct ap_item_convert_item * iconv = 
		ap_item_convert_get_item(mod->ap_item_convert, cb->item);
	struct as_item_convert_db * db = as_item_convert_get_db(mod, cb->db);
	if (iconv->update_flags & AP_ITEM_CONVERT_UPDATE_PHYSICAL_CONVERT_LEVEL)
		db->physical_convert_level = iconv->physical_convert_level;
	if (iconv->update_flags & AP_ITEM_CONVERT_UPDATE_SOCKET_COUNT)
		db->socket_count = iconv->socket_count;
	if (iconv->update_flags & AP_ITEM_CONVERT_UPDATE_CONVERT_TID) {
		uint32_t i;
		memset(db->convert_tid, 0, sizeof(db->convert_tid));
		for (i = 0; i < iconv->convert_count; i++) {
			uint32_t tid = iconv->socket_attr[i].tid;
			assert(tid != 0);
			if (i >= COUNT_OF(db->convert_tid)) {
				assert(0);
				break;
			}
			db->convert_tid[i] = tid;
		}
	}
	iconv->update_flags = AP_ITEM_CONVERT_UPDATE_NONE;
	return TRUE;
}

static boolean cbitemcopy(
	struct as_item_convert_module * mod,
	struct as_item_cb_copy * cb)
{
	struct as_item_convert_db * src = as_item_convert_get_db(mod, cb->src);
	struct as_item_convert_db * dst = as_item_convert_get_db(mod, cb->dst);
	memcpy(dst, src, sizeof(*src));
	return TRUE;
}

static boolean onregister(
	struct as_item_convert_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item_convert, AP_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_item, AS_ITEM_MODULE_NAME);
	mod->item_offset = as_item_attach_data(mod->as_item, AS_ITEM_MDI_DATABASE,
		sizeof(struct as_item_convert_db), mod, NULL, NULL);
	if (mod->item_offset == SIZE_MAX) {
		ERROR("Failed to attach database item convert data.");
		return FALSE;
	}
	if (!as_item_set_database_stream(mod->as_item, DB_STREAM_MODULE_ID,
			mod, cbdecode, cbencode)) {
		ERROR("Failed to set item convert database stream.");
		return FALSE;
	}
	as_item_add_callback(mod->as_item, AS_ITEM_CB_INIT_GENERATED, 
		mod, cbinitgen);
	as_item_add_callback(mod->as_item, AS_ITEM_CB_LOAD, mod, cbload);
	as_item_add_callback(mod->as_item, AS_ITEM_CB_REFLECT, mod, cbreflect);
	as_item_add_callback(mod->as_item, AS_ITEM_CB_COPY, mod, cbitemcopy);
	return TRUE;
}

struct as_item_convert_module * as_item_convert_create_module()
{
	struct as_item_convert_module * mod = ap_module_instance_new(AS_ITEM_CONVERT_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	return mod;
}

void as_item_convert_add_callback(
	struct as_item_convert_module * mod,
	enum as_item_convert_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

struct as_item_convert_db * as_item_convert_get_db(
	struct as_item_convert_module * mod,
	struct as_item_db * item)
{
	return ap_module_get_attached_data(item, mod->item_offset);
}
