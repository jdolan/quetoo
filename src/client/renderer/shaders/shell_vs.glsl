/**
 * @brief Shell vertex shader.
 */

#version 120

#include "matrix_inc.glsl"

uniform float OFFSET;

varying vec4 color;
varying vec2 texcoord;

attribute vec3 POSITION;
attribute vec2 TEXCOORD;

/**
 * @brief Shader entry point.
 */
void main(void) {

	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * vec4(POSITION, 1.0);

	// pass texcoords through
	texcoord = TEXCOORD + OFFSET;
}
