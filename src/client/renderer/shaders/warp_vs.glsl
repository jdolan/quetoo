/*
 * @brief Warp vertex shader.
 */

#version 120

varying float fog;

/*
 * @brief
 */
void FogVertex(void) {
	fog = (gl_Position.z - gl_Fog.start) / (gl_Fog.end - gl_Fog.start);
	fog = clamp(fog, 0.0, 1.0) * gl_Fog.density;
}

/*
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
