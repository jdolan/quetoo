/**
 * @brief Null vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "include/uniforms.glsl"
#include "include/fog.glsl"

in vec3 POSITION;
in vec2 TEXCOORD;
in vec4 COLOR;
in vec3 NEXT_POSITION;

out VertexData {
	vec2 texcoord;
	vec4 color;
	vec3 point;
};

/**
 * @brief Shader entry point.
 */
void main(void) {

	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * vec4(mix(POSITION, NEXT_POSITION, TIME_FRACTION), 1.0);

	point = gl_Position.xyz;

	texcoord = TEXCOORD;

	// pass the color through as well
	color = COLOR * GLOBAL_COLOR;
}
