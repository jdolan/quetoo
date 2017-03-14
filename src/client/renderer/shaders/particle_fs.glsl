/**
 * @brief Null fragment shader.
 */

#version 330

#define FRAGMENT_SHADER

#include "include/matrix.glsl"
#include "include/fog.glsl"

uniform sampler2D SAMPLER0;

in VertexData {
	vec4 color;
	vec2 texcoord;
	float fog;
};

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

	FogFragment(fragColor, fog);
}
