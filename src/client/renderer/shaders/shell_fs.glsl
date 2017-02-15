/**
 * @brief Shell fragment shader.
 */

#version 130

#define FRAGMENT_SHADER

uniform sampler2D SAMPLER0;
uniform vec4 GLOBAL_COLOR;

in vec4 color;
in vec2 texcoord;

/**
 * @brief Shader entry point.
 */
void main(void) {

	gl_FragColor = GLOBAL_COLOR * texture(SAMPLER0, texcoord);	
}
