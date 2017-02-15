/**
 * @brief Null vertex shader.
 */

#version 130

#define VERTEX_SHADER

#include "matrix_inc.glsl"
#include "fog_inc.glsl"

uniform vec4 GLOBAL_COLOR;
uniform float TIME_FRACTION;

out vec4 color;
out vec2 texcoord;

in vec3 POSITION;
in vec2 TEXCOORD;
in vec4 COLOR;
in vec3 NEXT_POSITION;

/**
 * @brief Shader entry point.
 */
void main(void) {

	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * vec4(mix(POSITION, NEXT_POSITION, TIME_FRACTION), 1.0);

	texcoord = TEXCOORD;

	// pass the color through as well
	color = COLOR * GLOBAL_COLOR;

	FogVertex();
}
