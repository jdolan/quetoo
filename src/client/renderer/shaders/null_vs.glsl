/**
 * @brief Null vertex shader.
 */

#version 120

#include "matrix_inc.glsl"
#include "fog_inc.glsl"

varying vec4 color;
varying vec2 texcoord;

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
	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * gl_Vertex;

	texcoord = vec2(gl_MultiTexCoord0);

	// pass the color through as well
	color = gl_Color;

	FogVertex();
}
