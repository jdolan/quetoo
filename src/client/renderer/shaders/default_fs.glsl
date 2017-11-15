/**
 * @brief Default fragment shader.
 */

#version 330

precision lowp int;
precision lowp float;

#define FRAGMENT_SHADER

// Placeholders for ingame settings.
#define PARALLAX_OCCLUSION
#define PARALLAX_SHADOW_HARD
// #define PARALLAX_SHADOW_SOFT

#include "include/uniforms.glsl"
#include "include/fog.glsl"
#include "include/noise3d.glsl"
#include "include/tint.glsl"
#include "include/gamma.glsl"
#include "include/hdr.glsl"
#include "include/brdf.glsl"
#include "include/parallax.glsl"

#define MAX_LIGHTS $r_max_lights
#define LIGHT_SCALE $r_modulate

#if MAX_LIGHTS
struct LightParameters {
	vec3 ORIGIN[MAX_LIGHTS];
	vec3 COLOR[MAX_LIGHTS];
	float RADIUS[MAX_LIGHTS];
};

uniform LightParameters LIGHTS;
#endif

struct CausticParameters {
	bool ENABLE;
	vec3 COLOR;
};

uniform CausticParameters CAUSTIC;

uniform bool DIFFUSE;
uniform bool LIGHTMAP;
uniform bool DELUXEMAP;
uniform bool NORMALMAP;
uniform bool GLOSSMAP;
uniform bool STAINMAP;

uniform float BUMP;
uniform float PARALLAX;
uniform float HARDNESS;
uniform float SPECULAR;

uniform sampler2D SAMPLER0;
uniform sampler2D SAMPLER1;
uniform sampler2D SAMPLER2;
uniform sampler2D SAMPLER3;
uniform sampler2D SAMPLER4;
uniform sampler2D SAMPLER8;

#define tex_diffuse  SAMPLER0
#define tex_light    SAMPLER1
#define tex_normal   SAMPLER3
#define tex_deluxe   SAMPLER2
#define tex_specular SAMPLER4
#define tex_stain    SAMPLER8

uniform float ALPHA_THRESHOLD;

in VertexData {
	vec3 vtx_modelpoint;
	vec2 vtx_texcoords[2];
	vec4 vtx_color;
	vec3 vtx_point;
	vec3 vtx_normal;
	vec3 vtx_tangent;
	vec3 vtx_bitangent;
	vec3 vtx_eye;
};

#define uv_materials vtx_texcoords[0]
#define uv_lightmap  vtx_texcoords[1]

out vec4 fragColor;

vec3 eyeDir = normalize(vtx_eye);

/**
 * @brief Iterate the hardware light sources,
 * accumulating dynamic lighting for this fragment.
 */
void DynamicLighting(vec3 viewDir, vec3 normal,
	float specularIntensity, float specularPower,
	in out vec3 lightDiffuse, in out vec3 lightSpecular) {

	const float fillLight = 0.2;
	for (int i = 0; i < MAX_LIGHTS; i++) {

		// A light radius of 0.0 means break.
		if (LIGHTS.RADIUS[i] == 0.0) { break; }

		vec3  delta = LIGHTS.ORIGIN[i] - vtx_point;
		float distance = length(delta);

		// Don't shade points outside of the radius.
		if (distance > LIGHTS.RADIUS[i]) { continue; }

		// Get angle.
		vec3 lightDir = normalize(delta);

		// Make distance relative to radius.
		distance = 1.0 - distance / LIGHTS.RADIUS[i];

		if (LIGHTMAP) {
			lightDiffuse += Lambert(lightDir, normal, LIGHTS.COLOR[i])
				* distance * distance;
		} else {
			lightDiffuse += LambertFill(lightDir, normal, fillLight,
				LIGHTS.COLOR[i]) * distance * distance;
		}
		
		lightSpecular += Phong(viewDir, lightDir, normal, LIGHTS.COLOR[i],
			specularIntensity, specularPower) * distance;
	}
}

/**
 * @brief Shader entry point.
 */
void main(void) {

	// Get alpha and don't bother with fully transparent stuff.
	float alpha = vtx_color.a;
	if (DIFFUSE) {
		alpha *= texture(tex_diffuse, uv_textures).a;
		if (vtx_color.a * alpha < ALPHA_THRESHOLD) { discard; }
	}

	vec2 uv_materials = uv_textures;
	// Get distance from view.
	float viewDistance = length(vtx_point);

	// Lighting accumulators for diffuse and specular contributions.
	vec3 diffLight = vec3(0.0);
	vec3 specLight = vec3(0.0);

	// Get flat colors.
	vec4 diffAlbedo = vec4(1.0);
	vec3 specAlbedo = vec3(0.1);
	// Get view direction.
	vec3 eyeDir = normalize(vtx_eye);
	// Get vertex-normal.
	vec3 normal = normalize(vtx_normal);
	// Get constant shading.
	vec3 lightmap = vtx_color.rgb;

	vec3 lightDir = DELUXEMAP
		? texture(tex_deluxe, uv_lightmap).rgb
		: vec3(0.0, 0.0, 1.0);
	lightDir = normalize((lightDir - 0.5) * 2.0);

	float lightMask = 1.0;

 	if (DELUXEMAP && NORMALMAP && PARALLAX > 0) {
		#ifdef PARALLAX_OCCLUSION
		// Parallax mapping.
		uv_materials = POM(tex_normal, uv_materials, eyeDir,
			viewDistance, PARALLAX);
		#else
		uv_materials = BumpOffset(tex_normal, uv_materials, eyeDir,
			PARALLAX);
		#endif
		#ifdef PARALLAX_SHADOW_HARD
		// Self-shadowing parallax.
		lightMask = SelfShadowHard(tex_normal, uv_materials, lightDir,
			viewDistance, PARALLAX);
		#endif
		#ifdef PARALLAX_SHADOW_SOFT
		// Self-shadowing parallax.
		lightMask = SelfShadowSoft(tex_normal, uv_materials, lightDir,
			viewDistance, PARALLAX);
		#endif
	}

}
