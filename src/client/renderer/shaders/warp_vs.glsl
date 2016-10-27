/**
 * @brief Warp vertex shader.
 */

#version 120

#include "fog_inc.glsl"
#include "matrix_inc.glsl"

varying vec2 texcoord;

attribute vec3 POSITION;
attribute vec2 TEXCOORD;

/**
 * @brief
 */
void FogVertex(void) {
	fog = (gl_Position.z - FOG.START) / (FOG.END - FOG.START);
	fog = clamp(fog, 0.0, 1.0) * FOG.DENSITY;
}

/**
 * @brief Program entry point.
 */
void main(void) {

	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * vec4(POSITION, 1.0);

	// pass texcoords through
	texcoord = TEXCOORD;

	FogVertex();
}
