/**
 * @brief Phong BRDF for specular highlights.
 */
vec3 brdf_phong(vec3 view_dir, vec3 light_dir, vec3 normal,
	vec3 light_color, float specular_intensity, float specular_exponent) {

	vec3 reflection = reflect(light_dir, normal);
	float r_dot_v = max(-dot(view_dir, reflection), 0.0);
	return light_color * specular_intensity * pow(r_dot_v, 16.0 * specular_exponent);
}

/**
 * @brief Blinn-Phong BRDF for specular highlights.
 */
vec3 brdf_blinn(vec3 view_dir, vec3 light_dir, vec3 normal,
	vec3 light_color, float specular_intensity, float specular_exponent) {

	const float exponent = 16.0 * 4.0; // roughly matches phong this way
	vec3 half_angle = normalize(light_dir + view_dir);
	float n_dot_h = max(dot(normal, half_angle), 0.0);
	return light_color * specular_intensity * pow(n_dot_h, exponent * specular_exponent);
}

/**
 * @brief Lambert BRDF for diffuse lighting.
 */
vec3 brdf_lambert(vec3 light_dir, vec3 normal, vec3 light_color) {
	return light_color * max(dot(light_dir, normal), 0.0);
}

/**
 * @brief Half-Lambert BRDF for diffuse lighting.
 */
vec3 brdf_halflambert(vec3 light_dir, vec3 normal, vec3 light_color) {
	return light_color * (1.0 - (dot(normal, light_dir) * 0.5 + 0.5));
}
