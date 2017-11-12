/**
 * @brief Phong BRDF for specular highlights.
 */
vec3 Phong(vec3 viewDir, vec3 lightDir, vec3 normal,
	vec3 lightColor, float specIntensity, float specPower) {
	
	vec3 reflection = reflect(lightDir, normal);
	float RdotV = max(-dot(viewDir, reflection), 0.0);
	return lightColor * specIntensity * pow(RdotV, 16.0 * specPower);
}

/**
 * @brief Blinn-Phong BRDF for specular highlights.
 */
vec3 Blinn(vec3 viewDir, vec3 lightDir, vec3 normal,
	vec3 lightColor, float specIntensity, float specPower) {

	const float exponent = 16.0 * 4.0; // try to roughly match Phong highlight.
	vec3 halfAngle = normalize(lightDir + viewDir);
	float NdotH = max(dot(normal, halfAngle), 0.0);
	return lightColor * specIntensity * pow(NdotH, exponent * specPower);
}

/**
 * @brief Lambert BRDF for diffuse lighting.
 */
vec3 Lambert(vec3 lightDir, vec3 normal, vec3 lightColor) {
	return lightColor * max(dot(lightDir, normal), 0.0);
}

/**
 * @brief Lambert BRDF with fill light for diffuse lighting.
 */
vec3 LambertFill(vec3 lightDir, vec3 normal,
	float fillStrength, vec3 lightColor) {

	// Base
	float NdotL = dot(normal, lightDir);
	// Half-Lambert Soft Lighting
	float fill = 1.0 - (NdotL * 0.5 + 0.5);
	// Lambert Hard Lighting
	float direct = max(NdotL, 0.0);
	// Hard + Soft Lighting
	return lightColor * (fill * fillStrength + direct);
}
