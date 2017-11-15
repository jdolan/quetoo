/*

Some notes on performance / quality / features:

* Fetching large textures repeatedly is slow, using texture compression should help.
* It's common practice to scale the effect by miplevel, but here we do it by
  distance from the camera, this decouples it from anisotropic filtering.
* Most implementations trace into the surface, or middle-out.
  This scales outwards. I have found that this makes texture swimming less
  noticable because the swimming parts tend to appear as if occluded.
  It might be possible to expose another parameter which sets a reference height.

References:

* https://learnopengl.com/#!Advanced-Lighting/Parallax-Mapping
* http://sunandblackcat.com/tipFullView.php
  ?l=eng&topicid=28&topic=Parallax-Occlusion-Mapping
* http://graphics.cs.brown.edu/games/SteepParallax/index.html
* https://www.gamedev.net/articles/programming/graphics
  /a-closer-look-at-parallax-occlusion-mapping-r3262
* Michal Drobot, Quadtree Displacement Mapping With Height Blending,
  http://gamedevs.org/uploads/
  quadtree-displacement-mapping-with-height-blending.pdf
* Natalya Tatarchuk
  https://developer.amd.com/wordpress/media/2012/10/
  Tatarchuk-ParallaxOcclusionMapping-FINAL_Print.pdf
* Art Tevs, Ivo Ihrke, Hans-Peter Seidel,
  Maximum Mipmaps for Fast, Accurate, and Scalable Dynamic Height Field Rendering,
  http://tevs.eu/files/i3d08_talk.pdf
* Ermin Hrkalovic, Mikael Lundgren,
  Review of Displacement Mapping Techniques and Optimization
  http://www.diva-portal.org/smash/get/diva2:831762/FULLTEXT01.pdf
* Terry Welsh, Parallax Mapping with Offset Limiting
  http://exibeo.net/docs/parallax_mapping.pdf
* Ryan Brucks, Epic Games, video walktrough of the algorithm.
  https://www.youtube.com/watch?v=4gBAOB7b5Mg

*/

/* textureLod supposedly is faster than textureGrad.
vec2 pdx = dPdx(uv_materials);
vec2 pdy = dPdy(uv_materials);
float heightTexture(sampler2D tex, vec2 uv) {
	return textureGrad(tex, uv, pdx, pdy).a;
}
*/

#extension GL_ARB_texture_query_lod : require

float heightTexture(sampler2D tex, vec2 uv) {
	// Limit texture lod to 128*128
	float lod = textureQueryLOD(tex, uv).x;
	lod = min(lod, 7);
	return textureLod(tex, uv, lod).a;
}

/**
 * @brief Cheap parallax shader.
 */
vec2 BumpOffset(sampler2D tex, vec2 uv, vec3 viewDir, float scale) {
	vec2 offset = viewDir.xy * heightTexture(tex, uv) * 0.02 * scale + uv;
	// Offset limiting.
	return mix (uv, offset, dot(vec3(0.0, 0.0, 1.0), normalize(viewDir)));
}

/**
 * @brief Parallax Occlusion Mapping. Expensive shader.
 * Performance is impacted by texture resolution
 * and by anisotropic filtering quality.
 */
vec2 POM(sampler2D tex, vec2 uv,
	vec3 viewDir, float distance, float scale) {

	// Decide how much samples to take for this pixel.
	// Clamp the effect to a range around the view
	// and do less at perpendicular angles.
	const float maxSamples = 16;
	const float minSamples = 2;
	const float near = 100;
	const float far = 500;
	float falloff = 1.0 - (clamp(distance, near, far) - near) / (far - near);
	if (falloff == 0.0) { return uv; } // Early exit.
	float angle = 1.0 - dot(vec3(0.0, 0.0, 1.0), normalize(viewDir));
	angle = angle * 0.75 + 0.25; // Temper a bit.
	float numSamples = mix(minSamples, maxSamples, min(angle, falloff));

	// Attenuate scale with distance and scale to something sane.
	scale = scale * falloff * 0.05;

	// Record keeping.
	vec2  prevUV;
	vec2  currUV = uv;
	float surfaceHeightPrev;
	float surfaceHeightCurr = heightTexture(tex, currUV);
	float rayHeightPrev;
	float rayHeightCurr = 0.0;

	// Height offset per step.
	float stepHeight = 1.0 / numSamples;
	// UV offset per step.
	vec2 uvDelta = viewDir.xy * scale / numSamples;

	while (rayHeightCurr < surfaceHeightCurr) {
		prevUV = currUV;
		currUV += uvDelta;
		surfaceHeightPrev = surfaceHeightCurr;
		surfaceHeightCurr = heightTexture(tex, currUV);
		rayHeightPrev = rayHeightCurr;
		rayHeightCurr += stepHeight;
	}

	// Take last two heights and interpolate between them.
	float a = surfaceHeightCurr - rayHeightCurr;
	float b = surfaceHeightPrev - rayHeightCurr + stepHeight;
	float blend = a / (a - b);

	return mix(currUV, prevUV, blend);
}

/**
 * @brief Heightmap Hard Shadow Trace.
 * Expensive shader. Performance is impacted by texture resolution,
 * anisotropic filtering quality, sample count, and to some degree
 * by the scale of the effect.
 */
float SelfShadowHard(sampler2D tex, vec2 uv, vec3 lightDir,
	float distance, float scale) {

	// Shorten the shadow cast.
	// Mostly for practical reasons, samples get spaced closer.
	const float shorten = 0.2;
	
	// Decide on the maximum number of samples to take.
	float numSamples = 40;
	// Do less at perpendicular angles.
	float angle = length(lightDir.xy);
	// Do less at distance.
	float closeness = 1.0 - min(distance / 1000, 1.0);
	// Do less at low bumpiness.
	float bumpiness = min(scale, 1.0);
	// Final sample number.
	numSamples *= bumpiness * angle * closeness;

	// Simply return white if it's not worth it.
	if (numSamples < 1.0) { return 1.0; } // Early exit.

	float step = 1.0 / numSamples;
	vec2 currUV  = uv;
	vec2 deltaUV = lightDir.xy * (scale * shorten) / numSamples;
	float heightSurface = heightTexture(tex, currUV);
	// Bias height to avoid shadow acne.
	float heightRay = heightSurface + (step * 0.15);

	while (heightRay > heightSurface && heightRay < 1.0) {
		heightRay += step;
		currUV += deltaUV;
		heightSurface = heightTexture(tex, currUV);
	}

	// Are we in shadow?
	return heightSurface > heightRay ? 0.0 : 1.0;
}

/**
 * @brief Heightmap Soft Shadow Trace.
 * Expensive shader. Performance is impacted by texture resolution,
 * anisotropic filtering quality, sample count, and to some degree
 * by the scale of the effect. Much more expensive than SelfShadowHard().
 */
float SelfShadowSoft(sampler2D tex, vec2 uv, vec3 lightDir,
	float distance, float scale) {

	// Multiplier that shortens the shadow cast.
	// Mostly for practical reasons, samples get spaced closer.
	const float shorten = 0.2;

	// Decide on the maximum number of samples to take.
	float numSamples = 50;
	// Do less at perpendicular angles.
	float angle = length(lightDir.xy); // Better :)
	// Do less at distance.
	float closeness = 1.0 - min(distance / 1000, 1.0);
	// Do less at low bumpiness.
	float bumpiness = min(scale, 1.0);
	// float angle = abs(dot(vec3(0.0, 0.0, 1.0), lightDir));
	numSamples *= bumpiness * angle * closeness;

	// Setup positions and stepsizes. Start one sample out.
	// Height of the step is a fraction of the empty space above the surface.
	float heightInit = heightTexture(tex, uv);
	float heightStep = (1.0 - heightInit) / numSamples;
	float heightCurr = heightInit + heightStep;
	vec2 coordsStep = lightDir.xy * (scale * shorten) / numSamples;
	vec2 coordsCurr = uv + coordsStep;

	// Raymarch upwards along the lightray.
	// Compute the partial shadowing factor.
	float shadowFactor;
	for (float i = 1; i < numSamples; i += 1) {

		float heightSurface = heightTexture(tex, coordsCurr);
		// If we intersect the heightfield.
		if (heightSurface > heightCurr) {
			// Compute blocker-to-receiver formula.
			float new = (heightSurface - heightCurr) * (1.0 - i / numSamples);
			shadowFactor = max(shadowFactor, new);
		}

		heightCurr += heightStep;
		coordsCurr += coordsStep;
	}

	// Massage result a bit.
	// Invert so shadowFactor becomes lightFactor.
	shadowFactor = 1.0 - shadowFactor * 4.0;
	// Harden edges, comment this out if you want a softer look.
	shadowFactor *= smoothstep(0.75, 0.85, shadowFactor);

	return shadowFactor;
}
