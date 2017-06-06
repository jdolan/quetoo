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
void FogFragment(inout vec4 fragColor) {
	float fog = clamp(((gl_FragCoord.z / gl_FragCoord.w) - FOG.START) / (FOG.END - FOG.START) * FOG.DENSITY, 0.0, 1.0);
	fragColor.rgb = mix(fragColor.rgb, FOG.COLOR, fog);
}

#endif
