#pragma once

uniform bool soft_particles;
uniform vec2 inv_viewport_size;
uniform vec2 camera_range;
uniform float transition_size;
uniform sampler2D depth_attachment;

/**
 * @brief Reverse depth calculation
 */
float calc_depth(in float z) {
	return (2. * camera_range.x) / (camera_range.y + camera_range.x - z * (camera_range.y - camera_range.x));
}

/**
 * @brief Calculate the soft edge factor for the specified fragment.
 */
float soft_edges() {

	if (!soft_particles) {
		return 1.0;
	}

	return smoothstep(0.0, transition_size, clamp(calc_depth(texture(depth_attachment, gl_FragCoord.xy * inv_viewport_size).r) - calc_depth(gl_FragCoord.z), 0.0, 1.0));
}