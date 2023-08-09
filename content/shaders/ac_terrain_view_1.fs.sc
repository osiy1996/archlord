$input v_color0, v_texcoord0, v_texcoord1, v_texcoord2, v_texcoord3, v_texcoord4, v_texcoord5

#include "common.h"

SAMPLER2D(s_texAlpha1, 3);
SAMPLER2D(s_texColor1, 4);

void main()
{
	vec4 c = vec4(texture2D(s_texColor1, v_texcoord4).rgb, texture2D(s_texAlpha1, v_texcoord3).a);
	gl_FragColor = vec4(saturate(c.xyz * v_color0.xyz), c.a);
}
