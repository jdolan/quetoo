/**
 * @brief Default fragment shader.
 */

#version 330

#extension GL_ARB_texture_query_lod: enable

#define FRAGMENT_SHADER

#include "include/color.glsl"
#include "include/fog.glsl"
#include "include/gamma.glsl"
#include "include/matrix.glsl"
#include "include/noise3d.glsl"
#include "include/tint.glsl"

#define MAX_LIGHTS $r_max_lights
#define MODULATE $r_modulate
#define DRAW_BSP_LIGHTMAPS $r_draw_bsp_lightmaps

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
uniform sampler2DArray SAMPLER1;
uniform sampler2D SAMPLER3;
uniform sampler2D SAMPLER4;
uniform sampler2D SAMPLER8;

uniform float ALPHA_THRESHOLD;

uniform float TIME_FRACTION;
uniform float TIME;

in VertexData {
	vec3 modelpoint;
	vec2 texcoords[2];
	vec4 color;
	vec3 point;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec3 eye;
};

vec3 eyeDir;

out vec4 fragColor;

/**
 * @brief Clamp value t to range [a,b] and map [a,b] to [0,1].
 */
float linearstep(float a, float b, float t) {
	return clamp((t - a) / (b - a), 0.0, 1.0);
}

/**
 * @brief Used to dither the image before quantizing, combats banding artifacts.
 */
void DitherFragment(inout vec3 color) {

	// source: Alex Vlachos, Valve Software. Advanced VR Rendering, GDC2015.

	vec3 pattern = vec3(dot(vec2(171.0, 231.0), gl_FragCoord.xy));
	pattern.rgb = fract(pattern.rgb / vec3(103.0, 71.0, 97.0));
	color = clamp(color + (pattern.rgb / 255.0), 0.0, 1.0);
}

/**
 * @brief Yield the parallax offset for the texture coordinate.
 */
vec2 BumpTexcoord() {

	// Negative PARRALAX is used to skip the expensive SelfShadowHeightmap() routine.
	float SCALE = abs(PARALLAX);

	float texLod = 0;

	#if GL_ARB_texture_query_lod
	{
		// 2^7 = 128*128 largest texture size.
		const float maxMipmaps = 7;

		vec2 texSize = textureSize(SAMPLER3, 0).xy;
		float numMipmaps = log2(max(texSize.x, texSize.y));

		float minMip = numMipmaps - maxMipmaps;
		float mipLevel = textureQueryLOD(SAMPLER3, texcoords[0]).y;

		texLod = max(mipLevel, minMip);
	}
	#endif

	float numSamples = 32;
	float scale = SCALE * 0.04;

	#if 1 // Optimizations
	{
		const float minSamples = 8;

		#if 1 // Fade out at a distance.
		const float near = 200.0;
		const float far = 500.0;
		float dist = 1.0 - linearstep(near, far, length(point));
		numSamples = mix(minSamples, numSamples, dist);
		scale *= dist;
		#endif

		#if 1 // Do less work on surfaces orthogonal to the view. Might give subtle peeling artifacts.
		float orthogonality = clamp(dot(eyeDir, vec3(0.0, 0.0, 1.0)), 0.0, 1.0);
		numSamples = mix(minSamples, numSamples, 1.0 - orthogonality*orthogonality);
		#endif
	}
	#endif

	if (scale == 0.0) {
		return texcoords[0];
	}

	// Record keeping.
	vec2  uvPrev;
	vec2  uvCurr = texcoords[0] - (eyeDir.xy * scale * 0.5); // Middle-out parallaxing.
	float surfaceHeightPrev;
	float surfaceHeightCurr = textureLod(SAMPLER3, texcoords[0], texLod).a;
	float rayHeightPrev;
	float rayHeightCurr = 0.0;

	if (SCALE < 0.5) {
		return uvCurr + (eyeDir.xy * surfaceHeightCurr * scale);
	}

	// Height offset per step.
	float stepHeight = 1.0 / numSamples;
	// UV offset per step.
	vec2 uvDelta = eyeDir.xy * scale / numSamples;

	while (rayHeightCurr < surfaceHeightCurr) {

		uvPrev = uvCurr;
		uvCurr += uvDelta;
		surfaceHeightPrev = surfaceHeightCurr;
		surfaceHeightCurr = textureLod(SAMPLER3, uvCurr, texLod).a;
		rayHeightPrev = rayHeightCurr;
		rayHeightCurr += stepHeight;
	}

	// Take last two heights and interpolate between them.
	float a = surfaceHeightCurr - rayHeightCurr;
	float b = surfaceHeightPrev - rayHeightCurr + stepHeight;
	float blend = a / (a - b);

	return mix(uvCurr, uvPrev, blend);
}

/**
 * @brief Highpasses the heightmap to approximate ambient occlusion.
 */
float AmbientOcclusionHeightmap() {
	float heightA = texture(SAMPLER3, texcoords[0]).a;
	float heightB = texture(SAMPLER3, texcoords[0], 4).a;
	return (heightA - heightB) * 0.5 + 0.5;
}

/**
 * @brief Traces a ray from a point on the heightmap
 * towards the lightsource and checks for intersections.
 */
float SelfShadowHeightmap(vec3 lightDir, sampler2D tex, vec2 uv) {

	// Only work on grazing angles.
 	float angle = (0.95 - lightDir.z) * 2.0;
	if (angle == 0.0) {
		return 1.0;
	}

	// Only work near the camera.
	const float near = 750.0;
	const float far = 1000.0;
	float distance = linearstep(near, far, length(point));
	if (distance > 1.0) {
		return 1.0;
	}

	float texLod = 0;
	#if GL_ARB_texture_query_lod
	{
		const float maxMipmaps = 7;

		vec2 texSize = textureSize(tex, 0).xy;
		float numMipmaps = log2(max(texSize.x, texSize.y));

		float minMip = numMipmaps - maxMipmaps;
		float mipLevel = textureQueryLOD(tex, uv).y;

		texLod = max(mipLevel, minMip);
	}
	#endif

	float shadowLength;
	{
		const float lengthScale = 0.2;
		
		// Noisy edges are better than blocky edges.
		const float softness = 0.1;
		float pattern = fract(dot(vec2(171.0, 231.0), gl_FragCoord.xy) / 600);

		shadowLength = (PARALLAX * lengthScale) + (softness * pattern);
		shadowLength *= 1.0 - distance;
	}

	vec3 ray;
	vec3 rayDelta;
	{
		float numSamples = 8 + (16 * angle);
		numSamples *= 1.0 - distance;
		ray = vec3(uv.x, uv.y, textureLod(tex, uv, texLod).a);
		rayDelta = vec3(lightDir.xy * shadowLength, 1.0) / numSamples;
	}

	while (ray.z < 1.0 - rayDelta.z) {
		if (ray.z < textureLod(tex, ray.xy, texLod).a) {
			// return linearstep(300, 500, dist);
			return 0.0;
		}
		ray += rayDelta;
	}

	return 1.0;
}

/**
 * @brief Yield the diffuse modulation from bump-mapping.
 */
void BumpFragment(in vec3 deluxemap, in vec3 normalmap, in vec3 glossmap, out float lightmapDiffuseScale, out float lightmapSpecularScale) {
	float glossFactor = clamp(dot(glossmap, vec3(0.299, 0.587, 0.114)), 0.0078125, 1.0);

	lightmapDiffuseScale = clamp(dot(deluxemap, normalmap), 0.0, 1.0);
	lightmapSpecularScale = (HARDNESS * glossFactor) * pow(clamp(-dot(eyeDir, reflect(deluxemap, normalmap)), 0.0078125, 1.0), (16.0 * glossFactor) * SPECULAR);
}

/**
 * @brief Yield the final sample color after factoring in dynamic light sources.
 */
void LightFragment(in vec4 diffuse, in vec3 lightmap, in vec3 normalmap, in float lightmapDiffuseScale, in float lightmapSpecularScale) {

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
		float len = length(delta);

		if (len < LIGHTS.RADIUS[i]) {

			float lambert = dot(normalmap, normalize(delta));
			if (lambert > 0.0) {

				float atten = clamp(1.0 - len / LIGHTS.RADIUS[i], 0.0, 1.0);
				float atten_squared = atten * atten;

				light += LIGHTS.COLOR[i] * lambert * atten_squared;
			}
		}
	}
#endif

	// now modulate the diffuse sample with the modified lightmap
	float lightmapLuma = dot(lightmap.rgb, vec3(0.299, 0.587, 0.114));

	float blackPointLuma = 0.015625;
	float l = exp2(lightmapLuma) - blackPointLuma;
	float lightmapDiffuseBumpedLuma = l * lightmapDiffuseScale;
	float lightmapSpecularBumpedLuma = l * lightmapSpecularScale;

	vec3 diffuseLightmapColor = lightmap.rgb * lightmapDiffuseBumpedLuma;
	vec3 specularLightmapColor = (lightmapLuma + lightmap.rgb) * 0.5 * lightmapSpecularBumpedLuma;

	fragColor.rgb = diffuse.rgb * ((diffuseLightmapColor + specularLightmapColor) + light);

	// lastly modulate the alpha channel by the color
	fragColor.a = diffuse.a * color.a;
}

/**
 * @brief Render caustics
 */
void CausticFragment(in vec3 lightmap) {
	if (CAUSTIC.ENABLE) {
		vec3 model_scale = vec3(0.024, 0.024, 0.016);
		float time_scale = 0.6;
		float caustic_thickness = 0.02;
		float caustic_glow = 8.0;
		float caustic_intensity = 0.3;

		// grab raw 3d noise
		float factor = noise3d((modelpoint * model_scale) + (TIME * time_scale));

		// scale to make very close to -1.0 to 1.0 based on observational data
		factor = factor * (0.3515 * 2.0);

		// make the inner edges stronger, clamp to 0-1
		factor = clamp(pow((1 - abs(factor)) + caustic_thickness, caustic_glow), 0, 1);

		// start off with simple color * 0-1
		vec3 caustic_out = CAUSTIC.COLOR * factor * caustic_intensity;

		// multiply caustic color by lightmap, clamping so it doesn't go pure black
		caustic_out *= clamp((lightmap * 1.6) - 0.5, 0.1, 1.0);

		// add it up
		fragColor.rgb = clamp(fragColor.rgb + caustic_out, 0.0, 1.0);
	}
}

/**
 * @brief Shader entry point.
 */
void main(void) {

	eyeDir = normalize(eye);

	// texture coordinates
	vec2 uvTextures = NORMALMAP && PARALLAX != 0.0 ? BumpTexcoord() : texcoords[0];
	vec2 uvLightmap = texcoords[1];

	// flat shading
	vec3 lightmap = color.rgb;
	vec3 deluxemap = vec3(0.0, 0.0, 1.0);

	if (LIGHTMAP) {
		const float layer = max(0, DRAW_BSP_LIGHTMAPS - 1);
		lightmap =  texture(SAMPLER1, vec3(uvLightmap, layer)).rgb * MODULATE;

		if (STAINMAP) {
			vec4 stain = texture(SAMPLER8, uvLightmap);
			lightmap = mix(lightmap.rgb, stain.rgb, stain.a).rgb;
		}
	}

	// then resolve any bump mapping
	vec4 normalmap = vec4(normal, 0.5);

	float lightmapDiffuseScale = 1.0;
	float lightmapSpecularScale = 0.0;
	float lightmapSelfShadowScale = 1.0;

	if (NORMALMAP) {

		normalmap = texture(SAMPLER3, uvTextures);

		// scale by BUMP
		normalmap.xy = (normalmap.xy * 2.0 - 1.0) * BUMP;
		normalmap.xyz = normalize(normalmap.xyz);

		vec3 glossmap = vec3(0.5);

		if (GLOSSMAP) {
			glossmap = texture(SAMPLER4, uvTextures).rgb;
		} else if (DIFFUSE) {
			vec4 diffuse = texture(SAMPLER0, uvTextures);
			float processedGrayscaleDiffuse = dot(diffuse.rgb * diffuse.a, vec3(0.299, 0.587, 0.114)) * 0.875 + 0.125;
			float guessedGlossValue = clamp(pow(processedGrayscaleDiffuse * 3.0, 4.0), 0.0, 1.0) * 0.875 + 0.125;

			glossmap = vec3(guessedGlossValue);
		}

		if (DELUXEMAP) {

			// resolve the light direction and deluxemap
			vec4 deluxeColorHDR = texture(SAMPLER1, vec3(texcoords[1], 1));
			vec3 lightdir = deluxeColorHDR.rgb * deluxeColorHDR.a;

			deluxemap = normalize(2.0 * (lightdir + 0.5));

			// resolve the bumpmap diffuse and specular scales
			BumpFragment(deluxemap, normalmap.xyz, glossmap, lightmapDiffuseScale, lightmapSpecularScale);

			// and self-shadowing, if a heightmap is available
			if (PARALLAX > 0.0) {
				lightmapSelfShadowScale = SelfShadowHeightmap(deluxemap, SAMPLER3, uvTextures) * 0.5 + 0.5;
			}
		}

		// and then transform the normalmap to model space for lighting
		normalmap.xyz = normalize(
			normalmap.x * normalize(tangent) +
			normalmap.y * normalize(bitangent) +
			normalmap.z * normalize(normal)
		);
	}

	lightmapDiffuseScale *= lightmapSelfShadowScale;

	vec4 diffuse = vec4(1.0);

	if (DIFFUSE) { // sample the diffuse texture, honoring the parallax offset
		diffuse = ColorFilter(texture(SAMPLER0, uvTextures));

		// see if diffuse can be discarded because of alpha test
		if (diffuse.a < ALPHA_THRESHOLD) {
			discard;
		}

		TintFragment(diffuse, uvTextures);
	}

	// add any dynamic lighting to yield the final fragment color
	LightFragment(diffuse, lightmap, normalmap.xyz, lightmapDiffuseScale, lightmapSpecularScale);

	// underliquid caustics
	CausticFragment(lightmap);

#if DRAW_BSP_LIGHTMAPS == 0

	fragColor.rgb *= exp(fragColor.rgb);
	fragColor.rgb /= fragColor.rgb + 0.825;

#endif

	// and fog
	FogFragment(length(point), fragColor);

	// and dithering
	DitherFragment(fragColor.rgb);

	// and gamma
	GammaFragment(fragColor);
}
