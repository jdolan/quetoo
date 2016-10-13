/**
 * @brief Shell vertex shader.
 */

#version 120

#include "matrix_inc.glsl"

uniform float OFFSET;

varying vec4 color;
varying vec2 texcoord;

/**
 * @brief Shader entry point.
 */
void main(void) {

	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * gl_Vertex;

	// pass texcoords through
	texcoord = vec2(gl_MultiTexCoord0 + OFFSET);
	
	// pass the color through as well
	color = gl_Color;
}
