/**
 * @brief Default vertex shader.
 */

#version 120

#include "matrix_inc.glsl"
#include "fog_inc.glsl"

uniform bool DIFFUSE;
uniform bool NORMALMAP;

varying vec4 color;
varying vec3 point;
varying vec3 normal;
varying vec3 tangent;
varying vec3 bitangent;

varying mat4 modelView;

attribute vec4 TANGENT;

/**
 * @brief Transform the point, normal and tangent vectors, passing them through
 * to the fragment shader for per-pixel lighting.
 */
void LightVertex(void) {

	point = vec3(MODELVIEW_MAT * gl_Vertex);
	normal = normalize(vec3(NORMAL_MAT * vec4(gl_Normal, 1.0)));

	modelView = MODELVIEW_MAT;

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

	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * gl_Vertex;

	if (DIFFUSE) { // pass texcoords through
		gl_TexCoord[0] = TEXTURE_MAT * gl_MultiTexCoord0;
		gl_TexCoord[1] = TEXTURE_MAT * gl_MultiTexCoord1;
	}

	// pass the color through as well
	color = gl_Color;

	LightVertex();

	FogVertex();
}
