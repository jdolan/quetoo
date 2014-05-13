/*
 * @brief Planar shadows vertex shader.
 */

#version 120

uniform mat4 MODEL_MATRIX;
uniform mat4 SHADOW_MATRIX;

varying vec3 points[2];

/*
 * ShadowVertex
 */
void ShadowVertex(void) {
	points[0] = vec3(MODEL_MATRIX * gl_Vertex);
	points[1] = vec3(SHADOW_MATRIX * gl_Vertex);
}

/*
 * main
 */
void main(void) {

	// mvp transform into clip space
	gl_Position = ftransform();

	// and primary color
	gl_FrontColor = gl_Color;
	
	ShadowVertex();
}
