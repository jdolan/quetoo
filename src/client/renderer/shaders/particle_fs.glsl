/**
 * @brief Particle fragment shader.
 */

#version 330

#define FRAGMENT_SHADER

#include "include/matrix.glsl"
#include "include/fog.glsl"

uniform sampler2D SAMPLER0;

in VertexData {
	vec4 color;
	vec2 texcoord;
	vec3 point;
};

out vec4 fragColor;

/**
 * @brief Shader entry point.
 */
void main(void) {

	fragColor = color * texture(SAMPLER0, texcoord);

	FogFragment(length(point), fragColor);
}
