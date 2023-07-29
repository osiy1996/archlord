#ifndef _AP_GRID_H_
#define _AP_GRID_H_

#include <assert.h>

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_module.h"

#define AP_GRID_ITEM_BOTTOM_STRING_LENGTH 5
#define AP_GRID_INVALID_INDEX UINT32_MAX

BEGIN_DECLS

enum ap_grid_item_type {
	AP_GRID_ITEM_TYPE_NONE = 0x00000000,
	AP_GRID_ITEM_TYPE_ITEM = 0x00000001,
	AP_GRID_ITEM_TYPE_SKILL = 0x00000002,
	AP_GRID_ITEM_TYPE_SHORCUT = 0x00000004,
	AP_GRID_ITEM_TYPE_SPECIALIZE = 0x00000008,
	AP_GRID_ITEM_TYPE_QUEST = 0x00000010,
	AP_GRID_ITEM_TYPE_GUILDMARK = 0x00000020,
	AP_GRID_ITEM_TYPE_SOCIALACTION = 0x00000040,
};

/* In the original implementation, this data type has 
* a lot more members such as tooltip, grid cooldown, etc.
*
* On server side, we will access grid members (mostly items) 
* thousands of times per-frame. As such, reducing the 
* size of this structure will allow for better cache utilization.
*/
struct ap_grid_item {
	enum ap_grid_item_type type;
	uint32_t id;
	uint32_t tid;
	/* Type of this object depends on grid item type. */
	void * object;
};

struct ap_grid {
	uint32_t grid_count;
	uint32_t item_count;
	struct ap_grid_item * items;
	uint16_t layer_count;
	uint16_t row_count;
	uint16_t column_count;
	uint32_t item_types;
};

struct ap_grid * ap_grid_new(
	uint16_t layer_count,
	uint16_t row_count,
	uint16_t column_count,
	uint32_t item_types);

void ap_grid_free(struct ap_grid * grid);

void ap_grid_clear(struct ap_grid * grid);

boolean ap_grid_is_free(
	struct ap_grid * grid,
	uint16_t layer,
	uint16_t row,
	uint16_t column);

/**
 * Set grid slot empty.
 * \param[in,out] grid   Grid pointer.
 * \param[in]     layer  Grid layer.
 * \param[in]     row    Grid row.
 * \param[in]     column Grid column.
 */
void ap_grid_set_empty(
	struct ap_grid * grid,
	uint16_t layer,
	uint16_t row,
	uint16_t column);

void ap_grid_set_empty_by_index(
	struct ap_grid * grid,
	uint32_t index);

boolean ap_grid_get_empty(
	struct ap_grid * grid,
	uint16_t * layer,
	uint16_t * row,
	uint16_t * column);

boolean ap_grid_get_empty_in_layer(
	struct ap_grid * grid,
	uint16_t layer,
	uint16_t * row,
	uint16_t * column);

void ap_grid_add_item(
	struct ap_grid * grid,
	uint16_t layer,
	uint16_t row,
	uint16_t column,
	enum ap_grid_item_type type,
	uint32_t id,
	uint32_t tid,
	void * object);

/**
 * Add grid item by index.
 * \param[in,out] grid   Grid pointer.
 * \param[in]     index  Grid item index.
 * \param[out]    layer  Layer that corresponds to index.
 * \param[out]    row    Row that corresponds to index.
 * \param[out]    column Column that corresponds to index.
 * \param[in]     type   Grid item type.
 * \param[in]     id     Grid item id.
 * \param[in]     tid    Grid item tid.
 * \param[in]     object Grid item object.
 */
void ap_grid_add_item_by_index(
	struct ap_grid * grid,
	uint32_t index,
	uint16_t * layer,
	uint16_t * row,
	uint16_t * column,
	enum ap_grid_item_type type,
	uint32_t id,
	uint32_t tid,
	void * object);


void * ap_grid_get_object(
	struct ap_grid * grid,
	uint16_t layer,
	uint16_t row,
	uint16_t column);

void * ap_grid_get_object_by_id(
	struct ap_grid * grid,
	uint32_t object_id);

/**
 * Move grid item within grid.
 * \param[in] grid       Grid pointer.
 * \param[in] prev_layer Layer that the item is currently in.
 * \param[in] prev_row   Row that the item is currently in.
 * \param[in] prev_col   Column that the item is currently in.
 * \param[in] layer      Layer to move the item to.
 * \param[in] row        Row to move the item to.
 * \param[in] col        Column to move the item to.
 *
 * \return TRUE if successful, FALSE if not.
 */
boolean ap_grid_move_item(
	struct ap_grid * grid,
	uint16_t prev_layer,
	uint16_t prev_row,
	uint16_t prev_col,
	uint16_t layer,
	uint16_t row,
	uint16_t col);

/**
 * Convert grid position to grid index.
 * \param[in] grid  Grid pointer.
 * \param[in] layer Grid item layer.
 * \param[in] row   Grid item row.
 * \param[in] col   Grid item column.
 *
 * \return Corresponding grid item index if successful.
 *         Otherwise AP_GRID_INVALID_INDEX.
 */
inline uint32_t ap_grid_get_index(
	const struct ap_grid * grid,
	uint16_t layer,
	uint16_t row,
	uint16_t column)
{
	uint32_t index = (((layer * grid->row_count) + row) * 
		grid->column_count) + column;
	return (index >= grid->grid_count) ? 
		AP_GRID_INVALID_INDEX : index;
}

inline void * ap_grid_get_object_by_index(
	struct ap_grid * grid,
	uint32_t index)
{
	return grid->items[index].object;
}

inline void * ap_grid_get_object_by_index_secure(
	struct ap_grid * grid,
	uint32_t index)
{
	if (index >= grid->grid_count)
		return NULL;
	return grid->items[index].object;
}

inline uint32_t ap_grid_get_empty_count(struct ap_grid * grid)
{
	assert(grid->item_count <= grid->grid_count);
	return (grid->grid_count - grid->item_count);
}

END_DECLS

#endif /* _AP_GRID_H_ */
