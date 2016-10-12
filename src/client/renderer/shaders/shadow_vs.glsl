/**
 * @brief Planar shadows vertex shader.
 */

#version 120

#include "fog_inc.glsl"

uniform mat4 MATRIX;
uniform vec4 LIGHT;

varying vec4 point;

/**
 * @brief
 */
void ShadowVertex() {
	point = gl_ModelViewMatrix * MATRIX * gl_Vertex;	
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
	gl_Position = gl_ModelViewProjectionMatrix * MATRIX * gl_Vertex;
	
	// and primary color
	gl_FrontColor = gl_Color;
	
	ShadowVertex();
    
    FogVertex();
}
