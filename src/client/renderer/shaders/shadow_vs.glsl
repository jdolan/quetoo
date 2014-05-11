/*
 * @brief Planar shadows vertex shader.
 */

#version 120

uniform mat4 MODEL_MATRIX;

varying vec3 point;

/*
 * ShadowVertex
 */
void ShadowVertex(void) {
	point = vec3(MODEL_MATRIX * gl_Vertex);
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
