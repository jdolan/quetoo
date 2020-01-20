/**
 * @brief Null fragment shader.
 */

#version 330

#define FRAGMENT_SHADER

#include "include/fog.glsl"
#include "include/gamma.glsl"
#include "include/matrix.glsl"
#include "include/tint.glsl"

uniform sampler2D SAMPLER0;

in VertexData {
	vec2 texcoord;
	vec4 color;
	vec3 point;
};

out vec4 fragColor;

/**
 * @brief Shader entry point.
 */
void main(void) {

	fragColor = color * texture(SAMPLER0, texcoord);

	TintFragment(fragColor, texcoord);

	FogFragment(length(point), fragColor);

	GammaFragment(fragColor);
}
