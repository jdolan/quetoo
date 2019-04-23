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
 * @brief Clamps float to 0..1 range, common in HLSL.
 */
float saturate(float x){
	return clamp(x, 0.0, 1.0);
}

/**
 * @brief After lighting some fragments might contain values greater
 * than 1.0 ("HDR"). By default these get clamped, but that is ugly.
 * Tonemapping instead compresses the HDR range back to LDR smoothly.
 */
void TonemapFragment(inout vec3 rgb){

	/*

	Some reference values for S.
	Low values give bright LDR results, high values darker ones.
	The tonemapper will map left (HDR) to right (LDR).
	Arrows mark the HDR value for a 0.5 LDR value.

	if S = 0.825        |    if S = 2.7182789
	0.5000 -> 0.4998 <- |    0.5000 -> 0.2327
	1.0000 -> 0.7672    |    1.0000 -> 0.5000 <-
	1.5000 -> 0.8907    |    1.5000 -> 0.7121
	2.0000 -> 0.9471    |    2.0000 -> 0.8446
	3.0000 -> 0.9865    |    3.0000 -> 0.9568
	4.0000 -> 0.9962    |    4.0000 -> 0.9877
	5.0000 -> 0.9989    |    5.0000 -> 0.9964

	if S = 6.7225335    |    if S = 14.7781122
	0.5000 -> 0.1092    |    0.5000 -> 0.0528
	1.0000 -> 0.2879    |    1.0000 -> 0.1554
	1.5000 -> 0.5000 <- |    1.5000 -> 0.3127
	2.0000 -> 0.6873    |    2.0000 -> 0.5000 <-
	3.0000 -> 0.8996    |    3.0000 -> 0.8030
	4.0000 -> 0.9701    |    4.0000 -> 0.9366
	5.0000 -> 0.9910    |    5.0000 -> 0.9805

	*/

	const float S = 0.825;

	rgb *= exp(rgb);
	rgb /= rgb + S;
}

/**
 * @brief Cubic interpolation helper for textureBicubic().
 */
vec4 cubic(float v) {
	vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
	vec4 s = n * n * n;
	float x = s.x;
	float y = s.y - 4.0 * s.x;
	float z = s.z - 4.0 * s.y + 6.0 * s.x;
	float w = 6.0 - x - y - z;
	return vec4(x, y, z, w) * (1.0 / 6.0);
}

/**
 * @brief Bicubic interpolation of texels, smoother and more expensive than bilinear.
 */
vec4 textureBicubic(sampler2DArray sampler, vec3 coords) {

	// source: http://www.java-gaming.org/index.php?topic=35123.0

	vec2 texCoords = coords.xy;
	float layer = coords.z;

	vec2 texSize = textureSize(sampler, 0).xy;
	vec2 invTexSize = 1.0 / texSize;

	texCoords = texCoords * texSize - 0.5;

	vec2 fxy = fract(texCoords);
	texCoords -= fxy;

	vec4 xcubic = cubic(fxy.x);
	vec4 ycubic = cubic(fxy.y);

	vec4 c = texCoords.xxyy + vec2(-0.5, +1.5).xyxy;

	vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
	vec4 offset = c + vec4(xcubic.yw, ycubic.yw) / s;

	offset *= invTexSize.xxyy;

	vec4 sample0 = texture(sampler, vec3(offset.xz, layer));
	vec4 sample1 = texture(sampler, vec3(offset.yz, layer));
	vec4 sample2 = texture(sampler, vec3(offset.xw, layer));
	vec4 sample3 = texture(sampler, vec3(offset.yw, layer));

	float sx = s.x / (s.x + s.y);
	float sy = s.z / (s.z + s.w);

	return mix(mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);
}

/**
 * @brief Clamp value t to range [a,b] and map [a,b] to [0,1].
 */
float linearstep(float a, float b, float t) {
	return saturate((t - a) / (b - a));
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
 * @brief Returns greyscale from color value.
 */
float ToLuma(vec3 rgb) {
	return dot(rgb, vec3(0.299, 0.587, 0.114));
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
		float orthogonality = saturate(dot(eyeDir, vec3(0.0, 0.0, 1.0)));
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
void BumpFragment(in vec3 deluxemap, in vec3 normalmap, in vec3 glossmap, out float outDiff, out float outSpec) {

	const float glossMin = 0.0078125;
	const float glossMax = 1.0;

	float glossFactor = clamp(ToLuma(glossmap), glossMin, glossMax);

	float phongBase = -dot(eyeDir, reflect(deluxemap, normalmap));
	float phongExponent  = SPECULAR * (16.0 * glossFactor);
	float phongIntensity = HARDNESS * glossFactor;

	outDiff = saturate(dot(deluxemap, normalmap));
	outSpec = phongIntensity * pow(clamp(phongBase, glossMin, glossMax), phongExponent);
}

/**
 * @brief Yield the final sample color after factoring in dynamic light sources.
 */
void LightFragment(in vec4 diffuse, in vec3 lightmap, in vec3 normalmap, in float lmDiffScale, in float lmSpecScale) {

	vec3 light = vec3(0.0);

	#if MAX_LIGHTS
	// Iterate the hardware light sources, accumulating dynamic lighting for
	// this fragment. A light radius of 0.0 means break.

	for (int i = 0; i < MAX_LIGHTS; i++) {

		if (LIGHTS.RADIUS[i] == 0.0)
			break;

		vec3 delta = LIGHTS.ORIGIN[i] - point;
		float len = length(delta);

		if (len < LIGHTS.RADIUS[i]) {

			float lambert = dot(normalmap, normalize(delta));
			if (lambert > 0.0) {

				float atten = saturate(1.0 - (len / LIGHTS.RADIUS[i]));
				float atten_squared = atten * atten;

				light += LIGHTS.COLOR[i] * lambert * atten_squared;
			}
		}
	}
	#endif

	// now modulate the diffuse sample with the modified lightmap
	float lightmapLuma = ToLuma(lightmap.rgb);

	const float blackPointLuma = 0.015625;
	float l = exp2(lightmapLuma) - blackPointLuma;

	float lmDiffLuma = l * lmDiffScale;
	float lmSpecLuma = l * lmSpecScale;

	vec3 diffuseLightmapColor = lightmap.rgb * lmDiffLuma;
	vec3 specularLightmapColor = (0.5 * (lightmapLuma + lightmap.rgb)) * lmSpecLuma;

	fragColor.rgb = diffuse.rgb * ((diffuseLightmapColor + specularLightmapColor) + light);

	// temp gamma correction hax
//	fragColor.rgb = sqrt((diffuse.rgb * diffuse.rgb) * (diffuseLightmapColor + specularLightmapColor + light));

	// lastly modulate the alpha channel by the color
	fragColor.a = diffuse.a * color.a;
}

/**
 * @brief Render caustics
 */
void CausticFragment(in vec3 lightmap, inout vec3 rgb) {
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
		factor = saturate(pow((1 - abs(factor)) + caustic_thickness, caustic_glow));

		// start off with simple color * 0-1
		vec3 caustic_out = CAUSTIC.COLOR * factor * caustic_intensity;

		// multiply caustic color by lightmap, clamping so it doesn't go pure black
		caustic_out *= clamp((lightmap * 1.6) - 0.5, 0.1, 1.0);

		// add it up
		rgb = clamp(rgb + caustic_out, 0.0, 1.0);
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
		lightmap = textureBicubic(SAMPLER1, vec3(uvLightmap, layer)).rgb * MODULATE;

		if (STAINMAP) {
			vec4 stain = texture(SAMPLER8, uvLightmap);
			lightmap = mix(lightmap.rgb, stain.rgb, stain.a).rgb;
		}
	}

	// then resolve any bump mapping
	vec4 normalmap = vec4(normal, 0.5);

	float lmDiffScale = 1.0;
	float lmSpecScale = 0.0;
	float lmSelfShadowScale = 1.0;

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
			float processedGrayscaleDiffuse = ToLuma(diffuse.rgb * diffuse.a) * 0.875 + 0.125;
			float guessedGlossValue = saturate(pow(processedGrayscaleDiffuse * 3.0, 4.0)) * 0.875 + 0.125;

			glossmap = vec3(guessedGlossValue);
		}

		if (DELUXEMAP) {

			// resolve the light direction and deluxemap
			vec3 lightdir = texture(SAMPLER1, vec3(uvLightmap, 1.0)).rgb;

			deluxemap = normalize(lightdir * 2.0 - 1.0);

			// resolve the bumpmap diffuse and specular scales
			BumpFragment(deluxemap, normalmap.xyz, glossmap, lmDiffScale, lmSpecScale);

			// and self-shadowing, if a heightmap is available
			if (PARALLAX > 0.0) {
				lmSelfShadowScale = SelfShadowHeightmap(deluxemap, SAMPLER3, uvTextures) * 0.5 + 0.5;
			}
		}

		// and then transform the normalmap to model space for lighting
		normalmap.xyz = normalize(
			normalmap.x * normalize(tangent) +
			normalmap.y * normalize(bitangent) +
			normalmap.z * normalize(normal)
		);
	}

	lmDiffScale *= lmSelfShadowScale;

	vec4 diffuse = vec4(1.0);

	if (DIFFUSE) {
		diffuse = ColorFilter(texture(SAMPLER0, uvTextures));

		if (diffuse.a < ALPHA_THRESHOLD) {
			discard;
		}

		TintFragment(diffuse, uvTextures);
	}

	LightFragment(diffuse, lightmap, normalmap.xyz, lmDiffScale, lmSpecScale);

	CausticFragment(lightmap, fragColor.rgb);

	TonemapFragment(fragColor.rgb);

	FogFragment(length(point), fragColor);

	DitherFragment(fragColor.rgb);

	GammaFragment(fragColor);
}
