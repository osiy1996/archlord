$input a_position, a_normal, a_texcoord0, a_texcoord1, a_texcoord2, a_texcoord3, a_texcoord4, a_texcoord5, a_texcoord6, a_texcoord7

#include "common.h"

void main()
{
	//vec4 posClip = mul(u_modelViewProj, vec4(a_position, 1.0));
	//vec4 normalClip = mul(u_modelViewProj, vec4(a_normal, 0.0));
	//posClip.xy += normalize(normalClip.xy) / u_viewRect.zw * posClip.w * 3.0f;
	//posClip.z += normalClip.z;
	//gl_Position = posClip;
	//vec3 pos = mul(u_modelView, vec4(a_position, 1.0)).xyz;
	//vec3 normal = mul(u_modelView, vec4(a_normal, 0.0)).xyz;
	//pos.xyz += normalize(normal) * 5.0f;
	//gl_Position = mul(u_proj, vec4(pos, 1.0f));
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
}
