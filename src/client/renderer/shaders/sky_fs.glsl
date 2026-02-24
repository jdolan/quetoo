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

in common_vertex_t vertex;
in vec3 cubemap_coord;

out vec4 out_color;

/**
 * @brief
 */
void main(void) {

  if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {

    out_color = texture(texture_sky, normalize(cubemap_coord));

    // Use vertex fog (sky is simple geometry)
    out_color.rgb = mix(out_color.rgb, vertex.fog.rgb, vertex.fog.a);

  } else {

    vec2 st = vertex.diffusemap;

    if ((stage.flags & STAGE_WARP) == STAGE_WARP) {
      st += texture(texture_warp, st + vec2(ticks * stage.warp.x * 0.000125)).xy * stage.warp.y;
    }

    out_color = sample_material_stage(st) * vertex.color;
  }
}
