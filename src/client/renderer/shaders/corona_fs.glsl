/**
 * @brief Corona fragment shader.
 */

#version 330

#define FRAGMENT_SHADER

#include "include/matrix.glsl"
#include "include/fog.glsl"

in VertexData {
	vec4 color;
	vec2 texcoord;
};

const vec2 center_point = vec2(0.5, 0.5);

out vec4 fragColor;

/**
 * @brief Shader entry point.
 */
void main(void) {

	fragColor = vec4(color.rgb, mix(color.a, 0, length(texcoord - center_point) * 2.0));

	FogFragment(fragColor);
}
