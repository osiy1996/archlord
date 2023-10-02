$input a_position, a_normal, a_texcoord0, a_texcoord1, a_texcoord2, a_texcoord3, a_texcoord4, a_texcoord5, a_texcoord6, a_texcoord7
$output v_color0, v_texcoord0, v_texcoord1, v_texcoord2, v_texcoord3, v_texcoord4, v_texcoord5

#include "common.h"

vec4 do_light(vec3 n)
{
	vec3 light_dir = normalize(vec3(1, -1, 0));
	float diffuse = saturate(dot(n, -light_dir));
	float ambient = 0.3;
	float c = ambient + diffuse;
	return vec4(c, c, c, 1.0);
}

void main()
{
	vec4 normal = mul(u_model[0], vec4(a_normal, 1.0));
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
	v_texcoord0 = a_texcoord0;
	v_texcoord1 = a_texcoord1;
	v_texcoord2 = a_texcoord2;
	v_texcoord3 = a_texcoord3;
	v_texcoord4 = a_texcoord4;
	v_texcoord5 = a_texcoord5;
	v_color0 = do_light(normal.xyz);
}
