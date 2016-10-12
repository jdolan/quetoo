/**
 * @brief Warp vertex shader.
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

varying float fog;

/**
 * @brief
 */
void FogVertex(void) {
	fog = (gl_Position.z - FOG.START) / (FOG.END - FOG.START);
	fog = clamp(fog, 0.0, 1.0) * FOG.DENSITY;
}

/**
 * @brief Program entry point.
 */
void main(void) {

	// mvp transform into clip space
	gl_Position = ftransform();

	// pass texcoords through
	gl_TexCoord[0] = gl_MultiTexCoord0;

	// and primary color
	gl_FrontColor = gl_Color;

	FogVertex();
}
