#ifndef QUETOO_TINT_GLSL
#define QUETOO_TINT_GLSL

// RGB layer tints
uniform vec4 TINTS[3];
uniform bool TINTMAP;
uniform sampler2D SAMPLER6;

void TintFragment(inout vec4 diffuse, in vec2 texcoord)
{
	if (TINTMAP) {
		vec4 tint = texture(SAMPLER6, texcoord);

		if (tint.a > 0) {
			for (int i = 0; i < 3; i++) {
				if (TINTS[i].a > 0 && tint[i] > 0) {
					diffuse.rgb = mix(diffuse.rgb, TINTS[i].rgb * tint[i], tint.a);
				}
			}
		}
	}
}

#endif //QUETOO_TINT_GLSL
