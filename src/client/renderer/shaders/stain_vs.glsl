/**
 * @brief Null vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "matrix_inc.glsl"

out vec4 color;
out vec2 texcoord;

in vec3 POSITION;
in vec2 TEXCOORD;
in vec4 COLOR;

/**
 * @brief Shader entry point.
 */
void main(void) {

	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * vec4(POSITION, 1.0);

	texcoord = TEXCOORD;

	// pass the color through as well
	color = COLOR;
}
