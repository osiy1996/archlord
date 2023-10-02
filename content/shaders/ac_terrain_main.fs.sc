$input v_color0, v_texcoord0, v_texcoord1, v_texcoord2, v_texcoord3, v_texcoord4, v_texcoord5

#include "common.h"

SAMPLER2D(s_texBase,   0);
SAMPLER2D(s_texAlpha0, 1);
SAMPLER2D(s_texColor0, 2);
SAMPLER2D(s_texAlpha1, 3);
SAMPLER2D(s_texColor1, 4);

void main()
{
	vec4 c = texture2D(s_texBase, v_texcoord0);
	vec4 l0 = vec4(texture2D(s_texColor0, v_texcoord2).rgb, texture2D(s_texAlpha0, v_texcoord1).a);
	vec4 l1 = vec4(texture2D(s_texColor1, v_texcoord4).rgb, texture2D(s_texAlpha1, v_texcoord3).a);
	c = vec4(l0.rgb * l0.a + c.rgb * (1.0 - l0.a), 1.0);
	c = vec4(l1.rgb * l1.a + c.rgb * (1.0 - l1.a), 1.0);
	gl_FragColor = vec4(saturate(c.xyz * v_color0.xyz), 1.0);
	//gl_FragColor = vec4(0.3, 0.03, 0.03, 1.0);
}
