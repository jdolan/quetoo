/**
 * @brief Warp fragment shader.
 */

#version 330

#define FRAGMENT_SHADER

#include "include/fog.glsl"

uniform float OFFSET;
uniform vec4 GLOBAL_COLOR;

uniform sampler2D SAMPLER0;
uniform sampler2D SAMPLER5;

in VertexData {
	vec2 texcoord;
	vec3 point;
};

out vec4 fragColor;

/**
 * @brief Program entry point.
 */
void main(void) {

	// sample the warp texture at a time-varied offset
	vec4 warp = texture(SAMPLER5, texcoord + vec2(OFFSET));

	// and derive a diffuse texcoord based on the warp data
	vec2 coord = vec2(texcoord.x + warp.z, texcoord.y + warp.w);

	// sample the diffuse texture, factoring in primary color as well
	fragColor = GLOBAL_COLOR * texture(SAMPLER0, coord);

	// and add fog
	FogFragment(length(point), fragColor);
}
