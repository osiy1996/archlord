#include "server/as_refinery_process.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/vector.h"

#include "public/ap_chat.h"
#include "public/ap_drop_item.h"
#include "public/ap_item.h"
#include "public/ap_item_convert.h"
#include "public/ap_module.h"
#include "public/ap_random.h"
#include "public/ap_refinery.h"

#include "server/as_drop_item.h"
#include "server/as_item.h"
#include "server/as_player.h"

#include <assert.h>
#include <stdlib.h>

#define DROP_PROCESS_DELAY_MS 800
#define DROP_EXPIRE_DELAY_MS 45000

struct as_refinery_process_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_chat_module * ap_chat;
	struct ap_drop_item_module * ap_drop_item;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct ap_random_module * ap_random;
	struct ap_refinery_module * ap_refinery;
	struct as_character_module * as_character;
	struct as_drop_item_module * as_drop_item;
	struct as_item_module * as_item;
	struct as_player_module * as_player;
};

static uint32_t generatesocketcount(
	struct as_refinery_process_module * mod,
	uint32_t min_socket,
	uint32_t max_socket)
{
	if (!min_socket)
		return 1;
	if (max_socket > AP_ITEM_CONVERT_MAX_SOCKET || min_socket >= max_socket)
		return min_socket;
	return ap_random_between(mod->ap_random, min_socket, max_socket);
}

static struct ap_item * generateproduct(
	struct as_refinery_process_module * mod, 
	const struct ap_item_template * temp,
	uint32_t stack_count,
	const struct ap_refinery_product * product)
{
	struct ap_item * item = ap_item_create(mod->ap_item, temp->tid);
	if (!item)
		return NULL;
	item->update_flags = AP_ITEM_UPDATE_ALL;
	item->stack_count = stack_count;
	if (temp->type == AP_ITEM_TYPE_EQUIP) {
		struct ap_item_convert_item * attachment;
		if (temp->option_count) {
			uint32_t i;
			item->option_count = temp->option_count;
			for (i = 0; i < temp->option_count; i++) {
				item->option_tid[i] = temp->option_tid[i];
				item->options[i] = temp->options[i];
			}
		}
		else if (product->max_option_count) {
			uint32_t count = ap_random_between(mod->ap_random, 
				product->min_option_count, product->max_option_count);
			if (count) {
				ap_drop_item_generate_options_for_refined(mod->ap_drop_item, 
					item, count);
			}
		}
		attachment = ap_item_convert_get_item(mod->ap_item_convert, item);
		attachment->socket_count = generatesocketcount(mod, 
			product->min_socket_count, product->max_socket_count);
		attachment->update_flags |= AP_ITEM_CONVERT_UPDATE_SOCKET_COUNT;
	}
	return item;
}


static boolean cbreceive(
	struct as_refinery_process_module * mod,
	struct ap_refinery_cb_receive * cb)
{
	struct ap_character * character = cb->user_data;
	switch (cb->type) {
	case AP_REFINERY_PACKET_REFINE_ITEM: {
		static char resourcestring[1024];
		struct ap_refinery_resource reciperesources[AP_REFINERY_MAX_RESOURCE_COUNT];
		struct ap_item * resources[AP_REFINERY_MAX_RESOURCE_COUNT];
		uint32_t resource_count = 0;
		uint32_t i;
		struct ap_refinery_recipe * recipe;
		uint32_t productindex;
		struct ap_refinery_product * product;
		assert(COUNT_OF(resources) == COUNT_OF(cb->resource_item_id));
		for (i = 0; i < COUNT_OF(cb->resource_item_id); i++) {
			struct ap_item * item;
			if (!cb->resource_item_id[i])
				continue;
			item = ap_item_find(mod->ap_item, character,
				cb->resource_item_id[i], 3, AP_ITEM_STATUS_INVENTORY, 
				AP_ITEM_STATUS_SUB_INVENTORY, AP_ITEM_STATUS_CASH_INVENTORY);
			if (!item || item->in_use || item->expire_time)
				continue;
			reciperesources[resource_count].item_tid = item->tid;
			reciperesources[resource_count].stack_count = item->stack_count;
			resources[resource_count++] = item;
		}
		qsort(reciperesources, resource_count, sizeof(reciperesources[0]),
			ap_refinery_sort_resources);
		ap_refinery_make_resource_string(resourcestring, sizeof(resourcestring),
			reciperesources, resource_count);
		recipe = ap_refinery_find_recipe(mod->ap_refinery, resourcestring);
		if (!recipe || !recipe->product_count) {
			ap_refinery_make_result_packet(mod->ap_refinery, character->id,
				AP_REFINERY_RESULT_FAIL_DIFFERENT_ITEM, 0);
			as_player_send_packet(mod->as_player, character);
			break;
		}
		if (recipe->required_gold > character->inventory_gold) {
			ap_refinery_make_result_packet(mod->ap_refinery, character->id,
				AP_REFINERY_RESULT_FAIL_INSUFFICIENT_MONEY, 0);
			as_player_send_packet(mod->as_player, character);
			break;
		}
		/** \todo
		 * Some products are tagged with "failure", for which a fail-safe 
		 * mechanism needs to be implemented. 
		 * 
		 * For example, when refining unique accessories, Archondrite may be 
		 * tagged with "failure". After getting 2 "failures", in the next 
		 * refine, it should be guaranteed to get a non-failure product.
		 * 
		 * When a non-failure product is obtained, fail-counter that may 
		 * exist for that recipe should be reset.
		 * 
		 * Each recipe should have its unique fail counter. These should be 
		 * stored in a structure attached to 'struct as_character_db', 
		 * with a recipe ID and a fail-counter for each recipe. 
		 * For details, see as_item module. */
		productindex = ap_random_get(mod->ap_random, recipe->product_count);
		product = &recipe->products[productindex];
		if (!as_drop_item_distribute_custom(mod->as_drop_item, character, 
			product->item_tid, product->stack_count, mod, product, generateproduct)) {
			ap_refinery_make_result_packet(mod->ap_refinery, character->id,
				AP_REFINERY_RESULT_FAIL_FULL_INVENTORY, 0);
			as_player_send_packet(mod->as_player, character);
			break;
		}
		as_character_consume_gold(mod->as_character, character, recipe->required_gold);
		for (i = 0; i < resource_count; i++)
			as_item_delete(mod->as_item, character, resources[i]);
		ap_refinery_make_result_packet(mod->ap_refinery, character->id,
			AP_REFINERY_RESULT_SUCCESS, product->item_tid);
		as_player_send_packet(mod->as_player, character);
		break;
	}
	}
	return TRUE;
}

static void cbchatcreaterefined(
	struct as_refinery_process_module * mod,
	struct ap_character * c,
	uint32_t argc,
	const char * const * argv)
{
	struct ap_refinery_product product = { 0 };
	const struct ap_item_template * temp;
	if (argc < 1)
		return;
	product.item_tid = strtoul(argv[0], NULL, 10);
	temp = ap_item_get_template(mod->ap_item, product.item_tid);
	if (!temp)
		return;
	if (argc > 1) {
		product.stack_count = strtoul(argv[1], NULL, 10);
		if (!product.stack_count)
			product.stack_count = 1;
	}
	else {
		product.stack_count = 1;
	}
	product.min_option_count = temp->min_option_count;
	product.max_option_count = temp->max_option_count;
	product.min_socket_count = temp->min_socket_count;
	product.max_socket_count = temp->max_socket_count;
	as_drop_item_distribute_custom(mod->as_drop_item, c, 
		product.item_tid, product.stack_count, mod, &product, generateproduct);
}

static boolean onregister(
	struct as_refinery_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_chat, AP_CHAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_drop_item, AP_DROP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item_convert, AP_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_random, AP_RANDOM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_refinery, AP_REFINERY_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_drop_item, AS_DROP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_item, AS_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	ap_chat_add_command(mod->ap_chat, "/createrefined", mod, cbchatcreaterefined);
	ap_refinery_add_callback(mod->ap_refinery, AP_REFINERY_CB_RECEIVE, mod, cbreceive);
	return TRUE;
}

static void onclose(struct as_refinery_process_module * mod)
{
}

static void onshutdown(struct as_refinery_process_module * mod)
{
}

struct as_refinery_process_module * as_refinery_process_create_module()
{
	struct as_refinery_process_module * mod = ap_module_instance_new(AS_REFINERY_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	return mod;
}
