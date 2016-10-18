/**
 * @brief Warp fragment shader.
 */

#version 120

#include "fog_inc.glsl"

uniform float OFFSET;

uniform sampler2D SAMPLER0;
uniform sampler2D SAMPLER1;

varying vec4 color;
varying vec2 texcoord;

/**
 * @brief
 */
void FogFragment(void) {
	gl_FragColor.rgb = mix(gl_FragColor.rgb, FOG.COLOR, fog);
}

/**
 * @brief Program entry point.
 */
void main(void) {

	// sample the warp texture at a time-varied offset
	vec4 warp = texture2D(SAMPLER1, texcoord + vec2(OFFSET));

	// and derive a diffuse texcoord based on the warp data
	vec2 coord = vec2(texcoord.x + warp.z, texcoord.y + warp.w);

	// sample the diffuse texture, factoring in primary color as well
	gl_FragColor = texture2D(SAMPLER0, coord);

	FogFragment();  // add fog
}
