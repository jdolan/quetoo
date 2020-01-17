/**
 * @brief Particle vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "include/matrix.glsl"
#include "include/fog.glsl"

in vec3 POSITION;
in vec2 TEXCOORD0;
in vec2 TEXCOORD1;
in vec4 COLOR;
in float SCALE;
in float ROLL;
in vec3 END;
in int TYPE;

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

	texcoord0 = TEXCOORD0;
	texcoord1 = TEXCOORD1;
	color = COLOR;
	scale = SCALE;
	roll = ROLL;
	end = END;
	type = TYPE;
}
