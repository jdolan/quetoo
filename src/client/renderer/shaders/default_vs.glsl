/**
 * @brief Default vertex shader.
 */

#version 120

#include "matrix_inc.glsl"
#include "fog_inc.glsl"

uniform bool DIFFUSE;
uniform bool NORMALMAP;

varying vec4 color;
varying vec2 texcoords[2];
varying vec3 point;
varying vec3 normal;
varying vec3 tangent;
varying vec3 bitangent;

attribute vec3 POSITION;
attribute vec4 COLOR;
attribute vec2 TEXCOORD0;
attribute vec2 TEXCOORD1;
attribute vec3 NORMAL;
attribute vec4 TANGENT;

/**
 * @brief Transform the point, normal and tangent vectors, passing them through
 * to the fragment shader for per-pixel lighting.
 */
void LightVertex(void) {

	point = vec3(MODELVIEW_MAT * vec4(POSITION, 1.0));
	normal = normalize(vec3(NORMAL_MAT * vec4(NORMAL, 1.0)));

	if (NORMALMAP) {
		tangent = normalize(vec3(NORMAL_MAT * TANGENT));
		bitangent = cross(normal, tangent) * TANGENT.w;
	}
}

/**
 * @brief Calculate the interpolated fog value for the vertex.
 */
void FogVertex(void) {

	fog = (gl_Position.z - FOG.START) / (FOG.END - FOG.START);
	fog = clamp(fog, 0.0, 1.0) * FOG.DENSITY;
}

/**
 * @brief Shader entry point.
 */
void main(void) {

	LightVertex();

	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * vec4(point, 1.0);

	if (DIFFUSE) { // pass texcoords through
		texcoords[0] = vec2(TEXTURE_MAT * vec4(TEXCOORD0, 0, 1));
		texcoords[1] = vec2(TEXTURE_MAT * vec4(TEXCOORD1, 0, 1));
	}

	// pass the color through as well
	color = COLOR;

	FogVertex();
}
