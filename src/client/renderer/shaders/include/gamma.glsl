// source: http://chilliant.blogspot.be/2012/08/srgb-approximations-for-hlsl.html

vec3 pow3(vec3 base, float exp) {
	return vec3(pow(base.x, exp), pow(base.y, exp), pow(base.z, exp));
}

vec3 toGamma3(in vec3 linear) {
	return max(1.055f * pow3(linear, 0.416666667f) - 0.055f, 0);
}

vec3 toLinear3(in vec3 sRGB) {
	return 0.012522878f * sRGB +
	       0.682171111f * sRGB * sRGB +
	       0.305306011f * sRGB * sRGB * sRGB;
}

float toGamma(in float linear) {
	return max(1.055f * pow(linear, 0.416666667f) - 0.055f, 0);
}

float toLinear(in float sRGB) {
	return 0.012522878f * sRGB +
	       0.682171111f * sRGB * sRGB +
	       0.305306011f * sRGB * sRGB * sRGB;
}
