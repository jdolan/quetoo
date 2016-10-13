/**
 * @brief Planar shadows fragment shader.
 */

#version 120

uniform vec4 LIGHT;
uniform vec4 PLANE;

varying vec4 color;
varying vec4 point;

#define FOG_NO_UNIFORM
#include "fog_inc.glsl"

/**
 * @brief
 */
void ShadowFragment(void) {
	
	vec3 delta = LIGHT.xyz - (point.xyz / point.w);
	
	float dist = length(delta);
	if (dist < LIGHT.w) {
	
		float s = (LIGHT.w - dist) / LIGHT.w;
		float d = dot(PLANE.xyz, normalize(delta));
		
		gl_FragColor.a = min(s * d, color.a);
	} else {
		discard;
	}
}

/**
 * @brief
 */
void FogFragment(void) {
    gl_FragColor.a = mix(gl_FragColor.a, 0.0, fog);
}

/**
 * @brief Program entry point.
 */
void main(void) {

	gl_FragColor = color;

	ShadowFragment();
    
    FogFragment();
}

