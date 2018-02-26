/**
 * @brief Corona vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "include/matrix.glsl"
#include "include/fog.glsl"

in vec3 POSITION;
in vec2 TEXCOORD;
in vec4 COLOR;

out VertexData {
	vec4 color;
	vec2 texcoord;
	vec3 point;
};

/**
 * @brief Shader entry point.
 */
void main(void) {

	point = (MODELVIEW_MAT * vec4(POSITION, 1.0)).xyz;

	gl_Position = PROJECTION_MAT * vec4(point, 1.0);

	texcoord = TEXCOORD;

	color = COLOR;
}
