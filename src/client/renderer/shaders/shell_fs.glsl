/**
 * @brief Shell fragment shader.
 */

#version 120

uniform sampler2D SAMPLER0;

varying vec2 texcoord;

/**
 * @brief Shader entry point.
 */
void main(void) {

	gl_FragColor = texture2D(SAMPLER0, texcoord);	
}
