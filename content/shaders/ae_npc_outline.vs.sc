$input a_position, a_normal, a_texcoord0, a_texcoord1, a_texcoord2, a_texcoord3, a_texcoord4, a_texcoord5, a_texcoord6, a_texcoord7

#include "common.h"

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
}
