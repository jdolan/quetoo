/*

Some notes on performance / quality / features:

* Fetching large textures repeatedly is slow,
  limiting miplevel or using texture compression should help.
* It's common practice to scale the effect by miplevel, but here we do it by
  distance from the camera, this decouples it from anisotropic filtering.
* Storing min/max height in the mipmaps allows hierarchical traversal,
  which should be much faster in the average case. See references below.
* Have not tried this, but jittering the height-offset at each
  pixel/iteration might mask undersampling artefacts a bit.
* Culling shadows on highly grazing angles might help a bit?
* There's probably plenty of micro-optimization opportunities left.
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

/**
 * @brief Parallax Occlusion Mapping. Expensive shader.
 * Performance is impacted by texture resolution
 * and by anisotropic filtering quality.
 */
vec2 POM(sampler2D tex, vec2 texUV, vec3 viewDir, float distance, float scale) {

	// TODO: limit by miplevel so large texture fetches won't bog perf.
	// TODO: feed proper derivatives to sampler to get rid of artefacts.

	// Attenuate scale.
	scale *= 0.05;

	// Record keeping.
	vec2  prevUV;
	vec2  currUV = texUV;
	float prevTexZ;
	float currTexZ = texture(tex, currUV).a;
	float prevHeight;
	float currHeight = 0.0;

	// Decide on the maximum number of samples to take.
	float numSamples = 20;
	// Do less at perpendicular angles.
	float angle = abs(dot(vec3(0.0, 0.0, 1.0), viewDir));
	// Do less with distance.
	float closeness = 1.0 - min(distance / 1000, 1.0);
	// Final number of steps (minimum 1).
	numSamples = (numSamples - 1) * angle * closeness + 1;

	// Height offset per step.
	float stepHeight = 1.0 / numSamples;
	// UV offset per step.
	vec2 uvDelta = viewDir.xy * scale / numSamples;

	// Raymarch.
	while (currHeight < currTexZ) {
		prevUV = currUV;
		currUV += uvDelta;
		prevTexZ = currTexZ;
		currTexZ = texture(tex, currUV).a;
		prevHeight = currHeight;
		currHeight += stepHeight;
	}

	// Take last two heights and interpolate between them.
	float a = currTexZ - currHeight;
	float b = prevTexZ - currHeight + stepHeight;
	float blend = a / (a - b);

	return mix(currUV, prevUV, blend);
}

/**
 * @brief Heightmap Hard Shadow Trace.
 * Expensive shader. Performance is impacted by texture resolution,
 * anisotropic filtering quality, sample count, and to some degree
 * by the scale of the effect.
 */
float SelfShadowHard(sampler2D tex, vec2 uv, vec3 lightDir, float distance, float scale) {

	// TODO: limit by miplevel so large texture fetches won't bog perf.

	// Decide on the maximum number of samples to take.
	float numSamples = 40;
	// Do less at perpendicular angles.
	// float angle = abs(dot(vec3(0.0, 0.0, 1.0), lightDir));
	float angle = length(lightDir.xy); // Better :)
	// Do less at distance.
	float closeness = 1.0 - min(distance / 1000, 1.0);
	// Do less at low bumpiness.
	float bumpiness = min(scale, 1.0);
	// Final sample number.
	numSamples *= bumpiness * angle * closeness;

	// Shorten the shadow cast.
	// Mostly for practical reasons, samples get spaced closer.
	scale *= 0.2;

	// Simply return white if it's not worth it.
	if (numSamples < 1.0) { return 1.0; } // Early exit.

	float step = 1.0 / numSamples;
	vec2 currUV  = uv;
	vec2 deltaUV = lightDir.xy * scale / numSamples;
	float heightSurface = texture(tex, currUV).a;
	// Bias height to avoid shadow acne.
	float heightRay = heightSurface + (step * 0.15);

	while (heightRay > heightSurface && heightRay < 1.0) {
		heightRay += step;
		currUV += deltaUV;
		heightSurface = texture(tex, currUV).a;
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
float SelfShadowSoft( sampler2D tex, vec2 uv, vec3 lightDir, float scale) {

	// Multiplier that shortens the shadow cast.
	// Mostly for practical reasons, samples get spaced closer.
	const float shorten = 0.2;

	// Decide on the maximum number of samples to take.
	// Do less at perpendicular angles.
	// Do less at distance.
	// Do less at low bumpiness.
	float numSamples = 50;
	// float angle = abs(dot(vec3(0.0, 0.0, 1.0), lightDir));
	float angle = length(lightDir.xy); // Better :)
	float closeness = 1.0 - min(distance / 1000, 1.0);
	float bumpiness = min(scale, 1.0);
	numSamples *= bumpiness * angle * closeness;

	// Setup positions and stepsizes. Start one sample out.
	// Height of the step is a fraction of the empty space above the surface.
	float heightInit = texture(tex, uv).a;
	float heightStep = (1.0 - heightInit) / numSamples;
	float heightCurr = heightInit + heightStep;
	vec2 coordsStep = lightDir.xy * (scale * shorten) / numSamples;
	vec2 coordsCurr = uv + coordsStep;

	// Raymarch upwards along the lightray.
	// Compute the partial shadowing factor.
	float shadowFactor;
	for (float i = 1; i < numSamples; i += 1) {

		float heightSurface = texture(tex, coordsCurr).a;
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
