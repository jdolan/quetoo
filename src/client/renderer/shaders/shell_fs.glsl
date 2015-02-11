/*
 * @brief Shell fragment shader.
 */

#version 120

uniform sampler2D SAMPLER0;

/*
 * @brief Shader entry point.
 */
void main(void) {

	gl_FragColor = texture2D(SAMPLER0, gl_TexCoord[0].st) * gl_Color;	
}
