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

#define FOG_VARIABLE float fog

#ifdef VERTEX_SHADER
/**
 * @brief Calculate the fog mix factor.
 */
float FogVertex(void) {
	return clamp((gl_Position.z - FOG.START) / (FOG.END - FOG.START) * FOG.DENSITY, 0.0, 1.0);
}
#endif
