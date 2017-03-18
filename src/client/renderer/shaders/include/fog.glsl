struct FogParameters
{
	float START;
	float END;
	vec3 COLOR;
	float DENSITY;
};

uniform FogParameters FOG;

#ifdef VERTEX_SHADER

out float fog;

/**
 * @brief Calculate the fog mix factor.
 */
void FogVertex(void) {
	fog = (gl_Position.z - FOG.START) / (FOG.END - FOG.START);
	fog = clamp(fog * FOG.DENSITY, 0.0, 1.0);
}

#else

in float fog;

/**
 * @brief Apply fog to the fragment if enabled.
 */
void FogFragment(inout vec4 fragColor) {
	fragColor.rgb = mix(fragColor.rgb, FOG.COLOR, fog);
}

#endif
