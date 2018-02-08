#ifndef QUETOO_FOG_GLSL
#define QUETOO_FOG_GLSL

#include "include/uniforms.glsl"
#include "include/noise3d.glsl"

struct FogParameters {
	float START;
	float END;
	vec3 COLOR;
	float DENSITY;
	float GAMMA_CORRECTION;
};

uniform FogParameters FOG;

#ifdef FRAGMENT_SHADER

/**
 * @brief Apply fog to the fragment if enabled.
 */
void FogFragment(in float len, inout vec4 fragColor) {
	fragColor.rgb = mix(fragColor.rgb, FOG.COLOR, clamp((len - FOG.START) / (FOG.END - FOG.START) * FOG.DENSITY, 0.0, 1.0));
	fragColor.rgb *= mix(vec3(1.0), fragColor.rgb, FOG.GAMMA_CORRECTION);
}

#endif

#endif //QUETOO_FOG_GLSL
