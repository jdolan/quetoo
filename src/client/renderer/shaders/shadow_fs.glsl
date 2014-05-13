/*
 * @brief Planar shadows fragment shader.
 */

#version 120

#define SHADOW_ATTENUATION 96.0

uniform float INTENSITY;

varying vec3 points[2];

/*
 * ShadowFragment
 */
void ShadowFragment(void) {
	float attenuation = length(points[1] - points[0]) / SHADOW_ATTENUATION;
	gl_FragColor.a *= 1.0 - clamp(attenuation, 0.0, 1.0);
}

/*
 * main
 */
void main(void) {

	gl_FragColor = gl_Color;

	ShadowFragment();
}
