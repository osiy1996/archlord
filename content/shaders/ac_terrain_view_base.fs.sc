$input v_color0, v_texcoord0, v_texcoord1, v_texcoord2, v_texcoord3, v_texcoord4, v_texcoord5

#include "common.h"

SAMPLER2D(s_texBase,   0);

void main()
{
	vec4 c = texture2D(s_texBase, v_texcoord0);
	gl_FragColor = vec4(saturate(c.xyz * v_color0.xyz), 1.0);
}
