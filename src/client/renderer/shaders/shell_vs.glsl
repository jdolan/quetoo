/**
 * @brief Shell vertex shader.
 */

#version 120

#include "matrix_inc.glsl"

uniform float OFFSET;
uniform float SHELL_OFFSET;

varying vec4 color;
varying vec2 texcoord;

attribute vec3 POSITION;
attribute vec3 NORMAL;
attribute vec2 TEXCOORD;

uniform float TIME_FRACTION;

attribute vec3 NEXT_POSITION;
attribute vec3 NEXT_NORMAL;


/**
 * @brief Shader entry point.
 */
void main(void) {

	vec4 position = vec4(mix(POSITION, NEXT_POSITION, TIME_FRACTION) + mix(NORMAL, NEXT_NORMAL, TIME_FRACTION) * SHELL_OFFSET, 1.0);

	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * position;

	// pass texcoords through
	texcoord = TEXCOORD + OFFSET;
}
