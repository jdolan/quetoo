struct FogParameters {
	float START;
	float END;
	vec3 COLOR;
	float DENSITY;
};

uniform FogParameters FOG;

#ifdef VERTEX_SHADER

/**
 * @brief Calculate the fog mix factor.
 */
float FogVertex(void) {
	return clamp((gl_Position.z - FOG.START) / (FOG.END - FOG.START) * FOG.DENSITY, 0.0, 1.0);
}

#else

/**
 * @brief Apply fog to the fragment if enabled.
 */
void FogFragment(inout vec4 fragColor, in float fog) {
	fragColor.rgb = mix(fragColor.rgb, FOG.COLOR, fog);
}

#endif
