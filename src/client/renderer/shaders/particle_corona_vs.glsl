/**
 * @brief Corona vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "include/matrix.glsl"
#include "include/fog.glsl"

in vec3 POSITION;
in vec4 COLOR;
in float SCALE;

out VertexData {
	vec2 texcoord0;
	vec2 texcoord1;
	vec4 color;
	float scale;
	float roll;
	vec3 end;
	int type;
};

/**
 * @brief Shader entry point.
 */
void main(void) {

	// mvp transform into clip space
	gl_Position = vec4(POSITION, 1.0);

	texcoord0 = vec2(1.0, 1.0);
	texcoord1 = vec2(0.0, 0.0);
	scale = SCALE;
	roll = 0;
	end = vec3(0.0);
	type = 0;

	// pass the color through as well
	color = COLOR;
}
