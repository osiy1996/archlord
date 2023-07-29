#ifndef _AU_MATH_H_
#define _AU_MATH_H_

#include "core/macros.h"
#include "core/types.h"

#include "vendor/cglm/cglm.h"
#include "vendor/RenderWare/rwcore.h"

#include <float.h>

BEGIN_DECLS

enum interp_type {
	INTERP_CONSTANT,
	INTERP_LINEAR,
	INTERP_SHARP,
	INTERP_ROOT,
	INTERP_SPHERE,
	INTERP_SMOOTH,
	INTERP_COUNT
};

/*
 * Constructs a bounding-sphere that contains 
 * all given points with minimum radius.
 *
 * `stride` is the number of bytes between each position.
 */
inline void bsphere_from_points(
	RwSphere * bsphere,
	const void * vertices,
	uint32_t vertex_count,
	size_t stride)
{
	RwV3d low;
	RwV3d high;
	uint32_t i;
	float r = FLT_MIN;
	const RwV3d * p;;
	if (!vertex_count) {
		bsphere->center.x = 0.f;
		bsphere->center.y = 0.f;
		bsphere->center.z = 0.f;
		bsphere->radius = 0.f;
		return;
	}
	p = (const RwV3d *)vertices;
	low = *p;
	high = *p;
	for (i = 0; i < vertex_count; i++) {
		const RwV3d * p = (const RwV3d *)((uintptr_t)vertices +
				i * stride);
		if (p->x < low.x)
			low.x = p->x;
		if (p->y < low.y)
			low.y = p->y;
		if (p->z < low.z)
			low.z = p->z;
		if (p->x > high.x)
			high.x = p->x;
		if (p->y > high.y)
			high.y = p->y;
		if (p->z > high.z)
			high.z = p->z;
	}
	bsphere->center.x = (low.x + high.x) / 2.f;
	bsphere->center.y = (low.y + high.y) / 2.f;
	bsphere->center.z = (low.z + high.z) / 2.f;
	for (i = 0; i < vertex_count; i++) {
		const RwV3d * p = (const RwV3d *)((uintptr_t)vertices +
				i * stride);
		vec3 p1 = { 
			bsphere->center.x,
			bsphere->center.y,
			bsphere->center.z };
		vec3 p2 = { p->x, p->y, p->z };
		float d = glm_vec3_distance(p1, p2);
		if (d > r)
			r = d;
	}
	bsphere->radius = r;
}

inline boolean bsphere_raycast(
	const RwSphere * bsphere,
	vec3 origin,
	vec3 dir,
	float * distance)
{
	float b;
	float c;
	vec3 center = { bsphere->center.x, bsphere->center.y,
		bsphere->center.z };
	vec3 to_origin;
	float r;
	glm_vec3_sub(origin, center, to_origin);
	b = glm_vec3_dot(dir, to_origin);
	c = glm_vec3_dot(to_origin, to_origin) -
		bsphere->radius * bsphere->radius;
	r = b * b - c;
	if (r < 0)
		return FALSE;
	if (r == 0) {
		*distance = -b;
	}
	else {
		float root = sqrtf(r);
		float d1 = -b + root;
		float d2 = -b - root;
		*distance = (d1 < d2) ? d1 : d2;
	}
	return TRUE;
}

inline float roundmul(float f, float n)
{
	if (n == 0.f)
		return 0.f;
	return (roundf(f / n) * n);
}

inline boolean aabb_vec2(
	const vec2 start,
	const vec2 end,
	const vec2 p)
{
	return (
		p[0] >= start[0] && p[1] >= start[1] &&
		p[0] <= end[0] && p[1] <= end[1]);
}

inline float _vec2_tri_sign(
	const vec2 t0,
	const vec2 t1,
	const vec2 t2)
{
	return ((t0[0] - t2[0]) * (t1[1] - t2[1]) - 
		(t1[0] - t2[0]) * (t0[1] - t2[1]));
}

/*
 * Returns TRUE if 2D point is inside triangle.
 */
inline boolean vec2_triangle_point(
	const vec2 t0,
	const vec2 t1,
	const vec2 t2,
	const vec2 p)
{
	float d1, d2, d3;
	boolean has_neg, has_pos;
	d1 = _vec2_tri_sign(p, t0, t1);
	d2 = _vec2_tri_sign(p, t1, t2);
	d3 = _vec2_tri_sign(p, t2, t0);
	has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
	has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
	return !(has_neg && has_pos);
}

inline float interp(
	enum interp_type type,
	float value,
	float distance)
{
	distance = CLAMP(distance, 0.0f, 1.0f);
	switch (type) {
	case INTERP_CONSTANT:
		return value;
	case INTERP_LINEAR:
		break;
	case INTERP_SHARP:
		distance *= distance;
		break;
	case INTERP_ROOT:
		distance = sqrtf(distance);
		break;
	case INTERP_SPHERE:
		distance = sqrtf(distance * 2 - distance * distance);
		break;
	case INTERP_SMOOTH:
		distance = 3.0f * distance * distance - 
			2 * distance * distance * distance;
		break;
	default:
		break;
	}
	return value * distance;
}

END_DECLS

#endif /* _AU_MATH_H_ */