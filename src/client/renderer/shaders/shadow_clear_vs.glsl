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

#version 450

/*
 * Emits one oversized triangle covering the whole viewport/scissor rect, with
 * no vertex buffer (indexed purely by gl_VertexIndex). Paired with
 * shadow_clear_fs.glsl to "clear" a single light's block of the shadow atlas
 * via a scissored draw -- SDL_gpu's render-pass LOADOP_CLEAR always clears the
 * whole bound depth layer, but the atlas packs multiple lights' tiles into one
 * layer, so a per-light clear can't use it without wiping every other light's
 * cached shadow too (see R_DrawShadows).
 */

void main(void) {

  const vec2 position = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);

  gl_Position = vec4(position * 2.0 - 1.0, 1.0, 1.0);
}
