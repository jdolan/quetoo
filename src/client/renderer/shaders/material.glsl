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

#define STAGE_TEXTURE      0
#define STAGE_BLEND        4
#define STAGE_COLOR        8
#define STAGE_PULSE        16
#define STAGE_STRETCH      32
#define STAGE_ROTATE       64
#define STAGE_SCROLL_S     128
#define STAGE_SCROLL_T     256
#define STAGE_SCALE_S      512
#define STAGE_SCALE_T      1024
#define STAGE_TERRAIN      2048
#define STAGE_ANIM         4096
#define STAGE_LIGHTMAP     8192
#define STAGE_DIRTMAP      16384
#define STAGE_ENVMAP       32768
#define STAGE_WARP         65536
#define STAGE_FLARE        131072
#define STAGE_FOG          262144
#define STAGE_SHELL        524288

#define STAGE_DRAW         268435456
#define STAGE_MATERIAL     536870912

const float PI = 3.141592653589793115997963468544185161590576171875;
const float TWO_PI = PI * 2.0;

const float DIRTMAP[8] = float[](0.125, 0.250, 0.375, 0.500, 0.625, 0.750, 0.875, 1.000);

/**
 * @brief The material type.
 */
struct material_t {
	/**
	 * @brief The material alpha test threshold.
	 */
	float alpha_test;

	/**
	 * @brief The material roughness.
	 */
	float roughness;

	/**
	 * @brief The material hardness.
	 */
	float hardness;

	/**
	 * @brief The material specularity.
	 */
	float specularity;

	/**
	 * @brief The material parallax.
	 */
	float parallax;

	/**
	 * @brief The material bloom.
	 */
	float bloom;
};

uniform material_t material;

/**
 * @brief The stage type.
 */
struct stage_t {
	/**
	 * @brief The stage flags.
	 */
	int flags;

	/**
	 * @brief The stage color.
	 */
	vec4 color;

	/**
	 * @brief The stage alpha pulse.
	 */
	float pulse;

	/**
	 * @brief The stage texture coordinate origin for rotations and stretches.
	 */
	vec2 st_origin;

	/**
	 * @brief The stage stretch amplitude and frequency.
	 */
	vec2 stretch;

	/**
	 * @brief The stage rotation rate in radians per second.
	 */
	float rotate;

	/**
	 * @brief The stage scroll rate.
	 */
	vec2 scroll;

	/**
	 * @brief The stage scale factor.
	 */
	vec2 scale;

	/**
	 * @brief The stage terrain blending floor and ceiling.
	 */
	vec2 terrain;

	/**
	 * @brief The stage dirtmap intensity.
	 */
	float dirtmap;

	/**
	 * @brief The stage warp rate and intensity.
	 */
	vec2 warp;

	/**
	 * @brief The stage shell radius.
	 */
	float shell;
};

uniform stage_t stage;

/**
 * @brief
 */
void stage_transform(in stage_t stage, inout vec3 position, inout vec3 normal, inout vec3 tangent, inout vec3 bitangent) {

	if ((stage.flags & STAGE_SHELL) == STAGE_SHELL) {
		position += normal * stage.shell;
	}
}

/**
 * @brief
 */
void stage_vertex(in stage_t stage, in vec3 in_position, in vec3 position, inout vec2 diffusemap, inout vec4 color) {

	if ((stage.flags & STAGE_STRETCH) == STAGE_STRETCH) {
		float p = 1.0 + sin(ticks * .001 * stage.stretch.y * PI) * stage.stretch.x * .5;

		mat2 matrix;
		vec2 translate;
		matrix[0][0] = p;
		matrix[1][0] = 0;
		translate[0] = stage.st_origin.x - stage.st_origin.x * p;

		matrix[0][1] = 0;
		matrix[1][1] = p;
		translate[1] = stage.st_origin.y - stage.st_origin.y * p;

		diffusemap[0] = diffusemap[0] * matrix[0][0] + diffusemap[1] * matrix[1][0] + translate[0];
		diffusemap[1] = diffusemap[0] * matrix[0][1] + diffusemap[1] * matrix[1][1] + translate[1];
	}

	if ((stage.flags & STAGE_ROTATE) == STAGE_ROTATE) {
		float theta = ticks * 0.001 * stage.rotate * TWO_PI;

		diffusemap = diffusemap - stage.st_origin;
		diffusemap = mat2(cos(theta), -sin(theta), sin(theta),  cos(theta)) * diffusemap;
		diffusemap = diffusemap + stage.st_origin;
	}

	if ((stage.flags & STAGE_SCROLL_S) == STAGE_SCROLL_S) {
		diffusemap.s += stage.scroll.s * ticks * 0.001;
	}

	if ((stage.flags & STAGE_SCROLL_T) == STAGE_SCROLL_T) {
		diffusemap.t += stage.scroll.t * ticks * 0.001;
	}

	if ((stage.flags & STAGE_SCALE_S) == STAGE_SCALE_S) {
		diffusemap.s *= stage.scale.s;
	}

	if ((stage.flags & STAGE_SCALE_T) == STAGE_SCALE_T) {
		diffusemap.t *= stage.scale.t;
	}

	if ((stage.flags & STAGE_ENVMAP) == STAGE_ENVMAP) {
		diffusemap *= normalize(position).xy;
	}

	if ((stage.flags & STAGE_COLOR) == STAGE_COLOR) {
		color *= stage.color;
	}

	if ((stage.flags & STAGE_PULSE) == STAGE_PULSE) {
		color.a *= (sin(ticks * .001 * stage.pulse * PI) + 1.0) * .5;
	}

	if ((stage.flags & STAGE_TERRAIN) == STAGE_TERRAIN) {
		float z = clamp(in_position.z, stage.terrain.x, stage.terrain.y);
		color.a *= (z - stage.terrain.x) / (stage.terrain.y - stage.terrain.x);
	}

	if ((stage.flags & STAGE_DIRTMAP) == STAGE_DIRTMAP) {
		int index = int(in_position.x) + int(in_position.y) + int(in_position.z);
		color.a *= DIRTMAP[index % DIRTMAP.length()] * stage.dirtmap;
	}
}
