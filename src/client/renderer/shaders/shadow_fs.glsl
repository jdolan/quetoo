/*
 * @brief Planar shadows fragment shader.
 */

#version 120

#define SHADOW_ATTENUATION 192.0

uniform vec3 LIGHT_POS;

varying vec3 point;

/*
 * ShadowFragment
 */
void ShadowFragment(void) {
	float attenuation = length(point - LIGHT_POS) / SHADOW_ATTENUATION;
	gl_FragColor.a *= 1.0 - clamp(attenuation, 0.0, 1.0);
}

/*
 * main
 */
void main(void) {

	gl_FragColor = gl_Color;

	ShadowFragment();
}
