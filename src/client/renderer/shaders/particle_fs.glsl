/**
 * @brief Particle fragment shader.
 */

#version 330

#define FRAGMENT_SHADER

#define PARTICLE_DEFAULT 0
#define PARTICLE_SPARK 1
#define PARTICLE_ROLL 2
#define PARTICLE_EXPLOSION 3
#define PARTICLE_BUBBLE 4
#define PARTICLE_BEAM 5
#define PARTICLE_WEATHER 6
#define PARTICLE_SPLASH 7
#define PARTICLE_CORONA 8
#define PARTICLE_FLARE 9
#define PARTICLE_WIRE 10

#include "include/fog.glsl"
#include "include/gamma.glsl"
#include "include/matrix.glsl"

uniform sampler2D SAMPLER0;

in VertexData {
	vec4 color;
	vec2 texcoord;
	vec3 point;
	float type;
};

out vec4 fragColor;

/**
 * @brief Shader entry point.
 */
void main(void) {

	if (type == PARTICLE_CORONA) {
		float alpha = clamp(1.0 - length(texcoord - vec2(0.5, 0.5)) / 0.5, 0.0, 1.0);
		fragColor = color * color.a * alpha;
	} else {
		fragColor = color * texture(SAMPLER0, texcoord);
	}

	FogFragment(length(point), fragColor);

	GammaFragment(fragColor);
}
