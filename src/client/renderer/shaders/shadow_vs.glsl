/**
 * @brief Planar shadows vertex shader.
 */

#version 120

#include "fog_inc.glsl"
#include "matrix_inc.glsl"

uniform mat4 MATRIX;
uniform vec4 LIGHT;

varying vec4 point;

attribute vec3 POSITION;

/**
 * @brief
 */
void ShadowVertex() {
	point = MODELVIEW_MAT * MATRIX * vec4(POSITION, 1.0);	
}

/**
 * @brief
 */
void FogVertex(void) {
    fog = (gl_Position.z - FOG.START) / (FOG.END - FOG.START) / point.w;
    fog = clamp(fog, 0.0, 1.0) * FOG.DENSITY;
}

/**
 * @brief Program entry point.
 */
void main(void) {
	
	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * MATRIX * vec4(POSITION, 1.0);
	
	ShadowVertex();
	    
    FogVertex();
}
