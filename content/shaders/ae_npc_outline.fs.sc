#include "common.h"

SAMPLER2D(s_texBase,   0);
SAMPLER2D(s_texAlpha0, 1);
SAMPLER2D(s_texColor0, 2);
SAMPLER2D(s_texAlpha1, 3);
SAMPLER2D(s_texColor1, 4);

void main()
{
	gl_FragColor = vec4(0.913, 0.682, 0.450, 0.3);
}
