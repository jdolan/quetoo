/**
 * @brief Default vertex shader.
 */

#version 120

struct FogParameters
{
	float START;
	float END;
	vec3 COLOR;
	float DENSITY;
};

uniform FogParameters FOG;

uniform bool DIFFUSE;
uniform bool NORMALMAP;

varying vec3 point;
varying vec3 normal;
varying vec3 tangent;
varying vec3 bitangent;

varying float fog;

attribute vec4 TANGENT;

/**
 * @brief Transform the point, normal and tangent vectors, passing them through
 * to the fragment shader for per-pixel lighting.
 */
void LightVertex(void) {

	point = vec3(gl_ModelViewMatrix * gl_Vertex);
	normal = normalize(gl_NormalMatrix * gl_Normal);

	if (NORMALMAP) {
		tangent = normalize(gl_NormalMatrix * TANGENT.xyz);
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
	gl_Position = ftransform();

	if (DIFFUSE) { // pass texcoords through
		gl_TexCoord[0] = gl_MultiTexCoord0;
		gl_TexCoord[1] = gl_MultiTexCoord1;
	}

	// pass the color through as well
	gl_FrontColor = gl_Color;

	LightVertex();

	FogVertex();
}
