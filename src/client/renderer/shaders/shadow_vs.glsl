/**
 * @brief Planar shadows vertex shader.
 */

#version 120

#include "fog_inc.glsl"
#include "matrix_inc.glsl"

uniform mat4 SHADOW_MAT;
uniform vec4 LIGHT;

varying vec4 point;

attribute vec3 POSITION;

uniform float TIME_FRACTION;

attribute vec3 NEXT_POSITION;

/**
 * @brief
 */
void ShadowVertex() {
	point = MODELVIEW_MAT * SHADOW_MAT * vec4(mix(POSITION, NEXT_POSITION, TIME_FRACTION), 1.0);	
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

	ShadowVertex();
	
	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * point;
	    
    FogVertex();
}
