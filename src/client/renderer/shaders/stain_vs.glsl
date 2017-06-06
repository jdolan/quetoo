/**
 * @brief Null vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "include/uniforms.glsl"

in vec3 POSITION;
in vec2 TEXCOORD;
in vec4 COLOR;

out VertexData {
	vec2 texcoord;
	vec4 color;
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
