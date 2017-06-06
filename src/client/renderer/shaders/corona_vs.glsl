/**
 * @brief Corona vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "include/uniforms.glsl"
#include "include/fog.glsl"

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

	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * vec4(POSITION, 1.0);

	texcoord = TEXCOORD;

	color = COLOR;
}
