$input v_color0, v_texcoord0, v_texcoord1, v_texcoord2, v_texcoord3, v_texcoord4, v_texcoord5

#include "ac_terrain_common.h"

void main()
{
	float grid = calc_grid(v_color0.xz);
	gl_FragColor = vec4(0.0f, 0.0f, 0.0f, grid * 0.5f);
}
