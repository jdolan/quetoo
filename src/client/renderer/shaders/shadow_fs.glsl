/**
 * @brief Planar shadows fragment shader.
 */

#version 330

#define FRAGMENT_SHADER

#include "include/gamma.glsl"

uniform vec4 LIGHT;
uniform vec4 PLANE;
uniform vec4 GLOBAL_COLOR;

in VertexData {
	vec4 point;
};

out vec4 fragColor;

/**
 * @brief
 */
void ShadowFragment(void) {
	
	vec3 delta = LIGHT.xyz - (point.xyz / point.w);
	
	float dist = length(delta);
	if (dist < LIGHT.w) {
	
		float s = (LIGHT.w - dist) / LIGHT.w;
		float d = dot(PLANE.xyz, normalize(delta));
		
		fragColor.a = min(s * d, GLOBAL_COLOR.a);
	} else {
		discard;
	}
}

/**
 * @brief Program entry point.
 */
void main(void) {

	fragColor = GLOBAL_COLOR;

	ShadowFragment();

	GammaFragment(fragColor);
}
