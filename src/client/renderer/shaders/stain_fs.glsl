/**
 * @brief Null fragment shader.
 */

#version 330

#define FRAGMENT_SHADER

#include "include/gamma.glsl"
#include "include/matrix.glsl"

uniform sampler2D SAMPLER0;

in VertexData {
	vec2 texcoord;
	vec4 color;
};

out vec4 fragColor;

/**
 * @brief Shader entry point.
 */
void main(void) {

	vec4 stain_color = texture(SAMPLER0, texcoord);
	fragColor = vec4(stain_color.rgb * stain_color.a, stain_color.a) * color;

	GammaFragment(fragColor);
}
