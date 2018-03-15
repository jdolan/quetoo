
#ifndef QUETOO_GAMMA_GLSL
#define QUETOO_GAMMA_GLSL

#define GAMMA $r_gamma

#ifdef FRAGMENT_SHADER

/**
 * @brief Applies gamma correction to the input color.
 */
void GammaFragment(inout vec4 fragColor) {
	fragColor.rgb *= mix(vec3(1.0), fragColor.rgb, GAMMA);
}

#endif

#endif
