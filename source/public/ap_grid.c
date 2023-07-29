#include <assert.h>
#include <stdlib.h>

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_admin.h"
#include "public/ap_define.h"
#include "public/ap_grid.h"
#include "public/ap_module.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"
#include "utility/au_table.h"

struct ap_grid_context {
	struct ap_module module_ctx;
};

static struct ap_grid_context * g_Ctx = NULL;

static inline struct ap_grid_item * getitem(
	struct ap_grid * g,
	uint16_t layer, 
	uint16_t row,
	uint16_t col)
{
	uint32_t index = ap_grid_get_index(g, layer, row, col);
	if (index >= g->grid_count)
		return NULL;
	return &g->items[index];
}

static inline struct ap_grid_item * getitembyindex(
	struct ap_grid * g,
	uint32_t index)
{
	if (index >= g->grid_count)
		return NULL;
	return &g->items[index];
}

static inline void fromindex(
	struct ap_grid * g,
	uint32_t index,
	uint16_t * layer, 
	uint16_t * row,
	uint16_t * col)
{
	uint16_t rc = g->row_count;
	uint16_t cc = g->column_count;
	assert(index < g->grid_count);
	*layer = index / (rc * cc);
	*row = (index % (rc * cc)) / cc;
	*col = index % cc;
}

struct ap_grid * ap_grid_new(
	uint16_t layer_count,
	uint16_t row_count,
	uint16_t column_count,
	uint32_t item_types)
{
	size_t sz = sizeof(struct ap_grid) + 
		sizeof(struct ap_grid_item) * layer_count * row_count * 
		column_count;
	struct ap_grid * g = alloc(sz);
	memset(g, 0, sz);
	g->grid_count = layer_count * row_count * column_count;
	g->items = (struct ap_grid_item *)((uintptr_t)g + sizeof(*g));
	g->layer_count = layer_count;
	g->row_count = row_count;
	g->column_count = column_count;
	g->item_types = item_types;
	return g;
}

void ap_grid_free(struct ap_grid * grid)
{
	dealloc(grid);
}

void ap_grid_clear(struct ap_grid * grid)
{
	memset(grid->items, 0, grid->grid_count * sizeof(*grid->items));
	grid->item_count = 0;
}

boolean ap_grid_is_free(
	struct ap_grid * grid,
	uint16_t layer,
	uint16_t row,
	uint16_t column)
{
	struct ap_grid_item * item = getitem(grid, layer, row, column);
	if (!item)
		return FALSE;
	return (item->id == 0);
}

void ap_grid_set_empty(
	struct ap_grid * grid,
	uint16_t layer,
	uint16_t row,
	uint16_t column)
{
	uint32_t index = ap_grid_get_index(grid, layer, row, column);
	if (index == AP_GRID_INVALID_INDEX) {
		assert(0);
		return;
	}
	if (grid->items[index].id) {
		assert(grid->item_count != 0);
		memset(&grid->items[index], 0, sizeof(struct ap_grid_item));
		grid->item_count--;
	}
}

void ap_grid_set_empty_by_index(
	struct ap_grid * grid,
	uint32_t index)
{
	if (index < grid->grid_count) {
		if (grid->items[index].id) {
			assert(grid->item_count != 0);
			memset(&grid->items[index], 0, 
				sizeof(struct ap_grid_item));
			grid->item_count--;
		}
	}
}

boolean ap_grid_get_empty(
	struct ap_grid * grid,
	uint16_t * layer,
	uint16_t * row,
	uint16_t * column)
{
	uint32_t i;
	for (i = 0; i < grid->grid_count; i++) {
		if (grid->items[i].id == 0) {
			fromindex(grid, i, layer, row, column);
			return TRUE;
		}
	}
	return FALSE;
}

boolean ap_grid_get_empty_in_layer(
	struct ap_grid * grid,
	uint16_t layer,
	uint16_t * row,
	uint16_t * column)
{
	uint32_t begin = ap_grid_get_index(grid, layer, 0, 0);
	uint32_t end = begin + grid->row_count * grid->column_count;
	uint32_t i;
	if (begin == UINT32_MAX)
		return FALSE;
	for (i = begin; i < end; i++) {
		if (grid->items[i].id == 0) {
			fromindex(grid, i, &layer, row, column);
			return TRUE;
		}
	}
	return FALSE;
}

void ap_grid_add_item(
	struct ap_grid * grid,
	uint16_t layer,
	uint16_t row,
	uint16_t column,
	enum ap_grid_item_type type,
	uint32_t id,
	uint32_t tid,
	void * object)
{
	struct ap_grid_item * item = getitem(grid, layer, row, column);
	if (!item) {
		ERROR("Invalid grid index (layer = %u, row = %u, col = %u).",
			layer, row, column);
		assert(0);
		return;
	}
	assert(item->id == 0 && id != 0);
	item->type = type;
	item->id = id;
	item->tid = tid;
	item->object = object;
	grid->item_count++;
}

void ap_grid_add_item_by_index(
	struct ap_grid * grid,
	uint32_t index,
	uint16_t * layer,
	uint16_t * row,
	uint16_t * column,
	enum ap_grid_item_type type,
	uint32_t id,
	uint32_t tid,
	void * object)
{
	struct ap_grid_item * item = getitembyindex(grid, index);
	if (!item) {
		ERROR("Invalid grid index (layer = %u, row = %u, col = %u).",
			layer, row, column);
		assert(0);
		return;
	}
	fromindex(grid, index, layer, row, column);
	assert(item->id == 0 && id != 0);
	item->type = type;
	item->id = id;
	item->tid = tid;
	item->object = object;
	grid->item_count++;
}

void * ap_grid_get_object(
	struct ap_grid * grid,
	uint16_t layer,
	uint16_t row,
	uint16_t column)
{
	struct ap_grid_item * item = getitem(grid, layer, row, column);
	if (!item)
		return NULL;
	return item->object;
}

void * ap_grid_get_object_by_id(
	struct ap_grid * grid,
	uint32_t object_id)
{
	uint32_t i;
	for (i = 0; i < grid->grid_count; i++) {
		if (grid->items[i].id == object_id)
			return grid->items[i].object;
	}
	return NULL;
}

boolean ap_grid_move_item(
	struct ap_grid * grid,
	uint16_t prev_layer,
	uint16_t prev_row,
	uint16_t prev_col,
	uint16_t layer,
	uint16_t row,
	uint16_t col)
{
	struct ap_grid_item * src = getitem(grid, prev_layer, 
		prev_row, prev_col);
	struct ap_grid_item * dst = getitem(grid, layer, row, col);
	if (!src || !dst || dst->id)
		return FALSE;
	memcpy(dst, src, sizeof(*src));
	memset(src, 0, sizeof(*src));
	return TRUE;
}
