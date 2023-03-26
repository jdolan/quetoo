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
 * @brief
 */
float toksvig_gloss(in vec3 normal, in float power) {

	float len_rcp = 1.0 / saturate(length(normal));
	return 1.0 / (1.0 + power * (len_rcp - 1.0));
}

/**
 * @brief Toksvig normalmap antialiasing.
 */
float toksvig(in sampler2DArray sampler, in vec3 texcoord, in float roughness, in float specularity) {

	float power = pow(1.0 + specularity, 4.0);

	vec3 normalmap0 = (texture(sampler, texcoord, 0).xyz * 2.0 - 1.0) * vec3(vec2(roughness), 1.0);
	vec3 normalmap1 = (texture(sampler, texcoord, 1).xyz * 2.0 - 1.0) * vec3(vec2(roughness), 1.0);

	return power * min(toksvig_gloss(normalmap0, power), toksvig_gloss(normalmap1, power));
}
