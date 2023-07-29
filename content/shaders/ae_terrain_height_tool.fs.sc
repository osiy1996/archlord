$input v_color0

#include "ac_terrain_common.h"

uniform vec4 u_heightTool;

#define u_center (u_heightTool.xy)
#define u_radius (u_heightTool.z)

void main()
{
	float d = distance(u_center, v_color0.xz);
	float b = step(0.01f, u_radius - min(d, u_radius)) * 
		step(u_radius - min(d, u_radius), 50.0f);
	float grid = calc_grid(v_color0.xz);
	vec3 c = vec3(0.0f, 0.0f, 1.0f);
	// Apply radius with grid.
	gl_FragColor = vec4(
		saturate(b * c),
		max(b, grid) * 0.5f);
}
