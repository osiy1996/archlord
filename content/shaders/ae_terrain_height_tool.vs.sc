$input a_position
$output v_color0

#include "common.h"

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
	v_color0 = vec4(mul(u_model[0], vec4(a_position, 1.0)).xyz, 1.0);
}
