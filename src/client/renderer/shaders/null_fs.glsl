/**
 * @brief Null fragment shader.
 */

#version 330

#define FRAGMENT_SHADER

#include "matrix_inc.glsl"
#include "fog_inc.glsl"

uniform sampler2D SAMPLER0;

in vec4 color;
in vec2 texcoord;

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

	fragColor = color * texture(SAMPLER0, texcoord);

	FogFragment(); // and lastly add fog	
}
