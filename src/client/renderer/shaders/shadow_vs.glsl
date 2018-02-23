/**
 * @brief Planar shadows vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "include/uniforms.glsl"

uniform vec4 LIGHT;

in vec3 POSITION;
in vec3 NEXT_POSITION;

out VertexData {
	vec4 point;
};

/**
 * @brief
 */
void ShadowVertex() {
	point = MODELVIEW_MAT * SHADOW_MAT * vec4(mix(POSITION, NEXT_POSITION, TIME_FRACTION), 1.0);	
}

/**
 * @brief Program entry point.
 */
void main(void) {

	ShadowVertex();
	
	gl_Position = PROJECTION_MAT * point;
}
