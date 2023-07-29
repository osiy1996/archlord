#ifndef _AP_SECTOR_H_
#define _AP_SECTOR_H_

#include "core/macros.h"
#include "core/types.h"

#include "vendor/cglm/cglm.h"

/* Size of the entire X-axis. */
#define	AP_SECTOR_WORLD_INDEX_WIDTH 800
/* Size of the entire Z-axis. */
#define	AP_SECTOR_WORLD_INDEX_HEIGHT 800
/* Width of one sector. */
#define	AP_SECTOR_WIDTH (6400.0f)
/* Height of one sector. */
#define	AP_SECTOR_HEIGHT (6400.0f)
#define AP_SECTOR_DEFAULT_DEPTH 16
#define AP_SECTOR_STEPSIZE (400.0f)
#define AP_SECTOR_WORLD_START_X \
	(-(AP_SECTOR_WORLD_INDEX_WIDTH / 2) * AP_SECTOR_WIDTH)
#define AP_SECTOR_WORLD_START_Z \
	(-(AP_SECTOR_WORLD_INDEX_HEIGHT / 2) * AP_SECTOR_HEIGHT)
#define AP_SECTOR_WORLD_END_X \
	((AP_SECTOR_WORLD_INDEX_WIDTH / 2) * AP_SECTOR_WIDTH)
#define AP_SECTOR_WORLD_END_Z \
	((AP_SECTOR_WORLD_INDEX_HEIGHT / 2) * AP_SECTOR_HEIGHT)

#define	AP_SECTOR_MIN_HEIGHT -20000.0f
#define	AP_SECTOR_MAX_HEIGHT 100000.0f

BEGIN_DECLS

struct ap_scr_index {
	uint32_t x;
	uint32_t z;
};

boolean ap_scr_pos_to_index(
	const float * pos,
	uint32_t * x,
	uint32_t * z);

boolean ap_scr_find_index(
	struct ap_scr_index * v,
	const struct ap_scr_index * si);

/* 
 * Bird-view (x-z plane) distance between 
 * a 3D point to a sector.
 *
 * If point is inside the sector, return value will be 0.
 */
float ap_scr_distance(
	const vec3 pos,
	uint32_t sx,
	uint32_t sz);

void ap_scr_sort_indices(
	struct ap_scr_index * indices,
	uint32_t count);

inline float ap_scr_get_start_x(uint32_t sx)
{
	return (AP_SECTOR_WORLD_START_X + sx * AP_SECTOR_WIDTH);
}

inline float ap_scr_get_start_z(uint32_t sz)
{
	return (AP_SECTOR_WORLD_START_Z + sz * AP_SECTOR_WIDTH);
}

inline float ap_scr_get_end_x(uint32_t sx)
{
	return (AP_SECTOR_WORLD_START_X + (sx + 1) * AP_SECTOR_WIDTH);
}

inline float ap_scr_get_end_z(uint32_t sz)
{
	return (AP_SECTOR_WORLD_START_Z + (sz + 1) * AP_SECTOR_WIDTH);
}

inline boolean ap_scr_from_division_index(
	uint32_t index,
	uint32_t * sector_x,
	uint32_t * sector_z)
{
	uint32_t z = index % 100;
	uint32_t x = ((index % 10000) - z) / 100;
	x *= AP_SECTOR_DEFAULT_DEPTH;
	z *= AP_SECTOR_DEFAULT_DEPTH;
	if (x >= AP_SECTOR_WORLD_INDEX_WIDTH ||
		z >= AP_SECTOR_WORLD_INDEX_HEIGHT) {
		return FALSE;
	}
	*sector_x = x;
	*sector_z = z;
	return TRUE;
}

inline boolean ap_scr_div_index_from_sector_index(
	uint32_t sector_x,
	uint32_t sector_z,
	uint32_t * div_index)
{
	if (sector_x >= AP_SECTOR_WORLD_INDEX_WIDTH ||
		sector_z >= AP_SECTOR_WORLD_INDEX_HEIGHT) {
		return FALSE;
	}
	*div_index = (sector_x / AP_SECTOR_DEFAULT_DEPTH) * 100 +
		(sector_z / AP_SECTOR_DEFAULT_DEPTH);
	return TRUE;
}

inline boolean ap_scr_is_index_valid(uint32_t x, uint32_t z)
{
	return (x < AP_SECTOR_WORLD_INDEX_WIDTH && 
		z < AP_SECTOR_WORLD_INDEX_HEIGHT);
}

inline boolean ap_scr_is_same_division(uint32_t s1, uint32_t s2)
{
	return ((s1 / AP_SECTOR_DEFAULT_DEPTH) == 
		(s2 / AP_SECTOR_DEFAULT_DEPTH));
}

END_DECLS

#endif /* _AP_SECTOR_H_ */
