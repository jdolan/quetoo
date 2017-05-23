/**
 * @brief Planar shadows fragment shader.
 */

#version 330

#define FRAGMENT_SHADER

uniform vec4 LIGHT;
uniform vec4 PLANE;
uniform vec4 GLOBAL_COLOR;

#define FOG_NO_UNIFORM
#include "fog_inc.glsl"

in VertexData {
	vec4 point;
	FOG_VARIABLE;
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
 * @brief
 */
void FogFragment(void) {
    fragColor.a = mix(fragColor.a, 0.0, fog);
}

/**
 * @brief Program entry point.
 */
void main(void) {

	fragColor = GLOBAL_COLOR;

	ShadowFragment();
    
    FogFragment();
}

