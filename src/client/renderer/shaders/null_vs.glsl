/**
 * @brief Null vertex shader.
 */

#version 120

#include "matrix_inc.glsl"
#include "fog_inc.glsl"

uniform vec4 GLOBAL_COLOR;

varying vec4 color;
varying vec2 texcoord;

attribute vec3 POSITION;
attribute vec2 TEXCOORD;
attribute vec4 COLOR;

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
	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * vec4(POSITION, 1.0);

	texcoord = vec2(TEXTURE_MAT * vec4(TEXCOORD, 0, 1));

	// pass the color through as well
	color = COLOR * GLOBAL_COLOR;

	FogVertex();
}
