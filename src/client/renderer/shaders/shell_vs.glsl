/*
 * @brief Shell vertex shader.
 */

#version 120

uniform float OFFSET;

/*
 * @brief Shader entry point.
 */
void main(void) {

	// mvp transform into clip space
	gl_Position = ftransform();

	// pass texcoords through
	gl_TexCoord[0] = gl_MultiTexCoord0 + OFFSET;
	
	// pass the color through as well
	gl_FrontColor = gl_Color;
}
