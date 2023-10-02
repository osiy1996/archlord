$input v_color0, v_texcoord0, v_texcoord1, v_texcoord2, v_texcoord3, v_texcoord4, v_texcoord5

#include "ac_terrain_common.h"

SAMPLER2D(s_texAlpha, 0);

void main()
{
	float grid = calc_grid(v_color0.xz);
	float a = texture2D(s_texAlpha, v_texcoord0).a;
	gl_FragColor = vec4(0.70f, 0.70f, 0.70f, (1.0f - grid) * a * 0.5f);
}
