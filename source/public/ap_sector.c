#include "core/vector.h"

#include "public/ap_sector.h"

static int sort_sector_index(const void * e1, const void * e2)
{
	const struct ap_scr_index * s1 = e1;
	const struct ap_scr_index * s2 = e2;
	uint32_t mx[2] = { s1->x / 16, s2->x / 16 };
	uint32_t mz[2] = { s1->z / 16, s2->z / 16 };
	if (mz[0] < mz[1])
		return -1;
	else if (mz[0] > mz[1])
		return 1;
	if (mx[0] < mx[1])
		return -1;
	else if (mx[0] > mx[1])
		return 1;
	if (s1->z < s2->z)
		return -1;
	else if (s1->z > s2->z)
		return 1;
	if (s1->x < s2->x)
		return -1;
	else if (s1->x > s2->x)
		return 1;
	return 0;
}

boolean ap_scr_pos_to_index(
	const float * pos,
	uint32_t * x,
	uint32_t * z)
{
	if (pos[0] < AP_SECTOR_WORLD_START_X ||
		pos[0] >= AP_SECTOR_WORLD_END_X ||
		pos[2] < AP_SECTOR_WORLD_START_Z ||
		pos[2] >= AP_SECTOR_WORLD_END_Z)
		return FALSE;
	*x = (uint32_t)((pos[0] - AP_SECTOR_WORLD_START_X) / 
		AP_SECTOR_WIDTH);
	*z = (uint32_t)((pos[2] - AP_SECTOR_WORLD_START_Z) / 
		AP_SECTOR_HEIGHT);
	return TRUE;
}

boolean ap_scr_find_index(
	struct ap_scr_index * v,
	const struct ap_scr_index * si)
{
	uint32_t i;
	uint32_t c = vec_count(v);
	for (i = 0; i < c; i++) {
		if (v[i].x == si->x && v[i].z == si->z)
			return TRUE;
	}
	return FALSE;
}
float ap_scr_distance(
	const vec3 pos,
	uint32_t sx,
	uint32_t sz)
{
	float rmin[2] = {
		AP_SECTOR_WORLD_START_X + sx * AP_SECTOR_WIDTH,
		AP_SECTOR_WORLD_START_Z + sz * AP_SECTOR_HEIGHT };
	float rmax[2] = {
		AP_SECTOR_WORLD_START_X + (sx + 1) * AP_SECTOR_WIDTH,
		AP_SECTOR_WORLD_START_Z + (sz + 1) * AP_SECTOR_HEIGHT };
	float dx = MAX(rmin[0] - pos[0], MAX(pos[0] - rmax[0], 0.f));
	float dz = MAX(rmin[1] - pos[2], MAX(pos[2] - rmax[1], 0.f));
	return sqrtf(dx * dx + dz * dz);
}

void ap_scr_sort_indices(
	struct ap_scr_index * indices,
	uint32_t count)
{
	qsort(indices, count, sizeof(*indices), sort_sector_index);
}
