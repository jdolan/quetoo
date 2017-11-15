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
	float viewDistance = length(vtx_point);

	vec4 diffuse = vec4(1.0);
	fragColor.a = vtx_color.a;

	if (DIFFUSE) {
		diffuse = texture(tex_diffuse, uv_materials);
		fragColor.a *= diffuse.a;

		// see if diffuse can be discarded because of alpha test
		if (fragColor.a < ALPHA_THRESHOLD) { discard; }

		// change the color of some things
		TintFragment(diffuse, uv_materials);
	}

	// first resolve the flat shading
	vec3 lightmap = vtx_color.rgb;

	if (LIGHTMAP) {
		lightmap = texture(tex_light, uv_lightmap).rgb;

		if (STAINMAP) {
			vec4 stain = texture(tex_stain, uv_lightmap);
			lightmap = mix(lightmap.rgb, stain.rgb, stain.a).rgb;
		}
	}

	vec3 normal = vtx_normal;
	
	float lightmapBumpScale = 1.0;
	float lightmapSpecularScale = 0.0;

	if (NORMALMAP) {

		vec3 deluxemap = DELUXEMAP ? texture(tex_deluxe, uv_lightmap).rgb : vec3(0.0, 0.0, 1.0);
		deluxemap = normalize((deluxemap - 0.5) * 2.0);

		// resolve the initial normalmap sample
		normal = texture(tex_normal, uv_materials).xyz;
		normal = (normal - 0.5) * 2.0;
		normal.xy *= BUMP;
		normal = normalize(normal);

		vec3 glossmap = GLOSSMAP ? texture(tex_specular, uv_materials).rgb : vec3(0.5);

		// resolve the bumpmap modulation
		BumpFragment(deluxemap, normal, glossmap, lightmapBumpScale, lightmapSpecularScale);

		// and then transform the normalmap to model space for lighting
		normal = normalize(
			normal.x * normalize(vtx_tangent) +
			normal.y * normalize(vtx_bitangent) +
			normal.z * normalize(vtx_normal)
		);
	}

	// add any dynamic lighting to yield the final fragment color
	lightmap.rgb = NormalizeLightmap(lightmap.rgb, lightmapBumpScale, lightmapSpecularScale);

	#if MAX_LIGHTS
	fragColor.rgb = diffuse.rgb * ((lightmap.rgb + DynamicLighting(normal)) * LIGHT_SCALE);
	#else
	fragColor.rgb = diffuse.rgb * (lightmap.rgb * LIGHT_SCALE);
	#endif

	//fragColor.rgb = tonemap_ldr(fragColor.rgb);

	fragColor.rgb = lightmap.rgb;
	// and fog
	FogFragment(length(vtx_point), fragColor);
}
