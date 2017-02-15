/**
 * @brief Corona fragment shader.
 */

#version 330

#define FRAGMENT_SHADER

#include "matrix_inc.glsl"
#include "fog_inc.glsl"

in vec4 color;
in vec2 texcoord;

const vec2 center_point = vec2(0.5, 0.5);

out vec4 fragColor;

/**
 * @brief Apply fog to the fragment if enabled.
 */
void FogFragment(void) {
	fragColor.rgb = mix(fragColor.rgb, FOG.COLOR, fog);
}

/**
 * @brief Shader entry point.
 */
void main(void) {

	fragColor = vec4(color.rgb, mix(color.a, 0, length(texcoord - center_point) * 2.0));

	FogFragment(); // and lastly add fog	
}
