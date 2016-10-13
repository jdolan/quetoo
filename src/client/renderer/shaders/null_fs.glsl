/**
 * @brief Null fragment shader.
 */

#version 120

#include "matrix_inc.glsl"
#include "fog_inc.glsl"

uniform sampler2D SAMPLER0;

varying vec4 color;
varying vec2 texcoord;

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

	gl_FragColor = color * texture2D(SAMPLER0, texcoord);

	FogFragment(); // and lastly add fog	
}
