$input v_color0

#include "ac_terrain_common.h"

uniform vec4 u_levelTool;

#define u_center (u_levelTool.xy)
#define u_radius (u_levelTool.z)

void main()
{
	float d = distance(u_center, v_color0.xz);
	float g = step(0.01f, u_radius - min(d, u_radius)) * 
		step(u_radius - min(d, u_radius), 50.0f);
	float grid = calc_grid(v_color0.xz);
	vec3 c = vec3(0.0f, 1.0f, 0.0f);
	// Apply circle with grid.
	gl_FragColor = vec4(
		g * c,
		max(grid, g) * 0.5f);
}
