/**
 * @brief Null vertex shader.
 */

#version 120

#include "matrix_inc.glsl"
#include "fog_inc.glsl"

uniform vec4 GLOBAL_COLOR;

varying vec4 color;
varying vec2 texcoord;

attribute vec3 A_POSITION;
attribute vec2 A_TEXCOORD;
attribute vec4 A_COLOR;

/**
 * @brief Calculate the interpolated fog value for the vertex.
 */
void FogVertex(void) {

	fog = (gl_Position.z - FOG.START) / (FOG.END - FOG.START);
	fog = clamp(fog, 0.0, 1.0) * FOG.DENSITY;
}

/**
 * @brief Shader entry point.
 */
void main(void) {

	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * vec4(A_POSITION, 1.0);

	texcoord = vec2(TEXTURE_MAT * vec4(A_TEXCOORD, 0, 1));

	// pass the color through as well
	color = A_COLOR * GLOBAL_COLOR;

	FogVertex();
}
