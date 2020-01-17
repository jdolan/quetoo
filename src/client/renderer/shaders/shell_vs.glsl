/**
 * @brief Shell vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "include/matrix.glsl"

uniform float OFFSET;
uniform float SHELL_OFFSET;
uniform float TIME_FRACTION;

in vec3 POSITION;
in vec3 NORMAL;
in vec2 TEXCOORD;
in vec3 NEXT_POSITION;
in vec3 NEXT_NORMAL;

out VertexData {
	vec4 color;
	vec2 texcoord;
};

/**
 * @brief Shader entry point.
 */
void main(void) {

	vec4 position = vec4(mix(POSITION, NEXT_POSITION, TIME_FRACTION) +
						 mix(NORMAL, NEXT_NORMAL, TIME_FRACTION) * SHELL_OFFSET, 1.0);

	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * position;

	// pass texcoords through
	texcoord = TEXCOORD + OFFSET;
}
