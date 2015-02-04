/*
 * @brief Planar shadows vertex shader.
 */

#version 120

uniform mat4 MATRIX;
uniform vec4 LIGHT;

varying vec4 point;

/*
 * @brief
 */
void ShadowVertex() {
	
	point = gl_ModelViewMatrix * MATRIX * gl_Vertex;	
}

/*
 * @brief Program entry point.
 */
void main(void) {
	
	// mvp transform into clip space
	gl_Position = gl_ModelViewProjectionMatrix * MATRIX * gl_Vertex;
	
	// and primary color
	gl_FrontColor = gl_Color;
	
	ShadowVertex();
}
