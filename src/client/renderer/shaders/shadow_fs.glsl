/*
 * @brief Planar shadows fragment shader.
 */

#version 120

uniform vec4 LIGHT;
uniform vec4 PLANE;

varying vec4 point;

/*
 * @brief
 */
void ShadowFragment(void) {
	
	vec3 delta = LIGHT.xyz - (point.xyz / point.w);
	
	float dist = length(delta);
	if (dist < LIGHT.w) {
	
		float s = (LIGHT.w - dist) / LIGHT.w;
		float d = dot(PLANE.xyz, normalize(delta));
		
		gl_FragColor.a = s * d * gl_Color.a;
	} else {
		discard;
	}
}

/*
 * @brief Program entry point.
 */
void main(void) {

	gl_FragColor = gl_Color;

	ShadowFragment();
}

