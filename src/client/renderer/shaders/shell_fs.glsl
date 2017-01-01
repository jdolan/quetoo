/**
 * @brief Shell fragment shader.
 */

#version 120

#define FRAGMENT_SHADER

uniform sampler2D SAMPLER0;
uniform vec4 GLOBAL_COLOR;

varying vec4 color;
varying vec2 texcoord;

/**
 * @brief Shader entry point.
 */
void main(void) {

	gl_FragColor = GLOBAL_COLOR * texture2D(SAMPLER0, texcoord);	
}
