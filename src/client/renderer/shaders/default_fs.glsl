/**
 * @brief Default fragment shader.
 */

#version 330

#define FRAGMENT_SHADER

#include "include/matrix.glsl"
#include "include/fog.glsl"
#include "include/noise3d.glsl"

#define MAX_LIGHTS $r_max_lights

#if MAX_LIGHTS
struct LightParameters
{
	vec3 ORIGIN[MAX_LIGHTS];
	vec3 COLOR[MAX_LIGHTS];
	float RADIUS[MAX_LIGHTS];
};

uniform LightParameters LIGHTS;

// linear + quadratic attenuation
#define LIGHT_CONSTANT 0.0
#define LIGHT_LINEAR 4.0
#define LIGHT_QUADRATIC 8.0
#define LIGHT_ATTENUATION (LIGHT_CONSTANT + (LIGHT_LINEAR * dist) + (LIGHT_QUADRATIC * dist * dist))

// light color clamping
#define LIGHT_CLAMP_MIN 0.0
#define LIGHT_CLAMP_MAX 4.0
#endif

// RGB layer tints
uniform vec4 TINTS[3];

struct CausticParameters
{
	bool ENABLE;
	vec3 COLOR;
};

uniform CausticParameters CAUSTIC;

uniform bool DIFFUSE;
uniform bool LIGHTMAP;
uniform bool DELUXEMAP;
uniform bool NORMALMAP;
uniform bool GLOSSMAP;
uniform bool TINTMAP;

uniform float BUMP;
uniform float PARALLAX;
uniform float HARDNESS;
uniform float SPECULAR;

uniform sampler2D SAMPLER0;
uniform sampler2D SAMPLER1;
uniform sampler2D SAMPLER2;
uniform sampler2D SAMPLER3;
uniform sampler2D SAMPLER4;
uniform sampler2D SAMPLER6;

uniform float ALPHA_THRESHOLD;

uniform float TIME_FRACTION;
uniform float TIME;

in vec3 modelpoint;
in vec4 color;
in vec2 texcoords[2];
in vec3 point;
in vec3 normal;
in vec3 tangent;
in vec3 bitangent;
in vec3 eye;

vec3 eyeDir;

const vec3 two = vec3(2.0);
const vec3 negHalf = vec3(-0.5);

out vec4 fragColor;

/**
 * @brief Yield the parallax offset for the texture coordinate.
 */
vec2 BumpTexcoord(in float height) {
	return vec2(height * 0.04 - 0.02) * PARALLAX * eyeDir.xy;
}

/**
 * @brief Yield the diffuse modulation from bump-mapping.
 */
vec3 BumpFragment(in vec3 deluxemap, in vec3 normalmap, in vec3 glossmap) {

	float diffuse = max(dot(deluxemap, normalmap), 1.0);

	float specular = HARDNESS * pow(max(-dot(eyeDir, reflect(deluxemap, normalmap)), 0.0), 8.0 * SPECULAR);

	return diffuse + specular * glossmap;
}

/**
 * @brief Yield the final sample color after factoring in dynamic light sources.
 */
void LightFragment(in vec4 diffuse, in vec3 lightmap, in vec3 normalmap) {

	vec3 light = vec3(0.0);

#if MAX_LIGHTS
	/*
	 * Iterate the hardware light sources, accumulating dynamic lighting for
	 * this fragment. A light radius of 0.0 means break.
	 */
	for (int i = 0; i < MAX_LIGHTS; i++) {

		if (LIGHTS.RADIUS[i] == 0.0)
			break;

		vec3 delta = LIGHTS.ORIGIN[i] - point;
		float dist = length(delta);

		if (dist < LIGHTS.RADIUS[i]) {

			float d = dot(normalmap, normalize(delta));
			if (d > 0.0) {

				dist = 1.0 - dist / LIGHTS.RADIUS[i];
				light += LIGHTS.COLOR[i] * d * LIGHT_ATTENUATION;
			}
		}
	}

	light = clamp(light, LIGHT_CLAMP_MIN, LIGHT_CLAMP_MAX);
#endif

	// now modulate the diffuse sample with the modified lightmap
	fragColor.rgb = diffuse.rgb * (lightmap + light);

	// lastly modulate the alpha channel by the color
	fragColor.a = diffuse.a * color.a;
}

/**
 * @brief Render caustics
 */
void CausticFragment(in vec3 lightmap) {
	if (CAUSTIC.ENABLE) {
		float factor = noise3d((modelpoint * vec3(0.024, 0.024, 0.016)) + (TIME * 0.4));
		factor = pow((1 - abs(factor)) + 0.03, 6);

		fragColor.rgb += clamp(CAUSTIC.COLOR * factor * clamp((lightmap * 1.6) - 0.5, 0.1, 1.0) * 0.17, 0.0, 1.0);
	}
}

/**
 * @brief Shader entry point.
 */
void main(void) {

	// first resolve the flat shading
	vec3 lightmap = color.rgb;
	vec3 deluxemap = vec3(0.0, 0.0, 1.0);

	if (LIGHTMAP) {
		lightmap = texture(SAMPLER1, texcoords[1]).rgb;
	}

	// then resolve any bump mapping
	vec4 normalmap = vec4(normal, 1.0);
	vec2 parallax = vec2(0.0);
	vec3 bump = vec3(1.0);

	if (NORMALMAP) {

		eyeDir = normalize(eye);

		if (DELUXEMAP) {
			deluxemap = texture(SAMPLER2, texcoords[1]).rgb;
			deluxemap = normalize(two * (deluxemap + negHalf));
		}

		normalmap = texture(SAMPLER3, texcoords[0]);

		parallax = BumpTexcoord(normalmap.w);

		normalmap.xyz = normalize(two * (normalmap.xyz + negHalf));
		normalmap.xyz = normalize(vec3(normalmap.x * BUMP, normalmap.y * BUMP, normalmap.z));

		vec3 glossmap = vec3(1.0);

		if (GLOSSMAP) {
			glossmap = texture(SAMPLER4, texcoords[0]).rgb;
		}

		// resolve the bumpmap modulation
		bump = BumpFragment(deluxemap, normalmap.xyz, glossmap);

		// and then transform the normalmap to model space for lighting
		normalmap.xyz = normalize(
			normalmap.x * normalize(tangent) +
			normalmap.y * normalize(bitangent) +
			normalmap.z * normalize(normal)
		);
	}

	vec4 diffuse = vec4(1.0);

	if (DIFFUSE) { // sample the diffuse texture, honoring the parallax offset
		diffuse = texture(SAMPLER0, texcoords[0] + parallax);

		// see if diffuse can be discarded because of alpha test
		if (diffuse.a < ALPHA_THRESHOLD)
			discard;

		if (TINTMAP) {
			vec4 tint = texture(SAMPLER6, texcoords[0] + parallax);

			if (tint.a > 0) {
				for (int i = 0; i < 3; i++) {
					if (TINTS[i].a > 0 && tint[i] > 0) {
						diffuse.rgb = mix(diffuse.rgb, TINTS[i].rgb * tint[i], tint.a);
					}
				}
			}
		}

		// factor in bump mapping
		diffuse.rgb *= bump;
	}

	// add any dynamic lighting to yield the final fragment color
	LightFragment(diffuse, lightmap, normalmap.xyz);

    // underliquid caustics
	CausticFragment(lightmap);

	// and fog
	FogFragment(fragColor);
}
