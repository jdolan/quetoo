/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/**
 * @return A Quake plane (normal.xyz, dist) from the given coplanar points.
 */
vec4 plane_from_points(in vec3 a, in vec3 b, in vec3 c) {

	vec3 normal = normalize(cross(a - b, c - b));
	float dist = dot(a, normal);

	return vec4(normal, dist);
}

/**
 * @return The distance from `p` to `plane`.
 */
float distance_to_plane(in vec4 plane, in vec3 p) {
	return dot(p, plane.xyz) - plane.w;
}

/**
 * @return The distance from `p` to the line segment `ba`.
 * @see https://stackoverflow.com/questions/849211/shortest-distance-between-a-point-and-a-line-segment
 */
float distance_to_line(in vec3 a, in vec3 b, in vec3 p) {

	float dist_squared = dot(b - a, b - a);

	float fraction = clamp(dot(p - a, b - a) / dist_squared, 0.0, 1.0);
	return distance(p, a + fraction * (b - a));
}

/**
 * @return The distance from `p` to the triangle `abc`.
 */
float distance_to_triangle(in vec3 a, in vec3 b, in vec3 c, in vec3 p) {
	return min(abs(distance_to_line(a, b, p)),
		   min(abs(distance_to_line(b, c, p)),
			   abs(distance_to_line(c, a, p))));
}

/**
 * @return The area of the triangle `abc`.
 */
float triangle_area(in vec3 a, in vec3 b, in vec3 c) {
	return length(cross(b - a, c - a)) * 0.5;
}

/**
 * @return True if `p` resides within the triangle `abc`, false otherwise.
 */
bool barycentric(in vec3 a, in vec3 b, in vec3 c, in vec3 p) {

	float abc = triangle_area(a, b, c);

	vec3 bary = vec3(triangle_area(b, c, p),
					 triangle_area(c, a, p),
					 triangle_area(a, b, p)) / abc;

	if (bary.x < 0.0 || bary.x > 1.0 ||
		bary.y < 0.0 || bary.y > 1.0 ||
		bary.z < 0.0 || bary.z > 1.0) {
		return false;
	}

	return true;
}

/**
 * @brief The AABB normal vectors.
 */
vec3[] box_normals = vec3[](
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0));

/**
 * @brief Separating axis test.
 * @return True if abc and extents are separated by axis.
 */
bool sat(in vec3 a, in vec3 b, in vec3 c, vec3 extents, in vec3 axis) {

	vec3 p = vec3(dot(a, axis), dot(b, axis), dot(c, axis));

	float r = extents.x * abs(dot(box_normals[0], axis)) +
			  extents.y * abs(dot(box_normals[1], axis)) +
			  extents.z * abs(dot(box_normals[2], axis));

	if (max(-hmax(p), hmin(p)) > r) {
		return true; // separating axis
	}

	return false;
}

/**
 * @return True if the triangle abc intersects the bounding box.
 * @see https://gdbooks.gitbooks.io/3dcollisions/content/Chapter4/aabb-triangle.html
 */
bool triangle_intersects(in vec3 a, in vec3 b, in vec3 c, in vec3 mins, in vec3 maxs) {

	vec3 center = mix(mins, maxs, 0.5);
	vec3 extents = maxs - center;

	a -= center;
	b -= center;
	c -= center;

	vec3[] tri_edges = vec3[](b - a, c - b, a - c);

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			vec3 axis = cross(box_normals[i], tri_edges[j]);
			if (sat(a, b, c, extents, axis)) {
				return false;
			}
		}
	}

	for (int i = 0; i < 3; i++) {
		if (sat(a, b, c, extents, box_normals[i])) {
			return false;
		}
	}

	vec3 tri_normal = cross(b - a, c - b);
	if (sat(a, b, c, extents, tri_normal)) {
		return false;
	}

	return true;
}
