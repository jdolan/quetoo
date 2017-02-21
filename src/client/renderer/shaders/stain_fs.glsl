/**
 * @brief Null fragment shader.
 */

#version 330

#define FRAGMENT_SHADER

#include "matrix_inc.glsl"

uniform sampler2D SAMPLER0;

in vec4 color;
in vec2 texcoord;

out vec4 fragColor;

/**
 * @brief Shader entry point.
 */
void main(void) {

	fragColor = texture(SAMPLER0, texcoord) * color;
}
