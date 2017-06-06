/**
 * @brief Shell fragment shader.
 */

#version 330

#define FRAGMENT_SHADER

#include "include/uniforms.glsl"

uniform sampler2D SAMPLER0;

in VertexData {
	vec4 color;
	vec2 texcoord;
};

out vec4 fragColor;

/**
 * @brief Shader entry point.
 */
void main(void) {

	fragColor = GLOBAL_COLOR * texture(SAMPLER0, texcoord);	
}
