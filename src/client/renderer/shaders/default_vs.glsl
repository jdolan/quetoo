/**
 * @brief Default vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "include/uniforms.glsl"
#include "include/fog.glsl"

uniform bool DIFFUSE;
uniform bool LIGHTMAP;
uniform bool NORMALMAP;
uniform mat4 NORMAL_MAT;

in vec3 POSITION;
in vec4 COLOR;
in vec2 TEXCOORD0;
in vec2 TEXCOORD1;
in vec3 NORMAL;
in vec4 TANGENT;
in vec3 NEXT_POSITION;
in vec3 NEXT_NORMAL;
in vec4 NEXT_TANGENT;

out VertexData {
	vec3 modelpoint;
	vec2 texcoords[2];
	vec4 color;
	vec3 point;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec3 eye;
};

/**
 * @brief Transform the point, normal and tangent vectors, passing them through
 * to the fragment shader for per-pixel lighting.
 */
void LightVertex(void) {

	point = vec3(MODELVIEW_MAT * vec4(mix(POSITION, NEXT_POSITION, TIME_FRACTION), 1.0));
	normal = normalize(vec3(NORMAL_MAT * vec4(mix(NORMAL, NEXT_NORMAL, TIME_FRACTION), 1.0)));

	if (NORMALMAP) {
		vec4 tang = mix(TANGENT, NEXT_TANGENT, TIME_FRACTION);
		tangent = normalize(vec3(NORMAL_MAT * tang));
		bitangent = cross(normal, tangent) * tang.w;

		eye.x = -dot(point, tangent);
		eye.y = -dot(point, bitangent);
		eye.z = -dot(point, normal);
	}
}

/**
 * @brief Shader entry point.
 */
void main(void) {
	// get model coordinate
	modelpoint = vec3(mix(POSITION, NEXT_POSITION, TIME_FRACTION));

	LightVertex();

	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * vec4(point, 1.0);

	if (DIFFUSE) { // pass texcoords through
		texcoords[0] = TEXCOORD0;
	}

	if (LIGHTMAP) {
		texcoords[1] = TEXCOORD1;
	}

	// pass the color through as well
	color = COLOR * GLOBAL_COLOR;
}
