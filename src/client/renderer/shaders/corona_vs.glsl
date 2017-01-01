/**
 * @brief Corona vertex shader.
 */

#version 120

#define VERTEX_SHADER

#include "matrix_inc.glsl"
#include "fog_inc.glsl"

varying vec4 color;
varying vec2 texcoord;

attribute vec3 POSITION;
attribute vec2 TEXCOORD;
attribute vec4 COLOR;

/**
 * @brief Shader entry point.
 */
void main(void) {

	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * vec4(POSITION, 1.0);

	texcoord = TEXCOORD;

	// pass the color through as well
	color = COLOR;

	FogVertex();
}
