/**
 * @brief Warp vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "include/uniforms.glsl"
#include "include/fog.glsl"

in vec3 POSITION;
in vec2 TEXCOORD;

out VertexData {
	vec2 texcoord;
	vec3 point;
};

/**
 * @brief Program entry point.
 */
void main(void) {

	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * vec4(POSITION, 1.0);

	point = gl_Position.xyz;

	// pass texcoords through
	texcoord = TEXCOORD;
}
