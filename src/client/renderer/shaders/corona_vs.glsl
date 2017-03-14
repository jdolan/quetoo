/**
 * @brief Corona vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "include/matrix.glsl"
#include "include/fog.glsl"

out vec4 color;
out vec2 texcoord;

in vec3 POSITION;
in vec2 TEXCOORD;
in vec4 COLOR;

/**
 * @brief Shader entry point.
 */
void main(void) {

	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * vec4(POSITION, 1.0);

	texcoord = TEXCOORD;

	color = COLOR;

	FogVertex();
}
