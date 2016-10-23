/**
 * @brief Corona fragment shader.
 */

#version 120

#include "matrix_inc.glsl"
#include "fog_inc.glsl"

varying vec4 color;
varying vec2 texcoord;

const vec2 center_point = vec2(0.5, 0.5);

/**
 * @brief Apply fog to the fragment if enabled.
 */
void FogFragment(void) {
	gl_FragColor.rgb = mix(gl_FragColor.rgb, FOG.COLOR, fog);
}

/**
 * @brief Shader entry point.
 */
void main(void) {

	gl_FragColor = vec4(color.rgb, mix(color.a, 0, length(texcoord - center_point) * 2.0));

	FogFragment(); // and lastly add fog	
}
