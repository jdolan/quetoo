/**
 * @brief Planar shadows vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "include/matrix.glsl"

uniform mat4 SHADOW_MAT;
uniform vec4 LIGHT;
uniform float TIME_FRACTION;

in vec3 POSITION;
in vec3 NEXT_POSITION;

out VertexData {
	vec4 point;
};

/**
 * @brief
 */
void ShadowVertex() {
	point = MODEL_VIEW_MAT * SHADOW_MAT * vec4(mix(POSITION, NEXT_POSITION, TIME_FRACTION), 1.0);	
}

/**
 * @brief Program entry point.
 */
void main(void) {

	ShadowVertex();
	
	gl_Position = PROJECTION_MAT * point;
}
