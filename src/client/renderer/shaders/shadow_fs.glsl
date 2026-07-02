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
 * Depth-only point-light shadow pass: writes linear radial distance-from-light
 * (normalized by depth_range, plus a small bias) into a D16 depth atlas layer.
 * The lighting pass samples this atlas through a comparison sampler, comparing
 * the fragment's radial distance against the stored nearest-occluder distance.
 */

layout (location = 0) in float in_dist;

void main(void) {

  gl_FragDepth = min(in_dist + 0.0006, 1.0);
}
