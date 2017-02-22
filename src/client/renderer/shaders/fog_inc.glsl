#ifndef FOG_NO_UNIFORM
struct FogParameters
{
	float START;
	float END;
	vec3 COLOR;
	float DENSITY;
};

uniform FogParameters FOG;
#endif

#ifdef VERTEX_SHADER
out float fog;
#else
in float fog;
#endif

#ifdef VERTEX_SHADER
/**
 * @brief Calculate the fog mix factor.
 */
void FogVertex(void) {
	fog = (gl_Position.z - FOG.START) / (FOG.END - FOG.START);
	fog = clamp(fog * FOG.DENSITY, 0.0, 1.0);
}
#endif
