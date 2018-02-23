struct FogParameters {
	float START;
	float END;
	vec3 COLOR;
	float DENSITY;
};

uniform FogParameters FOG;

#ifdef FRAGMENT_SHADER

/**
 * @brief Apply fog to the fragment if enabled.
 */
void FogFragment(in float len, inout vec4 fragColor) {
	fragColor.rgb = mix(fragColor.rgb, FOG.COLOR, clamp((len - FOG.START) / (FOG.END - FOG.START) * FOG.DENSITY, 0.0, 1.0));
}

#endif
