/**
 * @brief Warp vertex shader.
 */

#version 120

#include "fog_inc.glsl"
#include "matrix_inc.glsl"

varying vec4 color;
varying vec2 texcoord;

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
	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * gl_Vertex;

	// pass texcoords through
	texcoord = vec2(gl_MultiTexCoord0);

	// and primary color
	color = gl_Color;

	FogVertex();
}
