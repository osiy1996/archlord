#ifndef _AR_TRN_COMMON_H_
#define _AR_TRN_COMMON_H_

#include "common.h"

float calc_grid_dim(float f, float dim)
{
	float g0 = (int)(f / dim) * dim;
	float s = sign(f - g0);
	float g1 = f + s * dim;
	return max(step(distance(g0, f), 20.0f), step(distance(g1, f), 20.0f));
}

float calc_grid(vec2 pos)
{
	return max(calc_grid_dim(pos.x, 400.0f), calc_grid_dim(pos.y, 400.0f));
}

float calc_grid_ex(vec2 pos, float dim)
{
	return max(calc_grid_dim(pos.x, dim), calc_grid_dim(pos.y, dim));
}

#endif // _AR_TRN_COMMON_H_
