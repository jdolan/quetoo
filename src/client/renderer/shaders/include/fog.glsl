#ifndef QUETOO_FOG_GLSL
#define QUETOO_FOG_GLSL

#include "include/uniforms.glsl"
#include "include/noise3d.glsl"

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

	// grab raw 3d noise
	/*float factor = noise3d(vec3(gl_FragCoord.xy / 256.0, TIME / 5000.0));

	// scale to make very close to -1.0 to 1.0 based on observational data
	factor = factor * (0.3515 * 2.0);

	// reduce scale so we're not too noisy
	factor = (1.9 + factor) / 2.9;

	fog *= factor;*/

	fragColor.rgb = mix(fragColor.rgb, FOG.COLOR, fog);
}

#endif

#endif //QUETOO_FOG_GLSL
