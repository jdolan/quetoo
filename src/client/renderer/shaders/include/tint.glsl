#ifndef QUETOO_TINT_GLSL
#define QUETOO_TINT_GLSL

// RGB layer tints
uniform vec4 TINTS[3];
uniform bool TINTMAP;
uniform sampler2D SAMPLER6;

void TintFragment(inout vec4 diffuse, in vec2 texcoord)
{
	// tintmaps use premultiplied alpha textures
	if (TINTMAP) {
		vec4 tint = texture(SAMPLER6, texcoord);
		diffuse.rgb *= 1.0 - tint.a;
		diffuse.rgb += (TINTS[0].rgb * tint.r);
		diffuse.rgb += (TINTS[1].rgb * tint.g);
		diffuse.rgb += (TINTS[2].rgb * tint.b);
	}
}

#endif //QUETOO_TINT_GLSL
