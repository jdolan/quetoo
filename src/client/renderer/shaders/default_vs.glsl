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
in vec3 TANGENT;
in vec3 BITANGENT;

in vec3 NEXT_POSITION;
in vec3 NEXT_NORMAL;
in vec3 NEXT_TANGENT;
in vec3 NEXT_BITANGENT;

out VertexData {
	vec3 vtx_modelpoint;
	vec2 vtx_texcoords[2];
	vec4 vtx_color;
	vec3 vtx_point;
	vec3 vtx_normal;
	vec3 vtx_tangent;
	vec3 vtx_bitangent;
	vec3 vtx_eye;
};

/**
 * @brief Transform the vtx_point, vtx_normal and vtx_tangent vectors, passing them through
 * to the fragment shader for per-pixel lighting.
 */
void LightVertex(void) {

	vtx_point = (MODELVIEW_MAT * vec4(mix(POSITION, NEXT_POSITION, TIME_FRACTION), 1.0)).xyz;
	vtx_normal = normalize(vec3(NORMAL_MAT * vec4(mix(NORMAL, NEXT_NORMAL, TIME_FRACTION), 1.0)));

	if (NORMALMAP) {
		vec3 tang = mix(TANGENT, NEXT_TANGENT, TIME_FRACTION).xyz;
		vtx_tangent = normalize(vec3(NORMAL_MAT * vec4(tang, 1.0)));

		vec3 bitang = mix(BITANGENT, NEXT_BITANGENT, TIME_FRACTION).xyz;
		vtx_bitangent = normalize(vec3(NORMAL_MAT * vec4(bitang, 1.0)));

		vtx_eye.x = -dot(vtx_point, vtx_tangent);
		vtx_eye.y = -dot(vtx_point, vtx_bitangent);
		vtx_eye.z = -dot(vtx_point, vtx_normal);
	}
}

/**
 * @brief Shader entry vtx_point.
 */
void main(void) {
	// get model coordinate
	vtx_modelpoint = vec3(mix(POSITION, NEXT_POSITION, TIME_FRACTION));

	LightVertex();

	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * vec4(vtx_point, 1.0);

	if (DIFFUSE) { // pass texcoords through
		vtx_texcoords[0] = TEXCOORD0;
	}

	if (LIGHTMAP) {
		vtx_texcoords[1] = TEXCOORD1;
	}

	// pass the vtx_color through as well
	vtx_color = COLOR * GLOBAL_COLOR;
}
