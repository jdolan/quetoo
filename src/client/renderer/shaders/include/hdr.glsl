// ACES color system default tonemaper.
vec3 tonemap_aces(vec3 x) {

	const float a = 2.51f;
	const float b = 0.03f;
	const float c = 2.43f;
	const float d = 0.59f;
	const float e = 0.14f;

	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);	
}

// Rather steep tonemapper to work with LDR authored maps.
vec3 tonemap_ldr(vec3 color) {
	// Make base more straight in final curve.
	color *= exp(color);
	// Gamma curve, 0.825 makes it pass through 0.5 in x and y.
	return color / (color + 0.825);
}

// Simplest Reinhard tonemapper.
vec3 tonemap_reinhard(vec3 x, float shallowness) {
	return x / (x + shallowness);
}

// Crosstalk makes HDR highlights tend towards white.
// This appears more realistic and maintains definition in overexposed areas.
// Needs to be applied on the HDR value before the tonemapping stage.
vec3 crosstalk(vec3 color, float treshold, float shallowness) {
	// Luminosity
	float luma = dot(color, vec3(0.299, 0.587, 0.114));
	// Blending Factor (sigmoid curve)
	float blend = max(luma - treshold, 0.0);
	      blend *= blend;
	      blend /= blend + shallowness;
	// Mix RGB with grayscale luminosity to simulate crosstalk.
	return mix(color, vec3(luma), blend);
}
