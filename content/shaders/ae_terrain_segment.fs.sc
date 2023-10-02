$input v_color0

#include "ac_terrain_common.h"

SAMPLER2D(s_texSegment, 0);

uniform vec4 u_segmentInfo[2];

#define u_begin    (u_segmentInfo[0].xy)
#define u_length   (u_segmentInfo[0].z)
#define u_opacity  (u_segmentInfo[1].r)

vec4 sample_segment(vec2 pos)
{
	float u = (pos.x - u_begin.x) / u_length;
	float v = (pos.y - u_begin.y) / u_length;
	return texture2D(s_texSegment, vec2(u, v));
}

void main()
{
	vec4 segment = sample_segment(v_color0.xz);
	float grid = calc_grid(v_color0.xz);
	// Apply radius with grid.
	gl_FragColor = vec4(
		segment.rgb * (1.0f - grid),
		max(segment.a, grid) * max(segment.a * u_opacity, grid * 0.8f));
}
