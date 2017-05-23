/**
 * @brief Null vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "matrix_inc.glsl"

in vec3 POSITION;
in vec2 TEXCOORD;
in vec4 COLOR;

out VertexData {
	vec4 color;
	vec2 texcoord;
};

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
